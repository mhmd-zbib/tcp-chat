#include "../include/room_manager.h"
#include "../../utils/include/logger.h"
#include "../include/client_manager.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Room manager implementation following SOLID principles

void room_manager_initialize(room_manager_t *manager)
{
    if (!manager) {
        LOG_ERROR("room_manager_initialize: manager is NULL");
        return;
    }

    manager->room_count = 0;
    pthread_mutex_init(&manager->global_mutex, NULL);

    for (int i = 0; i < MAX_ROOMS; i++) {
        room_t *room = &manager->rooms[i];
        memset(room->room_id, 0, sizeof(room->room_id));
        memset(room->room_name, 0, sizeof(room->room_name));
        room->client_count = 0;
        room->created_at   = 0;
        room->is_active    = 0;
        pthread_mutex_init(&room->mutex, NULL);

        for (int j = 0; j < MAX_CLIENTS_PER_ROOM; j++) {
            room->client_ids[j] = -1;
        }
    }

    LOG_INFO("Room manager initialized");
}

void room_manager_cleanup(room_manager_t *manager)
{
    if (!manager) {
        return;
    }

    LOG_INFO("Cleaning up room manager");
    pthread_mutex_lock(&manager->global_mutex);

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (manager->rooms[i].is_active) {
            room_cleanup(&manager->rooms[i]);
        }
    }

    pthread_mutex_unlock(&manager->global_mutex);
    pthread_mutex_destroy(&manager->global_mutex);
    LOG_INFO("Room manager cleanup complete");
}

char *room_manager_create_room(room_manager_t *manager, const char *room_name,
                               int creator_client_id)
{
    if (!manager || !room_name) {
        LOG_ERROR("room_manager_create_room: Invalid parameters");
        return NULL;
    }

    pthread_mutex_lock(&manager->global_mutex);

    if (manager->room_count >= MAX_ROOMS) {
        LOG_WARN("room_manager_create_room: Maximum rooms reached");
        pthread_mutex_unlock(&manager->global_mutex);
        return NULL;
    }

    room_t *room = NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!manager->rooms[i].is_active) {
            room = &manager->rooms[i];
            break;
        }
    }

    if (!room) {
        LOG_ERROR("room_manager_create_room: No available room slots");
        pthread_mutex_unlock(&manager->global_mutex);
        return NULL;
    }

    // Generate unique room ID
    char *room_id = room_generate_id();
    if (!room_id) {
        LOG_ERROR("room_manager_create_room: Failed to generate room ID");
        pthread_mutex_unlock(&manager->global_mutex);
        return NULL;
    }

    room_initialize(room, room_id, room_name);
    room_add_client(room, creator_client_id);
    manager->room_count++;

    LOG_INFO("room_manager_create_room: Created room '%s' with ID '%s'", room_name, room_id);
    pthread_mutex_unlock(&manager->global_mutex);

    return strdup(room_id);
}

int room_manager_join_room(room_manager_t *manager, const char *room_id, int client_id)
{
    if (!manager || !room_id) {
        LOG_ERROR("room_manager_join_room: Invalid parameters");
        return -1;
    }

    pthread_mutex_lock(&manager->global_mutex);
    room_t *room = room_manager_find_room(manager, room_id);

    if (!room) {
        LOG_WARN("room_manager_join_room: Room '%s' not found", room_id);
        pthread_mutex_unlock(&manager->global_mutex);
        return -1;
    }

    int result = room_add_client(room, client_id);
    pthread_mutex_unlock(&manager->global_mutex);

    if (result == 0) {
        LOG_INFO("room_manager_join_room: Client %d joined room '%s'", client_id, room_id);
    } else {
        LOG_WARN("room_manager_join_room: Failed to add client %d to room '%s'", client_id,
                 room_id);
    }

    return result;
}

