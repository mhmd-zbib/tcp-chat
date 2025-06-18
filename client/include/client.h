#ifndef CLIENT_H
#define CLIENT_H

#include "types.h"
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEFAULT_SERVER_IP   "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080

typedef struct {
    int                socket_fd;
    struct sockaddr_in server_addr;
    char               server_ip[INET_ADDRSTRLEN];
    int                server_port;
    char               nickname[MAX_NICKNAME_LEN];
    int                connected;
} client_t;

client_t *client_create(const char *server_ip, int server_port, const char *nickname);
int       client_connect_to_server(client_t *client);
void      client_disconnect(client_t *client);
void      client_destroy(client_t *client);

#endif
