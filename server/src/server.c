#include "../include/server.h"
#include "../include/client_manager.h"
#include "../include/client_session.h"
#include "../include/types.h"
server_t *server_create(const char *ip, int port)
{
    server_t *server = malloc(sizeof(server_t));
    if (!server) {
        fprintf(stderr, "Failed to allocate memory for server\n");
        return NULL;
    }
    server->port       = port;
    server->is_running = 0;
    strncpy(server->ip, ip, INET_ADDRSTRLEN - 1);
    server->ip[INET_ADDRSTRLEN - 1] = '\0';
    client_manager_initialize(&server->client_manager);
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd == -1) {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        free(server);
        return NULL;
    }
    int opt = 1;
    if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Failed to set socket options: %s\n", strerror(errno));
        close(server->socket_fd);
        free(server);
        return NULL;
    }
    memset(&server->addr, 0, sizeof(server->addr));
    server->addr.sin_family = AF_INET;
    server->addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &server->addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        close(server->socket_fd);
        free(server);
        return NULL;
    }
    printf("TCP socket created successfully (fd: %d)\n", server->socket_fd);
    return server;
}
int server_bind(server_t *server)
{
    if (!server) {
        fprintf(stderr, "Server is NULL\n");
        return -1;
    }
    if (bind(server->socket_fd, (struct sockaddr *)&server->addr, sizeof(server->addr)) < 0) {
        fprintf(stderr, "Bind failed: %s\n", strerror(errno));
        return -1;
    }
    printf("Socket successfully bound to %s:%d\n", server->ip, server->port);
    return 0;
}
int server_accept_clients(server_t *server)
{
    struct sockaddr_in client_addr;
    socklen_t          client_len = sizeof(client_addr);
    int client_socket = accept(server->socket_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            fprintf(stderr, "Accept failed: %s\n", strerror(errno));
        }
        return -1;
    }
    int client_id =
        client_manager_add_new_client(&server->client_manager, client_socket, client_addr);
    if (client_id < 0) {
        fprintf(stderr, "Failed to add client - server full\n");
        close(client_socket);
        return -1;
    }
    client_handler_args_t *args = malloc(sizeof(client_handler_args_t));
    args->manager               = &server->client_manager;
    args->client_id             = client_id;
    client_session_t *client = client_manager_get_session(&server->client_manager, client_id);
    if (pthread_create(&client->thread, NULL, client_session_handler_thread, args) != 0) {
        fprintf(stderr, "Failed to create thread for client %d\n", client_id);
        client_manager_remove_client(&server->client_manager, client_id);
        free(args);
        return -1;
    }
    pthread_detach(client->thread);
    return client_id;
}
void server_destroy(server_t *server)
{
    if (server) {
        client_manager_cleanup(&server->client_manager);
        if (server->socket_fd >= 0) {
            close(server->socket_fd);
            printf("Socket closed\n");
        }
        free(server);
    }
}
void server_print_info(const server_t *server)
{
    if (!server) {
        printf("Server: NULL\n");
        return;
    }
    printf("=== Server Info ===\n");
    printf("Socket FD: %d\n", server->socket_fd);
    printf("IP Address: %s\n", server->ip);
    printf("Port: %d\n", server->port);
    printf("Status: %s\n", server->is_running ? "Running" : "Stopped");
    printf("==================\n");
}