int room_manager_leave_room(room_manager_t *manager, const char *room_id, int client_id)
{
    if (!manager || !room_id) {
        LOG_ERROR("room_manager_leave_room: Invalid parameters");
        return -1;
    }

    pthread_mutex_lock(&manager->global_mutex);
    room_t *room = room_manager_find_room(manager, room_id);

    if (!room) {
        LOG_WARN("room_manager_leave_room: Room '%s' not found", room_id);
        pthread_mutex_unlock(&manager->global_mutex);
        return -1;
    }

    int result = room_remove_client(room, client_id);

    // If room is empty, deactivate it
    if (room->client_count == 0) {
        room_cleanup(room);
        manager->room_count--;
        LOG_INFO("room_manager_leave_room: Room '%s' closed (empty)", room_id);
    }

    pthread_mutex_unlock(&manager->global_mutex);

    if (result == 0) {
        LOG_INFO("room_manager_leave_room: Client %d left room '%s'", client_id, room_id);
    }

    return result;
}

int room_manager_broadcast_to_room(room_manager_t *manager, const char *room_id,
                                   const char *message, int sender_id)
{
    if (!manager || !room_id || !message) {
        LOG_ERROR("room_manager_broadcast_to_room: Invalid parameters");
        return -1;
    }

    pthread_mutex_lock(&manager->global_mutex);
    room_t *room = room_manager_find_room(manager, room_id);

    if (!room) {
        LOG_WARN("room_manager_broadcast_to_room: Room '%s' not found", room_id);
        pthread_mutex_unlock(&manager->global_mutex);
        return -1;
    }

    pthread_mutex_lock(&room->mutex);
    int sent_count = 0;

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        int client_id = room->client_ids[i];
        if (client_id != -1 && client_id != sender_id) {
            // Use external client manager to send message
            extern client_manager_t *g_client_manager;
            if (g_client_manager) {
                client_session_t *session = client_manager_get_session(g_client_manager, client_id);
                if (session && session->active) {
                    if (send(session->socket_fd, message, strlen(message), MSG_NOSIGNAL) >= 0) {
                        sent_count++;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&room->mutex);
    pthread_mutex_unlock(&manager->global_mutex);

    LOG_DEBUG("room_manager_broadcast_to_room: Sent message to %d clients in room '%s'", sent_count,
              room_id);
    return sent_count;
}

room_t *room_manager_find_room(room_manager_t *manager, const char *room_id)
{
    if (!manager || !room_id) {
        return NULL;
    }

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (manager->rooms[i].is_active && strcmp(manager->rooms[i].room_id, room_id) == 0) {
            return &manager->rooms[i];
        }
    }

    return NULL;
}

void room_manager_list_rooms(room_manager_t *manager, char *buffer, size_t buffer_size)
{
    if (!manager || !buffer || buffer_size == 0) {
        return;
    }

    pthread_mutex_lock(&manager->global_mutex);

    snprintf(buffer, buffer_size, "=== Active Rooms ===\n");

    if (manager->room_count == 0) {
        strncat(buffer, "No active rooms\n", buffer_size - strlen(buffer) - 1);
    } else {
        for (int i = 0; i < MAX_ROOMS && strlen(buffer) < buffer_size - 100; i++) {
            room_t *room = &manager->rooms[i];
            if (room->is_active) {
                char room_info[128];
                snprintf(room_info, sizeof(room_info), "ID: %s | Name: %s | Users: %d\n",
                         room->room_id, room->room_name, room->client_count);
                strncat(buffer, room_info, buffer_size - strlen(buffer) - 1);
            }
        }
    }

    strncat(buffer, "==================\n", buffer_size - strlen(buffer) - 1);
    pthread_mutex_unlock(&manager->global_mutex);
}

void room_manager_get_room_users(room_manager_t *manager, const char *room_id, char *buffer,
                                 size_t buffer_size)
{
    if (!manager || !room_id || !buffer || buffer_size == 0) {
        return;
    }

    pthread_mutex_lock(&manager->global_mutex);
    room_t *room = room_manager_find_room(manager, room_id);

    if (!room) {
        snprintf(buffer, buffer_size, "Room not found\n");
        pthread_mutex_unlock(&manager->global_mutex);
        return;
    }

    snprintf(buffer, buffer_size, "=== Users in Room %s ===\n", room->room_name);

    extern client_manager_t *g_client_manager;
    if (g_client_manager) {
        for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
            int client_id = room->client_ids[i];
            if (client_id != -1) {
                client_session_t *session = client_manager_get_session(g_client_manager, client_id);
                if (session && session->active) {
                    char user_info[64];
                    snprintf(user_info, sizeof(user_info), "- %s\n", session->nickname);
                    strncat(buffer, user_info, buffer_size - strlen(buffer) - 1);
                }
            }
        }
    }

    strncat(buffer, "======================\n", buffer_size - strlen(buffer) - 1);
    pthread_mutex_unlock(&manager->global_mutex);
}

// Room-specific functions
void room_initialize(room_t *room, const char *room_id, const char *room_name)
{
    if (!room || !room_id || !room_name) {
        LOG_ERROR("room_initialize: Invalid parameters");
        return;
    }

    strncpy(room->room_id, room_id, ROOM_ID_LENGTH);
    room->room_id[ROOM_ID_LENGTH] = '\0';

    strncpy(room->room_name, room_name, MAX_ROOM_NAME - 1);
    room->room_name[MAX_ROOM_NAME - 1] = '\0';

    room->client_count = 0;
    room->created_at   = time(NULL);
    room->is_active    = 1;

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        room->client_ids[i] = -1;
    }

    LOG_DEBUG("room_initialize: Initialized room '%s' with ID '%s'", room_name, room_id);
}

