#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H
#include "client_manager.h"
int  chat_protocol_handle_message(client_manager_t *manager, client_session_t *session,
                                  const char *message, int client_id);
int  chat_protocol_handle_nickname_change(client_manager_t *manager, client_session_t *session,
                                          const char *new_nickname);
void chat_protocol_send_welcome_message(client_session_t *session);
void chat_protocol_announce_user_joined(client_manager_t *manager, client_session_t *session,
                                        int client_id);
#endif
