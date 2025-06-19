#include "../include/cli_args.h"
#include "../include/client.h"
#include "logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static client_t *g_client = NULL;

void signal_handler(int sig)
{
    LOG_INFO("Received signal %d. Shutting down...", sig);
    if (g_client) {
        client_disconnect(g_client);
    }
    logger_cleanup();
    exit(0);
}

int main(int argc, char *argv[])
{
    // Initialize logger first
    logger_init("TCP-CHAT-CLIENT", LOG_LEVEL_DEBUG);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    LOG_INFO("TCP Chat Client starting...");

    cli_args_t args = parse_args(argc, argv);
    if (!args.valid) {
        LOG_ERROR("Invalid command line arguments");
        print_usage(argv[0]);
        logger_cleanup();
        return 1;
    }

    g_client = client_create(args.server_ip, args.server_port, args.nickname);
    if (!g_client) {
        LOG_ERROR("Failed to create client");
        logger_cleanup();
        return 1;
    }

    if (client_connect_to_server(g_client) < 0) {
        LOG_ERROR("Failed to connect to server");
        client_destroy(g_client);
        logger_cleanup();
        return 1;
    }

    LOG_INFO("\nConnection established! Press Ctrl+C to disconnect.");
    LOG_INFO("Waiting...");

    while (1) {
        sleep(1);
    }

    client_destroy(g_client);
    return 0;
}
