#include "../include/signal_handler.h"
#include "../../utils/include/logger.h"
#include <signal.h>
#include <stdio.h>
static server_t *g_server = NULL;
static void      signal_handler(int signal)
{
    LOG_WARN("Received signal %d. Shutting down server...", signal);
    if (g_server) {
        g_server->is_running = 0;
    }
}
void signal_handler_init(server_t *server)
{
    g_server = server;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
}
void signal_handler_cleanup(void)
{
    g_server = NULL;
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}