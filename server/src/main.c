#include "../include/cli_args.h"
#include "../include/server.h"
#include "../include/signal_handler.h"
#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    // Initialize logger first
    logger_init("TCP-CHAT-SERVER", LOG_LEVEL_DEBUG);

    cli_args_t args = parse_args(argc, argv);
    if (!args.valid) {
        LOG_ERROR("Invalid command line arguments");
        print_usage(argv[0]);
        logger_cleanup();
        return 1;
    }

    LOG_INFO("Starting TCP Chat Server...");
    LOG_INFO("Configuration: %s:%d", args.ip, args.port);
    LOG_INFO("Press Ctrl+C to stop the server");

    server_t *server = server_create(args.ip, args.port);
    if (!server) {
        LOG_FATAL("Failed to create TCP server");
        logger_cleanup();
        return 1;
    }

    signal_handler_init(server);
    server_print_info(server);

    if (server_bind(server) < 0) {
        LOG_FATAL("Failed to bind socket");
        server_destroy(server);
        signal_handler_cleanup();
        logger_cleanup();
        return 1;
    }
    if (listen(server->socket_fd, MAX_PENDING_CONNECTIONS) < 0) {
        LOG_ERRNO("Listen failed");
        server_destroy(server);
        signal_handler_cleanup();
        logger_cleanup();
        return 1;
    }

    LOG_SERVER("Server listening for connections...");
    server->is_running = 1;
    int flags          = fcntl(server->socket_fd, F_GETFL, 0);
    fcntl(server->socket_fd, F_SETFL, flags | O_NONBLOCK);

    while (server->is_running) {
        server_accept_clients(server);
        usleep(100000);
    }

    LOG_INFO("Shutting down server...");
    server_destroy(server);
    signal_handler_cleanup();
    logger_cleanup();
    LOG_INFO("Server shutdown complete");
    return 0;
}
