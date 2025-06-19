#include "../include/cli_args.h"
#include "../../utils/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
cli_args_t parse_args(int argc, char *argv[])
{
    cli_args_t args = {.valid = 1, .ip = DEFAULT_IP, .port = DEFAULT_PORT};
    if (argc > 1) {
        args.port = atoi(argv[1]);
        if (args.port <= 0 || args.port > 65535) {
            LOG_ERROR("Invalid port number: %s", argv[1]);
            args.valid = 0;
            return args;
        }
    }
    if (argc > 2) {
        args.ip = argv[2];
    }
    if (argc > 3) {
        LOG_ERROR("Too many arguments");
        args.valid = 0;
    }
    return args;
}

void print_usage(const char *program_name)
{
    LOG_INFO("Usage: %s [port] [ip]", program_name);
    LOG_INFO("  port: Port number to listen on (1-65535, default: %d)", DEFAULT_PORT);
    LOG_INFO("  ip:   IP address to bind to (default: %s)", DEFAULT_IP);
}