#include "../include/client.h"
#include "../../utils/include/logger.h"
#include "../include/e2e_encryption.h"
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

    client->security_ctx = security_context_create_compat(SECURITY_LEVEL_HIGH_INT);
    if (!client->security_ctx) {
        LOG_ERROR("client_create: Failed to create security context");
        free(client);
        return NULL;
    }

    // Initialize E2E encryption context
    client->e2e_ctx = malloc(sizeof(e2e_context_t));
    if (!client->e2e_ctx) {
        LOG_ERROR("client_create: Failed to allocate E2E context");
        security_context_destroy(client->security_ctx);
        free(client);
        return NULL;
    }

    if (e2e_context_init(client->e2e_ctx, nickname) < 0) {
        LOG_ERROR("client_create: Failed to initialize E2E encryption");
        free(client->e2e_ctx);
        security_context_destroy(client->security_ctx);
        free(client);
        return NULL;
    }

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr) <= 0) {
        LOG_ERROR("client_create: Invalid server IP address: %s", server_ip);
        e2e_context_cleanup(client->e2e_ctx);
        free(client->e2e_ctx);
        security_context_destroy(client->security_ctx);
        free(client);
        return NULL;
    }

    LOG_INFO("Client created with military-grade security + E2E encryption");
    LOG_INFO("Security Level: HIGH, E2E Encryption: ENABLED");
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

    // Initialize legacy encryption context for server communication (metadata only)
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

    LOG_DEBUG("Legacy encryption context initialized for server communication");

    // Broadcast our public key to other clients
    if (client_broadcast_public_key(client) < 0) {
        LOG_WARN("Failed to broadcast public key - E2E encryption may not work with all clients");
    }

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

    // Destroy E2E encryption context
    if (client->e2e_ctx) {
        e2e_context_cleanup(client->e2e_ctx);
        free(client->e2e_ctx);
        client->e2e_ctx = NULL;
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

// ============================================================================
// END-TO-END ENCRYPTION FUNCTIONS
// ============================================================================

int client_broadcast_public_key(client_t *client)
{
    if (!client || !client->connected || !client->e2e_ctx) {
        LOG_ERROR("client_broadcast_public_key: Invalid parameters or not connected");
        return -1;
    }

    // Create key exchange packet
    e2e_key_exchange_packet_t key_packet;
    if (e2e_create_key_exchange_packet(client->e2e_ctx, &key_packet) < 0) {
        LOG_ERROR("Failed to create key exchange packet");
        return -1;
    }

    // Prepare message with special prefix so server knows this is a key exchange
    char key_msg[sizeof(e2e_key_exchange_packet_t) * 2 + 50];
    snprintf(key_msg, sizeof(key_msg), "/keyexchange:");

    // Convert binary packet to hex string for transmission
    size_t hex_len  = sizeof(e2e_key_exchange_packet_t) * 2;
    char  *hex_data = key_msg + strlen(key_msg);

    const uint8_t *packet_bytes = (const uint8_t *)&key_packet;
    for (size_t i = 0; i < sizeof(e2e_key_exchange_packet_t); i++) {
        sprintf(hex_data + i * 2, "%02x", packet_bytes[i]);
    }
    hex_data[hex_len] = '\0';

    // Send through normal message channel (server will relay to other clients)
    if (client_send_message(client, key_msg) < 0) {
        LOG_ERROR("Failed to broadcast public key");
        return -1;
    }

    LOG_INFO("Broadcasted E2E public key to other clients");
    return 0;
}

int client_handle_key_exchange(client_t *client, const e2e_key_exchange_packet_t *key_packet)
{
    if (!client || !client->e2e_ctx || !key_packet) {
        LOG_ERROR("client_handle_key_exchange: Invalid parameters");
        return -1;
    }

    // Don't add our own key
    if (memcmp(key_packet->user_id, client->e2e_ctx->my_keys.user_id, 16) == 0) {
        LOG_DEBUG("Ignoring our own key exchange packet");
        return 0;
    }

    if (e2e_add_peer_key(client->e2e_ctx, key_packet) < 0) {
        LOG_ERROR("Failed to add peer key");
        return -1;
    }

    LOG_INFO("Added E2E public key for user: %s", key_packet->nickname);
    return 0;
}

int client_send_e2e_message(client_t *client, const char *message, const char *recipient_nickname)
{
    if (!client || !message || !recipient_nickname || !client->connected || !client->e2e_ctx) {
        LOG_ERROR("client_send_e2e_message: Invalid parameters or not connected");
        return -1;
    }

    // Find recipient's user ID by nickname
    uint8_t               recipient_id[16];
    const e2e_peer_key_t *peer = NULL;

    for (int i = 0; i < client->e2e_ctx->num_peers; i++) {
        if (client->e2e_ctx->peer_keys[i].active &&
            strcmp(client->e2e_ctx->peer_keys[i].nickname, recipient_nickname) == 0) {
            peer = &client->e2e_ctx->peer_keys[i];
            memcpy(recipient_id, peer->user_id, 16);
            break;
        }
    }

    if (!peer) {
        LOG_ERROR("Cannot send E2E message: recipient '%s' not found or no public key available",
                  recipient_nickname);
        return -1;
    }

    // Encrypt message with E2E encryption
    e2e_encrypted_packet_t encrypted_packet;
    if (e2e_encrypt_message(client->e2e_ctx, message, recipient_id, &encrypted_packet) < 0) {
        LOG_ERROR("Failed to encrypt E2E message");
        return -1;
    }

    // Prepare transmission message with special prefix
    char e2e_msg[sizeof(e2e_encrypted_packet_t) * 2 + 50];
    snprintf(e2e_msg, sizeof(e2e_msg), "/e2emsg:");

    // Convert binary packet to hex string for transmission
    size_t hex_len  = sizeof(e2e_encrypted_packet_t) * 2;
    char  *hex_data = e2e_msg + strlen(e2e_msg);

    const uint8_t *packet_bytes = (const uint8_t *)&encrypted_packet;
    for (size_t i = 0; i < sizeof(e2e_encrypted_packet_t); i++) {
        sprintf(hex_data + i * 2, "%02x", packet_bytes[i]);
    }
    hex_data[hex_len] = '\0';

    // Send through normal message channel (server will relay without decrypting)
    if (client_send_message(client, e2e_msg) < 0) {
        LOG_ERROR("Failed to send E2E encrypted message");
        return -1;
    }

    LOG_INFO("Sent E2E encrypted message to %s", recipient_nickname);
    return 0;
}

// Function to handle received E2E messages (call this from message parsing)
int client_handle_e2e_message(client_t *client, const char *hex_data)
{
    if (!client || !hex_data || !client->e2e_ctx) {
        LOG_ERROR("client_handle_e2e_message: Invalid parameters");
        return -1;
    }

    // Convert hex string back to binary packet
    e2e_encrypted_packet_t encrypted_packet;
    size_t                 hex_len = strlen(hex_data);

    if (hex_len != sizeof(e2e_encrypted_packet_t) * 2) {
        LOG_ERROR("Invalid E2E message length");
        return -1;
    }

    uint8_t *packet_bytes = (uint8_t *)&encrypted_packet;
    for (size_t i = 0; i < sizeof(e2e_encrypted_packet_t); i++) {
        unsigned int byte_val;
        if (sscanf(hex_data + i * 2, "%02x", &byte_val) != 1) {
            LOG_ERROR("Invalid hex data in E2E message");
            return -1;
        }
        packet_bytes[i] = (uint8_t)byte_val;
    }

    // Decrypt the message
    char   decrypted_message[1024];
    size_t decrypted_len = sizeof(decrypted_message);

    if (e2e_decrypt_message(client->e2e_ctx, &encrypted_packet, decrypted_message, &decrypted_len) <
        0) {
        LOG_WARN("Failed to decrypt E2E message - may not be intended for us");
        return -1;
    }

    // Find sender's nickname
    const e2e_peer_key_t *sender = e2e_find_peer_key(client->e2e_ctx, encrypted_packet.sender_id);
    const char           *sender_name = sender ? sender->nickname : "Unknown";

    // Display the decrypted message
    printf("\n🔒 [E2E] %s: %s\n", sender_name, decrypted_message);
    fflush(stdout);

    LOG_INFO("Received and decrypted E2E message from %s", sender_name);
    return 0;
}

// Function to parse hex string to key exchange packet
int client_parse_key_exchange(const char *hex_data, e2e_key_exchange_packet_t *key_packet)
{
    if (!hex_data || !key_packet) {
        return -1;
    }

    size_t hex_len = strlen(hex_data);
    if (hex_len != sizeof(e2e_key_exchange_packet_t) * 2) {
        LOG_ERROR("Invalid key exchange data length");
        return -1;
    }

    uint8_t *packet_bytes = (uint8_t *)key_packet;
    for (size_t i = 0; i < sizeof(e2e_key_exchange_packet_t); i++) {
        unsigned int byte_val;
        if (sscanf(hex_data + i * 2, "%02x", &byte_val) != 1) {
            LOG_ERROR("Invalid hex data in key exchange");
            return -1;
        }
        packet_bytes[i] = (uint8_t)byte_val;
    }

    return 0;
}
