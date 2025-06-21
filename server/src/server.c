#include "../include/server.h"
#include "../../utils/include/logger.h"
#include "../include/client_manager.h"
#include "../include/client_session.h"
#include "../include/room_manager.h"
#include "../include/types.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h> // for fcntl
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h> // for nanosleep
#include <unistd.h>

// Global instances for access from other modules
room_manager_t   *g_room_manager   = NULL;
client_manager_t *g_client_manager = NULL;

server_t *server_create(const char *ip, int port)
{
    server_t *server = malloc(sizeof(server_t));
    if (!server) {
        LOG_FATAL("Failed to allocate memory for server");
        return NULL;
    }
    server->port       = port;
    server->is_running = 0;
    strncpy(server->ip, ip, INET_ADDRSTRLEN - 1);
    server->ip[INET_ADDRSTRLEN - 1] = '\0';

    // Initialize managers
    client_manager_initialize(&server->client_manager);
    room_manager_initialize(&server->room_manager);

    // Set global references
    g_client_manager = &server->client_manager;
    g_room_manager   = &server->room_manager;

    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd == -1) {
        LOG_ERRNO("Socket creation failed");
        free(server);
        return NULL;
    }

    // Set socket options
    int opt = 1;
    // Enable address reuse
    if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARN("Failed to set SO_REUSEADDR option: %s", strerror(errno));
        close(server->socket_fd);
        free(server);
        return NULL;
    }

    // Ensure socket is in blocking mode
    int flags = fcntl(server->socket_fd, F_GETFL, 0);
    if (flags == -1) {
        LOG_ERRNO("Failed to get socket flags");
        close(server->socket_fd);
        free(server);
        return NULL;
    }

    // Remove O_NONBLOCK flag if it's set
    if (flags & O_NONBLOCK) {
        LOG_INFO("Setting socket to blocking mode");
        if (fcntl(server->socket_fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
            LOG_ERRNO("Failed to set socket to blocking mode");
            close(server->socket_fd);
            free(server);
            return NULL;
        }
    }
    memset(&server->addr, 0, sizeof(server->addr));
    server->addr.sin_family = AF_INET;
    server->addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &server->addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid IP address: %s", ip);
        close(server->socket_fd);
        free(server);
        return NULL;
    }
    LOG_SERVER("TCP socket created successfully (fd: %d)", server->socket_fd);
    return server;
}
int server_bind(server_t *server)
{
    if (!server) {
        LOG_ERROR("Server is NULL");
        return -1;
    }
    if (bind(server->socket_fd, (struct sockaddr *)&server->addr, sizeof(server->addr)) < 0) {
        LOG_ERRNO("Bind failed");
        return -1;
    }
    LOG_SERVER("Socket successfully bound to %s:%d", server->ip, server->port);
    return 0;
}
int server_accept_clients(server_t *server)
{
    if (!server) {
        LOG_ERROR("server_accept_clients: server is NULL");
        return -1;
    }

    server->is_running = 1;
    LOG_SERVER("Server is running and accepting connections");
    LOG_CONNECTION("Waiting for incoming connections...");

    while (server->is_running) {
        struct sockaddr_in client_addr;
        socklen_t          client_addr_len = sizeof(client_addr);

        // accept() will block until a connection is received
        int client_socket =
            accept(server->socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_socket < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we're still supposed to run
                if (!server->is_running) {
                    LOG_INFO("Server shutdown requested, stopping accept loop");
                    break;
                }
                continue;
            }

            // Handle expected non-blocking mode errors
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // These errors are normal in non-blocking mode - just sleep a bit
                struct timespec ts = {0, 100000000}; // 100ms
                nanosleep(&ts, NULL);
                continue;
            }

            // For other unexpected errors, log and sleep to avoid flooding logs
            LOG_ERRNO("Accept failed");
            struct timespec ts = {0, 500000000}; // 500ms
            nanosleep(&ts, NULL);
            LOG_CONNECTION("Waiting for incoming connections..."); // Re-log after error
            continue;
        }

        LOG_CONNECTION("New client connection from %s:%d", inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));

        // Add client to the manager
        int client_id =
            client_manager_add_new_client(&server->client_manager, client_socket, client_addr);
        if (client_id < 0) {
            LOG_ERROR("Failed to add client to manager - closing connection");
            close(client_socket);
            continue;
        }

        // Start a thread for the client session
        client_session_t *session = client_manager_get_session(&server->client_manager, client_id);

        // Create arguments for the handler thread
        void *args                   = malloc(sizeof(client_manager_t *) + sizeof(int));
        *((client_manager_t **)args) = &server->client_manager;
        *((int *)((client_manager_t **)args + 1)) = client_id;

        if (pthread_create(&session->thread, NULL, client_session_handler_thread, args) != 0) {
            LOG_ERROR("Failed to create thread for client: %s", strerror(errno));
            client_manager_remove_client(&server->client_manager, client_id);
            LOG_CONNECTION("Waiting for incoming connections..."); // Re-log after error
            continue;
        }

        LOG_CONNECTION("Started session thread for client %d", client_id);
        LOG_CONNECTION("Waiting for incoming connections..."); // Ready for next connection
    }

    LOG_SERVER("Server stopped accepting connections");
    return 0;
}
void server_destroy(server_t *server)
{
    if (server) {
        client_manager_cleanup(&server->client_manager);
        room_manager_cleanup(&server->room_manager);

        // Clear global references
        g_client_manager = NULL;
        g_room_manager   = NULL;

        if (server->socket_fd >= 0) {
            close(server->socket_fd);
            LOG_INFO("Socket closed");
        }
        free(server);
    }
}
void server_print_info(const server_t *server)
{
    if (!server) {
        LOG_INFO("Server: NULL");
        return;
    }
    LOG_INFO("=== Server Info ===");
    LOG_INFO("Socket FD: %d", server->socket_fd);
    LOG_INFO("IP Address: %s", server->ip);
    LOG_INFO("Port: %d", server->port);
    LOG_INFO("Status: %s", server->is_running ? "Running" : "Stopped");
    LOG_INFO("==================");
}