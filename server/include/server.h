#ifndef SERVER_H
#define SERVER_H
#include "client_manager.h"
#include "types.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define DEFAULT_PORT            8080
#define DEFAULT_IP              "127.0.0.1"
#define MAX_PENDING_CONNECTIONS 10
typedef struct {
    int                socket_fd;
    struct sockaddr_in addr;
    int                port;
    char               ip[INET_ADDRSTRLEN];
    int                is_running;
    client_manager_t   client_manager;
} server_t;
server_t *server_create(const char *ip, int port);
int       server_bind(server_t *server);
int       server_accept_clients(server_t *server);
void      server_destroy(server_t *server);
void      server_print_info(const server_t *server);
#endif