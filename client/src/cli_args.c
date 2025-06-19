#include "../include/cli_args.h"
#include "../../utils/include/logger.h"
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
        LOG_ERROR("Error: Nickname is required");
        return args;
    }

    args.nickname = argv[1];

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                args.server_ip = argv[++i];
            } else {
                LOG_ERROR("Error: --host requires an IP address");
                return args;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                args.server_port = atoi(argv[++i]);
                if (args.server_port <= 0 || args.server_port > 65535) {
                    LOG_ERROR("Error: Invalid port number");
                    return args;
                }
            } else {
                LOG_ERROR("Error: --port requires a port number");
                return args;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return args;
        } else {
            LOG_ERROR("Error: Unknown argument '%s'", argv[i]);
            return args;
        }
    }

    args.valid = 1;
    return args;
}

void print_usage(const char *program_name)
{
    LOG_INFO("Usage: %s <nickname> [options]", program_name);
    LOG_INFO("Options:");
    LOG_INFO("  -h, --host <ip>    Server IP address (default: %s)", DEFAULT_SERVER_IP);
    LOG_INFO("  -p, --port <port>  Server port (default: %d)", DEFAULT_SERVER_PORT);
    LOG_INFO("  --help             Show this help message");
    LOG_INFO("Example:");
    LOG_INFO("  %s Alice", program_name);
    LOG_INFO("  %s Bob --host 192.168.1.100 --port 9000", program_name);
}
