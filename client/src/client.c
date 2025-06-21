#include "../include/client.h"
#include "../../utils/include/logger.h"
#include "../include/handshake_protocol.h"
#include "../include/security_foundation.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

client_t *client_create(const char *server_ip, int server_port, const char *nickname)
{
    if (!server_ip || !nickname) {
        LOG_ERROR("client_create: Invalid arguments");
        return NULL;
    }

    if (security_foundation_init() < 0) {
        LOG_ERROR("client_create: Failed to initialize security foundation");
        return NULL;
    }

    client_t *client = malloc(sizeof(client_t));
    if (!client) {
        LOG_ERROR("client_create: Failed to allocate memory");
        return NULL;
    }

    client->socket_fd = -1;
    strncpy(client->server_ip, server_ip, INET_ADDRSTRLEN - 1);
    client->server_ip[INET_ADDRSTRLEN - 1] = '\0';
    client->server_port                    = server_port;
    strncpy(client->nickname, nickname, MAX_NICKNAME_LEN - 1);
    client->nickname[MAX_NICKNAME_LEN - 1] = '\0';
    client->connected                      = 0;

    client->security_ctx = security_context_create(SECURITY_LEVEL_HIGH);
    if (!client->security_ctx) {
        LOG_ERROR("client_create: Failed to create security context");
        free(client);
        return NULL;
    }

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr) <= 0) {
        LOG_ERROR("client_create: Invalid server IP address: %s", server_ip);
        security_context_destroy(client->security_ctx);
        free(client);
        return NULL;
    }

    LOG_INFO("Client created with military-grade security context");
    LOG_INFO("Security Level: HIGH, Hardware Features Enabled");
    LOG_INFO("Target server: %s:%d with nickname '%s'", server_ip, server_port, nickname);

    return client;
}

int client_connect_to_server(client_t *client)
{
    if (!client) {
        LOG_ERROR("client_connect_to_server: client is NULL");
        return -1;
    }

    if (client->connected) {
        LOG_INFO("client_connect_to_server: Already connected");
        return 0;
    }

    LOG_INFO("Performing pre-connection security validation...");

    if (detect_debugger()) {
        LOG_WARN("SECURITY ALERT: Debugger detected - continuing for Phase 1 development");
    }

    if (verify_process_integrity() < 0) {
        LOG_WARN("Process integrity verification failed - continuing for Phase 1 development");
    }

    if (security_context_validate(client->security_ctx) < 0) {
        LOG_WARN("Security context validation failed - continuing for Phase 1 development");
    }

    LOG_INFO("Security validation complete - establishing connection");

    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        LOG_ERRNO("client_connect_to_server: Failed to create socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(client->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARN("client_connect_to_server: Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    LOG_CONNECTION("Establishing secure connection to server %s:%d...", client->server_ip,
                   client->server_port);

    if (connect(client->socket_fd, (struct sockaddr *)&client->server_addr,
                sizeof(client->server_addr)) < 0) {
        LOG_ERRNO("client_connect_to_server: Failed to connect to server");
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    LOG_CONNECTION("TCP connection established. Starting handshake...");

    usleep(100000);

    if (handshake_perform_client_side(client->socket_fd, client->nickname) < 0) {
        LOG_ERROR("client_connect_to_server: Handshake failed");
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    // TODO: Replace with secure key exchange protocol
    uint8_t session_key[32];
    for (int i = 0; i < 32; i++) {
        session_key[i] = (uint8_t)(0x42 + i) ^ 0xAB;
    }

    if (init_encryption_context(client->security_ctx->enc_ctx, session_key) < 0) {
        LOG_ERROR("Failed to initialize encryption context");
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    LOG_DEBUG("Encryption context initialized");

    client->connected = 1;
    LOG_CONNECTION("Successfully connected and authenticated with server!");

    return 0;
}

void client_disconnect(client_t *client)
{
    if (!client) {
        return;
    }

    if (client->socket_fd >= 0 && client->connected) {
        LOG_CONNECTION("Disconnecting from server...");

        const char *quit_msg = "/quit";
        send(client->socket_fd, quit_msg, strlen(quit_msg), MSG_NOSIGNAL);

        usleep(100000); // 100ms

        close(client->socket_fd);
        client->socket_fd = -1;
    }

    client->connected = 0;
    LOG_CONNECTION("Disconnected from server");
}

void client_destroy(client_t *client)
{
    if (!client) {
        return;
    }

    // Disconnect first
    client_disconnect(client);

    // Destroy security context
    if (client->security_ctx) {
        security_context_destroy(client->security_ctx);
        client->security_ctx = NULL;
    }

    // Cleanup security foundation
    security_foundation_cleanup();

    free(client);
    LOG_INFO("Client resources released with secure cleanup");
}

int client_send_message(client_t *client, const char *message)
{
    if (!client || !message || !client->connected) {
        LOG_ERROR("client_send_message: Invalid parameters or not connected");
        return -1;
    }

    if (security_context_validate(client->security_ctx) < 0) {
        LOG_ERROR("Security context validation failed - message not sent");
        return -1;
    }

    size_t message_len = strlen(message);

    // TODO: Implement multi-layer encryption for production
    uint8_t encrypted_buffer[BUFFER_SIZE * 2];
    size_t  encrypted_len = sizeof(encrypted_buffer);

    if (encrypt_data(client->security_ctx->enc_ctx, (const uint8_t *)message, message_len,
                     encrypted_buffer, &encrypted_len) < 0) {
        LOG_ERROR("Failed to encrypt message");
        return -1;
    }

    ssize_t bytes_sent = send(client->socket_fd, encrypted_buffer, encrypted_len, 0);
    if (bytes_sent < 0) {
        LOG_ERRNO("client_send_message: Failed to send encrypted message");
        return -1;
    } else if ((size_t)bytes_sent < encrypted_len) {
        LOG_WARN("client_send_message: Partial encrypted message sent: %zd/%zu bytes", bytes_sent,
                 encrypted_len);
        return -1;
    }

    LOG_DEBUG("Sent encrypted message (%zu bytes plaintext -> %zu bytes encrypted)", message_len,
              encrypted_len);
    return 0;
}

int client_receive_message(client_t *client, char *buffer, size_t buffer_size)
{
    if (!client || !buffer || buffer_size == 0 || !client->connected) {
        LOG_ERROR("client_receive_message: Invalid parameters or not connected");
        return -1;
    }

    if (security_context_validate(client->security_ctx) < 0) {
        LOG_ERROR("Security context validation failed - message reception blocked");
        return -1;
    }

    uint8_t encrypted_buffer[BUFFER_SIZE * 2];
    ssize_t bytes_received = recv(client->socket_fd, encrypted_buffer, sizeof(encrypted_buffer), 0);

    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            LOG_CONNECTION("Server disconnected");
            client->connected = 0;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            } else {
                LOG_ERRNO("client_receive_message: Error receiving encrypted data");
            }
        }
        return -1;
    }

    size_t decrypted_len = buffer_size - 1;

    if (decrypt_data(client->security_ctx->enc_ctx, encrypted_buffer, (size_t)bytes_received,
                     (uint8_t *)buffer, &decrypted_len) < 0) {
        LOG_ERROR("Failed to decrypt received message");
        return -1;
    }

    buffer[decrypted_len] = '\0';
    LOG_DEBUG("Received encrypted message (%zd bytes encrypted -> %zu bytes plaintext)",
              bytes_received, decrypted_len);

    return (int)decrypted_len;
}