void room_cleanup(room_t *room)
{
    if (!room) {
        return;
    }

    memset(room->room_id, 0, sizeof(room->room_id));
    memset(room->room_name, 0, sizeof(room->room_name));
    room->client_count = 0;
    room->created_at   = 0;
    room->is_active    = 0;

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        room->client_ids[i] = -1;
    }
}

int room_add_client(room_t *room, int client_id)
{
    if (!room || client_id < 0) {
        LOG_ERROR("room_add_client: Invalid parameters");
        return -1;
    }

    pthread_mutex_lock(&room->mutex);

    if (room_has_client(room, client_id)) {
        LOG_WARN("room_add_client: Client %d already in room", client_id);
        pthread_mutex_unlock(&room->mutex);
        return -1;
    }

    if (room->client_count >= MAX_CLIENTS_PER_ROOM) {
        LOG_WARN("room_add_client: Room is full");
        pthread_mutex_unlock(&room->mutex);
        return -1;
    }

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        if (room->client_ids[i] == -1) {
            room->client_ids[i] = client_id;
            room->client_count++;
            pthread_mutex_unlock(&room->mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&room->mutex);
    return -1;
}

int room_remove_client(room_t *room, int client_id)
{
    if (!room || client_id < 0) {
        LOG_ERROR("room_remove_client: Invalid parameters");
        return -1;
    }

    pthread_mutex_lock(&room->mutex);

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        if (room->client_ids[i] == client_id) {
            room->client_ids[i] = -1;
            room->client_count--;
            pthread_mutex_unlock(&room->mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&room->mutex);
    return -1;
}

int room_has_client(room_t *room, int client_id)
{
    if (!room || client_id < 0) {
        return 0;
    }

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        if (room->client_ids[i] == client_id) {
            return 1;
        }
    }

    return 0;
}

char *room_generate_id(void)
{
    static char room_id[ROOM_ID_LENGTH + 1];
    const char  charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    srand(time(NULL) + getpid());

    for (int i = 0; i < ROOM_ID_LENGTH; i++) {
        room_id[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    room_id[ROOM_ID_LENGTH] = '\0';

    return room_id;
}
