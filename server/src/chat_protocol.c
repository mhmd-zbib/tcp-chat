#include "../include/chat_protocol.h"
#include "../../utils/include/logger.h"
#include "../include/client_manager.h"
#include "../include/client_session.h"
#include "../include/types.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
                                        const char *command)
{
    if (!manager || !session || !command) {
        return -1;
    }
    LOG_DEBUG("chat_protocol_handle_command: Processing command '%s' from client '%s'", command,
              session->nickname);
    if (strncmp(command, "/nick ", 6) == 0) {
        const char *new_nickname = command + 6;
        return chat_protocol_handle_nickname_change(manager, session, new_nickname);
    } else if (strncmp(command, "/users", 6) == 0 || strncmp(command, "/list", 5) == 0) {
        client_manager_broadcast_user_list(manager);
        return 0;
    } else if (strncmp(command, "/help", 5) == 0) {
        const char *help_msg = "Available commands:\n"
                               "  /nick <nickname>  - Change your nickname\n"
                               "  /users or /list   - Show online users\n"
                               "  /help             - Show this help message\n"
                               "  /quit             - Disconnect from chat\n";
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
        return chat_protocol_handle_command(manager, session, clean_message);
    }
    char formatted_message[BUFFER_SIZE + MAX_NICKNAME_LEN + 10];
    snprintf(formatted_message, sizeof(formatted_message), "%s: %s\n", session->nickname,
             clean_message);
    LOG_DEBUG("chat_protocol_handle_message: Broadcasting message from '%s': %s", session->nickname,
              formatted_message);
    client_manager_broadcast_message(manager, formatted_message, client_id);
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
                              "=====================================\n";
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
    char announcement[BUFFER_SIZE];
    snprintf(announcement, BUFFER_SIZE, "*** %s joined the chat ***\n", session->nickname);
    LOG_INFO("chat_protocol_announce_user_joined: %s", announcement);
    client_manager_broadcast_message(manager, announcement, client_id);
    client_manager_broadcast_user_list(manager);
}
