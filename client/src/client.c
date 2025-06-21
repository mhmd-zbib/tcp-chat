#include "../include/client.h"
#include "../../utils/include/logger.h"
#include "../include/handshake_protocol.h"
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

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr) <= 0) {
        LOG_ERROR("client_create: Invalid server IP address: %s", server_ip);
        free(client);
        return NULL;
    }

    LOG_INFO("Client created for server %s:%d with nickname '%s'", server_ip, server_port,
             nickname);

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

    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        LOG_ERRNO("client_connect_to_server: Failed to create socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(client->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARN("client_connect_to_server: Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    LOG_CONNECTION("Connecting to server %s:%d...", client->server_ip, client->server_port);

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

        // Send a quit message to the server if still connected
        const char *quit_msg = "/quit";
        send(client->socket_fd, quit_msg, strlen(quit_msg), MSG_NOSIGNAL);

        // Give the server a moment to process the quit message
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

    client_disconnect(client);
    free(client);
    LOG_INFO("Client resources released");
}

int client_send_message(client_t *client, const char *message)
{
    if (!client || !message || !client->connected) {
        LOG_ERROR("client_send_message: Invalid parameters or not connected");
        return -1;
    }

    size_t  len        = strlen(message);
    ssize_t bytes_sent = send(client->socket_fd, message, len, 0);
    if (bytes_sent < 0) {
        LOG_ERRNO("client_send_message: Failed to send message");
        return -1;
    } else if ((size_t)bytes_sent < len) {
        LOG_WARN("client_send_message: Partial message sent: %zd/%zu bytes", bytes_sent, len);
        return -1;
    }

    LOG_DEBUG("Sent message: %s", message);
    return 0;
}

int client_receive_message(client_t *client, char *buffer, size_t buffer_size)
{
    if (!client || !buffer || buffer_size == 0 || !client->connected) {
        LOG_ERROR("client_receive_message: Invalid parameters or not connected");
        return -1;
    }

    ssize_t bytes_received = recv(client->socket_fd, buffer, buffer_size - 1, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            LOG_CONNECTION("Server disconnected");
            client->connected = 0;
        } else {
            // Check if this is a timeout (which is expected)
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0; // Timeout, not an error
            } else {
                LOG_ERRNO("client_receive_message: Error receiving data");
            }
        }
        return -1;
    }

    buffer[bytes_received] = '\0';
    return (int)bytes_received;
}
