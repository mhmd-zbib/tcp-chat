#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H
#include "client_session.h"
#include <pthread.h>
#ifndef MAX_CLIENTS
#include "types.h"
#endif
typedef struct {
    client_session_t clients[MAX_CLIENTS];
    int              client_count;
    pthread_mutex_t  mutex;
} client_manager_t;
typedef struct {
    client_manager_t *manager;
    int               client_id;
} client_handler_args_t;
void              client_manager_initialize(client_manager_t *manager);
void              client_manager_cleanup(client_manager_t *manager);
int               client_manager_add_new_client(client_manager_t *manager, int socket_fd,
                                                struct sockaddr_in addr);
void              client_manager_remove_client(client_manager_t *manager, int client_id);
void              client_manager_broadcast_message(client_manager_t *manager, const char *message,
                                                   int sender_id);
void              client_manager_broadcast_user_list(client_manager_t *manager);
client_session_t *client_manager_get_session(client_manager_t *manager, int client_id);
#endif