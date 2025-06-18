#include "../include/client_session.h"
#include "../include/chat_protocol.h"
#include "../include/client_manager.h"
#include "../include/handshake_protocol.h"
#include "../include/types.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void client_session_initialize(client_session_t *session, int socket_fd, struct sockaddr_in addr)
{
    if (!session) {
        fprintf(stderr, "client_session_initialize: session is NULL\n");
        return;
    }
    session->socket_fd       = socket_fd;
    session->addr            = addr;
    session->active          = 1;
    session->handshake_state = HANDSHAKE_IDLE;
    session->sequence_number = 0;
    snprintf(session->nickname, MAX_NICKNAME_LEN, "User%d", socket_fd);
    printf("client_session_initialize: Initialized session for socket %d\n", socket_fd);
}
void client_session_cleanup(client_session_t *session)
{
    if (!session) {
        return;
    }
    if (session->socket_fd >= 0) {
        close(session->socket_fd);
        printf("client_session_cleanup: Closed socket %d for client '%s'\n", session->socket_fd,
               session->nickname);
    }
    session->socket_fd       = -1;
    session->active          = 0;
    session->handshake_state = HANDSHAKE_FAILED;
    session->sequence_number = 0;
    memset(session->nickname, 0, MAX_NICKNAME_LEN);
}
void client_session_set_nickname(client_session_t *session, const char *nickname)
{
    if (!session || !nickname) {
        fprintf(stderr, "client_session_set_nickname: Invalid parameters\n");
        return;
    }
    char old_nickname[MAX_NICKNAME_LEN];
    strncpy(old_nickname, session->nickname, MAX_NICKNAME_LEN - 1);
    strncpy(session->nickname, nickname, MAX_NICKNAME_LEN - 1);
    session->nickname[MAX_NICKNAME_LEN - 1] = '\0';
    printf("client_session_set_nickname: Changed nickname from '%s' to '%s'\n", old_nickname,
           session->nickname);
}
int client_session_is_active(const client_session_t *session)
{
    return session && session->active && session->socket_fd >= 0;
}
static int client_session_receive_message(client_session_t *session, char *buffer,
                                          size_t buffer_size)
{
    if (!session || !buffer || buffer_size == 0) {
        return -1;
    }
    int bytes_received = recv(session->socket_fd, buffer, buffer_size - 1, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("client_session_receive_message: Client '%s' disconnected\n", session->nickname);
        } else {
            printf("client_session_receive_message: Error receiving from client '%s': %s\n",
                   session->nickname, strerror(errno));
        }
        return -1;
    }
    buffer[bytes_received] = '\0';
    return bytes_received;
}
void *client_session_handler_thread(void *arg)
{
    client_handler_args_t *args = (client_handler_args_t *)arg;
    if (!args) {
        fprintf(stderr, "client_session_handler_thread: args is NULL\n");
        pthread_exit(NULL);
    }
    client_manager_t *manager   = args->manager;
    int               client_id = args->client_id;
    free(args);
    client_session_t *session = client_manager_get_session(manager, client_id);
    if (!session) {
        fprintf(stderr, "client_session_handler_thread: Failed to get session for client %d\n",
                client_id);
        pthread_exit(NULL);
    }
    printf("Starting handler for client %d\n", client_id);
    if (handshake_perform_server_side(session) < 0) {
        printf("Handshake failed for client %d\n", client_id);
        client_manager_remove_client(manager, client_id);
        pthread_exit(NULL);
    }
    chat_protocol_send_welcome_message(session);
    chat_protocol_announce_user_joined(manager, session, client_id);
    char buffer[BUFFER_SIZE];
    while (client_session_is_active(session)) {
        int bytes_received = client_session_receive_message(session, buffer, BUFFER_SIZE);
        if (bytes_received <= 0) {
            break;
        }
        if (chat_protocol_handle_message(manager, session, buffer, client_id) < 0) {
            printf("client_session_handler_thread: Error handling message from client %d\n",
                   client_id);
            break;
        }
    }
    printf("client_session_handler_thread: Ending handler for client %d\n", client_id);
    client_manager_remove_client(manager, client_id);
    pthread_exit(NULL);
}
