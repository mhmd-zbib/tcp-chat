#include "../include/cli_args.h"
#include "../include/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cli_args_t parse_args(int argc, char *argv[])
{
    cli_args_t args  = {0};
    args.server_ip   = DEFAULT_SERVER_IP;
    args.server_port = DEFAULT_SERVER_PORT;
    args.nickname    = NULL;
    args.valid       = 0;

    if (argc < 2) {
        fprintf(stderr, "Error: Nickname is required\n");
        return args;
    }

    args.nickname = argv[1];

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                args.server_ip = argv[++i];
            } else {
                fprintf(stderr, "Error: --host requires an IP address\n");
                return args;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                args.server_port = atoi(argv[++i]);
                if (args.server_port <= 0 || args.server_port > 65535) {
                    fprintf(stderr, "Error: Invalid port number\n");
                    return args;
                }
            } else {
                fprintf(stderr, "Error: --port requires a port number\n");
                return args;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return args;
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            return args;
        }
    }

    args.valid = 1;
    return args;
}

void print_usage(const char *program_name)
{
    printf("Usage: %s <nickname> [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --host <ip>    Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port <port>  Server port (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  --help             Show this help message\n");
    printf("\nExample:\n");
    printf("  %s Alice\n", program_name);
    printf("  %s Bob --host 192.168.1.100 --port 9000\n", program_name);
}
