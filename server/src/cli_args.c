#include "../include/cli_args.h"
#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
cli_args_t parse_args(int argc, char *argv[])
{
    cli_args_t args = {.ip = (char *)DEFAULT_IP, .port = DEFAULT_PORT, .valid = 1};
    if (argc >= 2) {
        args.port = atoi(argv[1]);
        if (args.port <= 0 || args.port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            args.valid = 0;
            return args;
        }
    }
    if (argc >= 3) {
        args.ip = argv[2];
    }
    if (argc > 3) {
        fprintf(stderr, "Too many arguments\n");
        args.valid = 0;
    }
    return args;
}
void print_usage(const char *program_name)
{
    printf("Usage: %s [port] [ip]\n", program_name);
    printf("  port: Port number to listen on (1-65535, default: %d)\n", DEFAULT_PORT);
    printf("  ip:   IP address to bind to (default: %s)\n", DEFAULT_IP);
}