#include "../include/cli_args.h"
#include "../include/server.h"
#include "../include/signal_handler.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    cli_args_t args = parse_args(argc, argv);
    if (!args.valid) {
        print_usage(argv[0]);
        return 1;
    }
    printf("Starting TCP Chat Server...\n");
    printf("Configuration: %s:%d\n", args.ip, args.port);
    printf("Press Ctrl+C to stop the server\n\n");
    server_t *server = server_create(args.ip, args.port);
    if (!server) {
        fprintf(stderr, "Failed to create TCP server\n");
        return 1;
    }
    signal_handler_init(server);
    server_print_info(server);
    if (server_bind(server) < 0) {
        fprintf(stderr, "Failed to bind socket\n");
        server_destroy(server);
        signal_handler_cleanup();
        return 1;
    }
    if (listen(server->socket_fd, MAX_PENDING_CONNECTIONS) < 0) {
        fprintf(stderr, "Listen failed: %s\n", strerror(errno));
        server_destroy(server);
        signal_handler_cleanup();
        return 1;
    }
    printf("Server listening for connections...\n");
    server->is_running = 1;
    int flags          = fcntl(server->socket_fd, F_GETFL, 0);
    fcntl(server->socket_fd, F_SETFL, flags | O_NONBLOCK);
    while (server->is_running) {
        server_accept_clients(server);
        usleep(100000);
    }
    printf("\nShutting down server...\n");
    server_destroy(server);
    signal_handler_cleanup();
    printf("Server shutdown complete.\n");
    return 0;
}
