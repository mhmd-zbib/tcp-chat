#include "../include/client_manager.h"
#include "../../utils/include/logger.h"
#include "../include/client_session.h"
#include "../include/security_foundation.h"
#include "../include/types.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static client_manager_t *g_manager = NULL;

void client_manager_initialize(client_manager_t *manager)
{
    if (!manager) {
        LOG_ERROR("client_manager_initialize: manager is NULL");
        return;
    }

    manager->client_count = 0;
    pthread_mutex_init(&manager->mutex, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_session_t *session = &manager->clients[i];
        session->active           = 0;
        session->socket_fd        = -1;
        memset(session->nickname, 0, MAX_NICKNAME_LEN);
        session->handshake_state = HANDSHAKE_IDLE;
        session->sequence_number = 0;
    }

    g_manager = manager;
    LOG_INFO("Client manager initialized");
}

void client_manager_cleanup(client_manager_t *manager)
{
    if (!manager) {
        return;
    }

    LOG_INFO("Cleaning up client manager");
    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (manager->clients[i].active) {
            client_session_cleanup(&manager->clients[i]);
            pthread_cancel(manager->clients[i].thread);
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    g_manager = NULL;
    LOG_INFO("Client manager cleanup complete");
}

int client_manager_add_new_client(client_manager_t *manager, int socket_fd, struct sockaddr_in addr)
{
    if (!manager) {
        LOG_ERROR("client_manager_add_new_client: manager is NULL");
        return -1;
    }

    pthread_mutex_lock(&manager->mutex);
    int client_id = -1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!manager->clients[i].active) {
            client_id = i;
            client_session_initialize(&manager->clients[i], socket_fd, addr);

            // Set client ID for security context tracking
            manager->clients[i].client_id          = client_id;
            manager->clients[i].security_validated = false;

            security_manager_t *sec_manager = get_security_manager();
            if (sec_manager) {
                if (security_manager_add_client(sec_manager, client_id, SECURITY_LEVEL_HIGH) < 0) {
                    LOG_ERROR("Failed to add client %d to security manager", client_id);
                    client_session_cleanup(&manager->clients[i]);
                    pthread_mutex_unlock(&manager->mutex);
                    return -1;
                }
                LOG_INFO("Client %d added to security manager with HIGH security level", client_id);

                // TODO: Replace with secure key exchange protocol
                uint8_t session_key[32];
                for (int j = 0; j < 32; j++) {
                    session_key[j] = (uint8_t)(0x42 + j) ^ 0xAB;
                }
                pthread_mutex_lock(&sec_manager->manager_mutex);
                security_context_t *ctx = sec_manager->contexts[client_id];
                if (ctx && ctx->enc_ctx) {
                    if (init_encryption_context(ctx->enc_ctx, session_key) < 0) {
                        LOG_ERROR("Failed to initialize encryption context for client %d",
                                  client_id);
                        pthread_mutex_unlock(&sec_manager->manager_mutex);
                        client_session_cleanup(&manager->clients[i]);
                        pthread_mutex_unlock(&manager->mutex);
                        return -1;
                    }
                    LOG_INFO("Encryption context initialized for client %d", client_id);
                } else {
                    LOG_ERROR("Security context not available for client %d", client_id);
                }
                pthread_mutex_unlock(&sec_manager->manager_mutex);
            } else {
                LOG_WARN("Security manager not available for client %d", client_id);
            }

            manager->client_count++;
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (client_id != -1) {
        LOG_CONNECTION("New secure client connected: %s:%d (ID: %d)", inet_ntoa(addr.sin_addr),
                       ntohs(addr.sin_port), client_id);
    } else {
        LOG_WARN("Failed to add client - server full");
    }

    return client_id;
}

void client_manager_remove_client(client_manager_t *manager, int client_id)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);
    client_session_t *session = &manager->clients[client_id];

    if (session->active) {
        LOG_CONNECTION("Client disconnected: %s (ID: %d)", session->nickname, client_id);

        security_manager_t *sec_manager = get_security_manager();
        if (sec_manager) {
            security_manager_remove_client(sec_manager, client_id);
            LOG_INFO("Client %d removed from security manager", client_id);
        }

        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "*** %s left the chat ***\n", session->nickname);
        client_session_cleanup(session);
        manager->client_count--;

        pthread_mutex_unlock(&manager->mutex);
        client_manager_broadcast_message(manager, message, client_id);
        return;
    }

    pthread_mutex_unlock(&manager->mutex);
}

void client_manager_broadcast_message(client_manager_t *manager, const char *message, int sender_id)
{
    if (!manager || !message) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);
    int sent_count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_session_t *session = &manager->clients[i];
        if (session->active && i != sender_id) {
            if (send(session->socket_fd, message, strlen(message), MSG_NOSIGNAL) < 0) {
                LOG_WARN("Failed to send message to client %d - removing client", i);
                session->active = 0;
                client_session_cleanup(session);
                manager->client_count--;
            } else {
                sent_count++;
            }
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (sent_count > 0) {
        LOG_PROTOCOL("Broadcasted message to %d clients", sent_count);
    }
}

void client_manager_broadcast_user_list(client_manager_t *manager)
{
    if (!manager) {
        return;
    }

    char user_list[BUFFER_SIZE] = "*** Online users: ";
    pthread_mutex_lock(&manager->mutex);
    int user_count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (manager->clients[i].active) {
            if (user_count > 0) {
                strcat(user_list, ", ");
            }
            strcat(user_list, manager->clients[i].nickname);
            user_count++;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (user_count == 0) {
        strcat(user_list, "None");
    }

    strcat(user_list, " ***\n");
    LOG_INFO("Broadcasting user list (%d users)", user_count);
    client_manager_broadcast_message(manager, user_list, -1);
}

client_session_t *client_manager_get_session(client_manager_t *manager, int client_id)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return NULL;
    }

    return &manager->clients[client_id];
}
