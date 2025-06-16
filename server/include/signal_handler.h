#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H
#include "server.h"
void signal_handler_init(server_t *server);
void signal_handler_cleanup(void);
#endif
