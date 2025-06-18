#include "../include/cli_args.h"
#include "../include/client.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static client_t *g_client = NULL;

void signal_handler(int sig)
{
    printf("\nReceived signal %d. Shutting down...\n", sig);
    if (g_client) {
        client_disconnect(g_client);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("TCP Chat Client\n");
    printf("===============\n");

    cli_args_t args = parse_args(argc, argv);
    if (!args.valid) {
        print_usage(argv[0]);
        return 1;
    }

    g_client = client_create(args.server_ip, args.server_port, args.nickname);
    if (!g_client) {
        fprintf(stderr, "Failed to create client\n");
        return 1;
    }

    if (client_connect_to_server(g_client) < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        client_destroy(g_client);
        return 1;
    }

    printf("\nConnection established! Press Ctrl+C to disconnect.\n");
    printf("Waiting...\n");

    while (1) {
        sleep(1);
    }

    client_destroy(g_client);
    return 0;
}
