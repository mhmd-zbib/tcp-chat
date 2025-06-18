#ifndef CLI_ARGS_H
#define CLI_ARGS_H

typedef struct {
    char *server_ip;
    int   server_port;
    char *nickname;
    int   valid;
} cli_args_t;

cli_args_t parse_args(int argc, char *argv[]);
void       print_usage(const char *program_name);

#endif
