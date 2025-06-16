#include "../include/client_manager.h"
#include "../include/client_session.h"
#include "../include/types.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
static client_manager_t *g_manager = NULL;
void client_manager_initialize(client_manager_t *manager)
{
    if (!manager) {
        fprintf(stderr, "client_manager_initialize: manager is NULL\n");
        return;
    }
    manager->client_count = 0;
    pthread_mutex_init(&manager->mutex, NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_session_t *session = &manager->clients[i];
        session->active           = 0;
        session->socket_fd        = -1;
        memset(session->nickname, 0, MAX_NICKNAME_LEN);
        session->handshake_state = HANDSHAKE_IDLE;
        session->sequence_number = 0;
    }
    g_manager = manager;
    printf("client_manager_initialize: Manager initialized\n");
}
void client_manager_cleanup(client_manager_t *manager)
{
    if (!manager) {
        return;
    }
    printf("client_manager_cleanup: Cleaning up manager\n");
    pthread_mutex_lock(&manager->mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (manager->clients[i].active) {
            client_session_cleanup(&manager->clients[i]);
            pthread_cancel(manager->clients[i].thread);
        }
    }
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    g_manager = NULL;
    printf("client_manager_cleanup: Manager cleanup complete\n");
}
int client_manager_add_new_client(client_manager_t *manager, int socket_fd, struct sockaddr_in addr)
{
    if (!manager) {
        fprintf(stderr, "client_manager_add_new_client: manager is NULL\n");
        return -1;
    }
    pthread_mutex_lock(&manager->mutex);
    int client_id = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!manager->clients[i].active) {
            client_id = i;
            client_session_initialize(&manager->clients[i], socket_fd, addr);
            manager->client_count++;
            break;
        }
    }
    pthread_mutex_unlock(&manager->mutex);
    if (client_id != -1) {
        printf("client_manager_add_new_client: New client connected: %s:%d (ID: %d)\n",
               inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client_id);
    } else {
        printf("client_manager_add_new_client: Failed to add client - server full\n");
    }
    return client_id;
}
void client_manager_remove_client(client_manager_t *manager, int client_id)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return;
    }
    pthread_mutex_lock(&manager->mutex);
    client_session_t *session = &manager->clients[client_id];
    if (session->active) {
        printf("client_manager_remove_client: Client disconnected: %s (ID: %d)\n",
               session->nickname, client_id);
        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "*** %s left the chat ***\n", session->nickname);
        client_session_cleanup(session);
        manager->client_count--;
        pthread_mutex_unlock(&manager->mutex);
        client_manager_broadcast_message(manager, message, client_id);
        return;
    }
    pthread_mutex_unlock(&manager->mutex);
}
void client_manager_broadcast_message(client_manager_t *manager, const char *message, int sender_id)
{
    if (!manager || !message) {
        return;
    }
    pthread_mutex_lock(&manager->mutex);
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_session_t *session = &manager->clients[i];
        if (session->active && i != sender_id) {
            if (send(session->socket_fd, message, strlen(message), MSG_NOSIGNAL) < 0) {
                printf("client_manager_broadcast_message: Failed to send message to client %d - "
                       "removing client\n",
                       i);
                session->active = 0;
                client_session_cleanup(session);
                manager->client_count--;
            } else {
                sent_count++;
            }
        }
    }
    pthread_mutex_unlock(&manager->mutex);
    if (sent_count > 0) {
        printf("client_manager_broadcast_message: Broadcasted message to %d clients\n", sent_count);
    }
}
void client_manager_broadcast_user_list(client_manager_t *manager)
{
    if (!manager) {
        return;
    }
    char user_list[BUFFER_SIZE] = "*** Online users: ";
    pthread_mutex_lock(&manager->mutex);
    int user_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (manager->clients[i].active) {
            if (user_count > 0) {
                strcat(user_list, ", ");
            }
            strcat(user_list, manager->clients[i].nickname);
            user_count++;
        }
    }
    pthread_mutex_unlock(&manager->mutex);
    if (user_count == 0) {
        strcat(user_list, "None");
    }
    strcat(user_list, " ***\n");
    printf("client_manager_broadcast_user_list: Broadcasting user list (%d users)\n", user_count);
    client_manager_broadcast_message(manager, user_list, -1);
}
client_session_t *client_manager_get_session(client_manager_t *manager, int client_id)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return NULL;
    }
    return &manager->clients[client_id];
}
