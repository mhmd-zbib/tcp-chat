#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H
#include "handshake_protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#ifndef MAX_NICKNAME_LEN
#include "types.h"
#endif
typedef struct client_session_s {
    int                socket_fd;
    struct sockaddr_in addr;
    char               nickname[MAX_NICKNAME_LEN];
    pthread_t          thread;
    int                active;
    handshake_state_t  handshake_state;
    uint32_t           sequence_number;
    client_state_t     state;
    char               current_room_id[ROOM_ID_LENGTH + 1];
} client_session_t;
void  client_session_initialize(client_session_t *session, int socket_fd, struct sockaddr_in addr);
void  client_session_cleanup(client_session_t *session);
void  client_session_set_nickname(client_session_t *session, const char *nickname);
int   client_session_is_active(const client_session_t *session);
void *client_session_handler_thread(void *arg);
#endif
