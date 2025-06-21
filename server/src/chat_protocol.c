#include "../include/chat_protocol.h"
#include "../../utils/include/logger.h"
#include "../include/client_manager.h"
#include "../include/client_session.h"
#include "../include/room_manager.h"
#include "../include/types.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// External room manager instance
extern room_manager_t *g_room_manager;
static int chat_protocol_send_message_to_client(client_session_t *session, const char *message)
{
    if (!session || !message) {
        return -1;
    }
    if (send(session->socket_fd, message, strlen(message), MSG_NOSIGNAL) < 0) {
        LOG_ERROR("chat_protocol_send_message_to_client: Failed to send message to client '%s': %s",
                  session->nickname, strerror(errno));
        return -1;
    }
    return 0;
}
static int chat_protocol_handle_command(client_manager_t *manager, client_session_t *session,
                                        const char *command, int client_id)
{
    if (!manager || !session || !command) {
        return -1;
    }
    LOG_DEBUG("chat_protocol_handle_command: Processing command '%s' from client '%s'", command,
              session->nickname);

    if (strncmp(command, "/create ", 8) == 0) {
        const char *room_name = command + 8;
        return chat_protocol_handle_create_room(manager, session, room_name, client_id);
    } else if (strncmp(command, "/join ", 6) == 0) {
        const char *room_id = command + 6;
        return chat_protocol_handle_join_room(manager, session, room_id, client_id);
    } else if (strncmp(command, "/leave", 6) == 0) {
        return chat_protocol_handle_leave_room(manager, session, client_id);
    } else if (strncmp(command, "/rooms", 6) == 0) {
        return chat_protocol_handle_list_rooms(session);
    } else if (strncmp(command, "/nick ", 6) == 0) {
        const char *new_nickname = command + 6;
        return chat_protocol_handle_nickname_change(manager, session, new_nickname);
    } else if (strncmp(command, "/users", 6) == 0 || strncmp(command, "/list", 5) == 0) {
        return chat_protocol_handle_list_users(session, client_id);
    } else if (strncmp(command, "/help", 5) == 0) {
        const char *help_msg = "Available commands:\n"
                               "  /create <name>        - Create a new room\n"
                               "  /join <room_id>       - Join an existing room\n"
                               "  /leave                - Leave current room\n"
                               "  /rooms                - List all rooms\n"
                               "  /users or /list       - Show users in current room\n"
                               "  /nick <nickname>      - Change your nickname\n"
                               "  /help                 - Show this help message\n"
                               "  /quit                 - Disconnect from chat\n";
        chat_protocol_send_message_to_client(session, help_msg);
        return 0;
    } else if (strncmp(command, "/quit", 5) == 0) {
        LOG_CLIENT("Client '%s' requested to quit", session->nickname);
        return -1;
    } else {
        const char *unknown_cmd = "Unknown command. Type /help for available commands.\n";
        chat_protocol_send_message_to_client(session, unknown_cmd);
        return 0;
    }
}
int chat_protocol_handle_message(client_manager_t *manager, client_session_t *session,
                                 const char *message, int client_id)
{
    if (!manager || !session || !message) {
        return -1;
    }
    char clean_message[BUFFER_SIZE];
    strncpy(clean_message, message, BUFFER_SIZE - 1);
    clean_message[BUFFER_SIZE - 1]                = '\0';
    clean_message[strcspn(clean_message, "\n\r")] = '\0';
    if (strlen(clean_message) == 0) {
        return 0;
    }
    if (clean_message[0] == '/') {
        return chat_protocol_handle_command(manager, session, clean_message, client_id);
    }

    if (session->state != CLIENT_STATE_IN_ROOM || strlen(session->current_room_id) == 0) {
        const char *error_msg =
            "You must join a room before sending messages. Use /create <name> or /join <room_id>\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    char formatted_message[BUFFER_SIZE + MAX_NICKNAME_LEN + 10];
    snprintf(formatted_message, sizeof(formatted_message), "%s: %s\n", session->nickname,
             clean_message);
    LOG_DEBUG("chat_protocol_handle_message: Broadcasting message from '%s' in room '%s'",
              session->nickname, session->current_room_id);

    // Broadcast to room instead of all clients
    room_manager_broadcast_to_room(g_room_manager, session->current_room_id, formatted_message,
                                   client_id);
    return 0;
}
int chat_protocol_handle_nickname_change(client_manager_t *manager, client_session_t *session,
                                         const char *new_nickname)
{
    if (!manager || !session || !new_nickname) {
        return -1;
    }
    char clean_nickname[MAX_NICKNAME_LEN];
    strncpy(clean_nickname, new_nickname, MAX_NICKNAME_LEN - 1);
    clean_nickname[MAX_NICKNAME_LEN - 1]               = '\0';
    clean_nickname[strcspn(clean_nickname, "\n\r \t")] = '\0';
    if (strlen(clean_nickname) == 0) {
        const char *error_msg = "Error: Nickname cannot be empty.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }
    if (strlen(clean_nickname) >= MAX_NICKNAME_LEN) {
        const char *error_msg = "Error: Nickname too long.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }
    char old_nickname[MAX_NICKNAME_LEN];
    strncpy(old_nickname, session->nickname, MAX_NICKNAME_LEN - 1);
    client_session_set_nickname(session, clean_nickname);
    char announcement[BUFFER_SIZE];
    snprintf(announcement, BUFFER_SIZE, "*** %s is now known as %s ***\n", old_nickname,
             session->nickname);
    LOG_INFO("chat_protocol_handle_nickname_change: %s", announcement);
    client_manager_broadcast_message(manager, announcement, -1);
    client_manager_broadcast_user_list(manager);
    return 0;
}
void chat_protocol_send_welcome_message(client_session_t *session)
{
    if (!session) {
        return;
    }
    const char *welcome_msg = "=== Welcome to TCP Chat Server ===\n"
                              "You are now connected to the chat room.\n"
                              "Type /help for available commands.\n"
                              "Type your messages to chat with other users.\n"
                              "=====================================\n\n"
                              "=== Room Selection Required ===\n"
                              "Please choose an option:\n"
                              "  /create <room_name> - Create a new room\n"
                              "  /join <room_id>     - Join an existing room\n"
                              "  /rooms              - List available rooms\n"
                              "===============================\n";
    if (chat_protocol_send_message_to_client(session, welcome_msg) < 0) {
        LOG_ERROR(
            "chat_protocol_send_welcome_message: Failed to send welcome message to client '%s'",
            session->nickname);
    } else {
        LOG_INFO("chat_protocol_send_welcome_message: Sent welcome message to client '%s'",
                 session->nickname);
    }
}
void chat_protocol_announce_user_joined(client_manager_t *manager, client_session_t *session,
                                        int client_id)
{
    if (!manager || !session) {
        return;
    }

    session->state = CLIENT_STATE_IN_LOBBY;

    char announcement[BUFFER_SIZE];
    snprintf(announcement, BUFFER_SIZE, "%s connected\n", session->nickname);
    LOG_INFO("chat_protocol_announce_user_joined: %s", announcement);

    // Room instructions are now sent as part of the welcome message
    // No need to send them again here
}

// Room command handlers following FILENAME_FUNCTIONNAME pattern
int chat_protocol_handle_create_room(client_manager_t *manager, client_session_t *session,
                                     const char *room_name, int client_id)
{
    if (!manager || !session || !room_name || !g_room_manager) {
        LOG_ERROR("chat_protocol_handle_create_room: Invalid parameters");
        return -1;
    }

    if (strlen(room_name) == 0 || strlen(room_name) >= MAX_ROOM_NAME) {
        const char *error_msg = "Invalid room name. Must be 1-63 characters.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    // Leave current room if in one
    if (session->state == CLIENT_STATE_IN_ROOM && strlen(session->current_room_id) > 0) {
        room_manager_leave_room(g_room_manager, session->current_room_id, client_id);
    }

    char *room_id = room_manager_create_room(g_room_manager, room_name, client_id);
    if (!room_id) {
        const char *error_msg = "Failed to create room. Server may be full.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    // Update client state
    strncpy(session->current_room_id, room_id, ROOM_ID_LENGTH);
    session->current_room_id[ROOM_ID_LENGTH] = '\0';
    session->state                           = CLIENT_STATE_IN_ROOM;

    char success_msg[BUFFER_SIZE];
    snprintf(success_msg, BUFFER_SIZE,
             "Room created successfully!\n"
             "Room ID: %s\n"
             "Room Name: %s\n"
             "You can now start chatting!\n",
             room_id, room_name);
    chat_protocol_send_message_to_client(session, success_msg);

    free(room_id);
    LOG_INFO("chat_protocol_handle_create_room: Client '%s' created room '%s'", session->nickname,
             room_name);
    return 0;
}

int chat_protocol_handle_join_room(client_manager_t *manager, client_session_t *session,
                                   const char *room_id, int client_id)
{
    if (!manager || !session || !room_id || !g_room_manager) {
        LOG_ERROR("chat_protocol_handle_join_room: Invalid parameters");
        return -1;
    }

    if (strlen(room_id) != ROOM_ID_LENGTH) {
        const char *error_msg = "Invalid room ID format.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    // Leave current room if in one
    if (session->state == CLIENT_STATE_IN_ROOM && strlen(session->current_room_id) > 0) {
        room_manager_leave_room(g_room_manager, session->current_room_id, client_id);
    }

    int result = room_manager_join_room(g_room_manager, room_id, client_id);
    if (result < 0) {
        const char *error_msg = "Room not found or room is full.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    // Update client state
    strncpy(session->current_room_id, room_id, ROOM_ID_LENGTH);
    session->current_room_id[ROOM_ID_LENGTH] = '\0';
    session->state                           = CLIENT_STATE_IN_ROOM;

    // Get room info
    room_t *room = room_manager_find_room(g_room_manager, room_id);
    char    success_msg[BUFFER_SIZE];
    if (room) {
        snprintf(success_msg, BUFFER_SIZE,
                 "Joined room successfully!\n"
                 "Room: %s (ID: %s)\n"
                 "You can now start chatting!\n",
                 room->room_name, room_id);
    } else {
        snprintf(success_msg, BUFFER_SIZE,
                 "Joined room %s successfully!\n"
                 "You can now start chatting!\n",
                 room_id);
    }
    chat_protocol_send_message_to_client(session, success_msg);

    // Announce to room
    char announcement[BUFFER_SIZE];
    snprintf(announcement, BUFFER_SIZE, "*** %s joined the room ***\n", session->nickname);
    room_manager_broadcast_to_room(g_room_manager, room_id, announcement, client_id);

    LOG_INFO("chat_protocol_handle_join_room: Client '%s' joined room '%s'", session->nickname,
             room_id);
    return 0;
}

int chat_protocol_handle_leave_room(client_manager_t *manager, client_session_t *session,
                                    int client_id)
{
    if (!manager || !session || !g_room_manager) {
        LOG_ERROR("chat_protocol_handle_leave_room: Invalid parameters");
        return -1;
    }

    if (session->state != CLIENT_STATE_IN_ROOM || strlen(session->current_room_id) == 0) {
        const char *error_msg = "You are not currently in a room.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    // Announce leaving to room
    char announcement[BUFFER_SIZE];
    snprintf(announcement, BUFFER_SIZE, "*** %s left the room ***\n", session->nickname);
    room_manager_broadcast_to_room(g_room_manager, session->current_room_id, announcement,
                                   client_id);

    // Leave the room
    room_manager_leave_room(g_room_manager, session->current_room_id, client_id);

    // Update client state
    memset(session->current_room_id, 0, sizeof(session->current_room_id));
    session->state = CLIENT_STATE_IN_LOBBY;

    const char *success_msg = "Left room successfully.\n"
                              "Use /create <name> or /join <room_id> to enter a new room.\n";
    chat_protocol_send_message_to_client(session, success_msg);

    LOG_INFO("chat_protocol_handle_leave_room: Client '%s' left room", session->nickname);
    return 0;
}

int chat_protocol_handle_list_rooms(client_session_t *session)
{
    if (!session || !g_room_manager) {
        LOG_ERROR("chat_protocol_handle_list_rooms: Invalid parameters");
        return -1;
    }

    char room_list[BUFFER_SIZE * 2];
    room_manager_list_rooms(g_room_manager, room_list, sizeof(room_list));
    chat_protocol_send_message_to_client(session, room_list);

    return 0;
}

int chat_protocol_handle_list_users(client_session_t *session, int client_id)
{
    if (!session || !g_room_manager) {
        LOG_ERROR("chat_protocol_handle_list_users: Invalid parameters");
        return -1;
    }

    if (session->state != CLIENT_STATE_IN_ROOM || strlen(session->current_room_id) == 0) {
        const char *error_msg = "You must be in a room to see user list.\n";
        chat_protocol_send_message_to_client(session, error_msg);
        return 0;
    }

    char user_list[BUFFER_SIZE];
    room_manager_get_room_users(g_room_manager, session->current_room_id, user_list,
                                sizeof(user_list));
    chat_protocol_send_message_to_client(session, user_list);

    return 0;
}
