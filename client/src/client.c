#include "../include/client.h"
#include "../include/handshake_protocol.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

client_t *client_create(const char *server_ip, int server_port, const char *nickname)
{
    if (!server_ip || !nickname) {
        fprintf(stderr, "client_create: Invalid arguments\n");
        return NULL;
    }

    client_t *client = malloc(sizeof(client_t));
    if (!client) {
        fprintf(stderr, "client_create: Failed to allocate memory\n");
        return NULL;
    }

    client->socket_fd = -1;
    strncpy(client->server_ip, server_ip, INET_ADDRSTRLEN - 1);
    client->server_ip[INET_ADDRSTRLEN - 1] = '\0';
    client->server_port                    = server_port;
    strncpy(client->nickname, nickname, MAX_NICKNAME_LEN - 1);
    client->nickname[MAX_NICKNAME_LEN - 1] = '\0';
    client->connected                      = 0;

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr) <= 0) {
        fprintf(stderr, "client_create: Invalid server IP address: %s\n", server_ip);
        free(client);
        return NULL;
    }

    printf("Client created for server %s:%d with nickname '%s'\n", server_ip, server_port,
           nickname);

    return client;
}

int client_connect_to_server(client_t *client)
{
    if (!client) {
        fprintf(stderr, "client_connect_to_server: client is NULL\n");
        return -1;
    }

    if (client->connected) {
        printf("client_connect_to_server: Already connected\n");
        return 0;
    }

    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        fprintf(stderr, "client_connect_to_server: Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(client->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "client_connect_to_server: Warning: Failed to set SO_REUSEADDR: %s\n",
                strerror(errno));
    }

    printf("Connecting to server %s:%d...\n", client->server_ip, client->server_port);

    if (connect(client->socket_fd, (struct sockaddr *)&client->server_addr,
                sizeof(client->server_addr)) < 0) {
        fprintf(stderr, "client_connect_to_server: Failed to connect to server: %s\n",
                strerror(errno));
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    printf("TCP connection established. Starting handshake...\n");

    usleep(100000);

    if (handshake_perform_client_side(client->socket_fd, client->nickname) < 0) {
        fprintf(stderr, "client_connect_to_server: Handshake failed\n");
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    client->connected = 1;
    printf("Successfully connected and authenticated with server!\n");

    return 0;
}

void client_disconnect(client_t *client)
{
    if (!client) {
        return;
    }

    if (client->socket_fd >= 0) {
        printf("Disconnecting from server...\n");
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    client->connected = 0;
    printf("Disconnected from server\n");
}

void client_destroy(client_t *client)
{
    if (!client) {
        return;
    }

    client_disconnect(client);
    free(client);
}
