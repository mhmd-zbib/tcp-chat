#include "../include/handshake_protocol.h"
#include "../include/client_session.h"
#include "../include/types.h"
#include "logger.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
uint32_t handshake_generate_sequence_number(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec ^ tv.tv_usec) ^ getpid();
}
int handshake_send_packet(int socket_fd, const handshake_packet_t *packet)
{
    if (!packet) {
        LOG_ERROR("handshake_send_packet: packet is NULL");
        return -1;
    }

    ssize_t bytes_sent = send(socket_fd, packet, sizeof(handshake_packet_t), MSG_NOSIGNAL);
    if (bytes_sent != sizeof(handshake_packet_t)) {
        if (bytes_sent < 0) {
            LOG_ERRNO("handshake_send_packet: Failed to send packet");
        } else {
            LOG_ERROR("handshake_send_packet: Partial packet sent: %zd/%zu bytes", bytes_sent,
                      sizeof(handshake_packet_t));
        }
        return -1;
    }

    LOG_PROTOCOL("Sent %s packet (seq=%u, ack=%u) on socket %d",
                 packet->type == HANDSHAKE_TYPE_SYN       ? "SYN"
                 : packet->type == HANDSHAKE_TYPE_SYN_ACK ? "SYN-ACK"
                                                          : "ACK",
                 packet->seq_num, packet->ack_num, socket_fd);
    return 0;
}
int handshake_receive_packet(int socket_fd, handshake_packet_t *packet)
{
    if (!packet) {
        LOG_ERROR("handshake_receive_packet: packet is NULL");
        return -1;
    }
    struct timeval timeout;
    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_WARN("handshake_receive_packet: Failed to set socket timeout: %s", strerror(errno));
    }
    ssize_t bytes_received = recv(socket_fd, packet, sizeof(handshake_packet_t), 0);
    if (bytes_received != sizeof(handshake_packet_t)) {
        if (bytes_received < 0) {
            LOG_ERRNO("handshake_receive_packet: Failed to receive packet");
        } else if (bytes_received == 0) {
            LOG_WARN("handshake_receive_packet: Connection closed by peer");
        } else {
            LOG_ERROR("handshake_receive_packet: Partial packet received: %zd/%zu bytes",
                      bytes_received, sizeof(handshake_packet_t));
        }
        return -1;
    }

    LOG_PROTOCOL("Received %s packet (seq=%u, ack=%u) on socket %d",
                 packet->type == HANDSHAKE_TYPE_SYN       ? "SYN"
                 : packet->type == HANDSHAKE_TYPE_SYN_ACK ? "SYN-ACK"
                                                          : "ACK",
                 packet->seq_num, packet->ack_num, socket_fd);
    return 0;
}
static int handshake_reset_socket_timeout(int socket_fd)
{
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_WARN("handshake_reset_socket_timeout: Failed to reset socket timeout: %s",
                 strerror(errno));
        return -1;
    }
    return 0;
}
int handshake_perform_server_side(client_session_t *client)
{
    if (!client) {
        LOG_ERROR("handshake_perform_server_side: client is NULL");
        return -1;
    }

    LOG_HANDSHAKE("Starting 3-way handshake for client on socket %d", client->socket_fd);
    client->handshake_state = HANDSHAKE_IDLE;

    // Step 1: Receive SYN from client
    handshake_packet_t syn_packet = {0};
    LOG_HANDSHAKE("Step 1: Waiting for SYN packet from client on socket %d", client->socket_fd);
    if (handshake_receive_packet(client->socket_fd, &syn_packet) < 0) {
        LOG_ERROR("Handshake failed: Could not receive SYN packet from socket %d",
                  client->socket_fd);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }

    if (syn_packet.type != HANDSHAKE_TYPE_SYN) {
        LOG_ERROR("Handshake failed: Invalid packet type (expected SYN, got %d) from socket %d",
                  syn_packet.type, client->socket_fd);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }

    // Store client's sequence number and set nickname
    uint32_t client_seq     = syn_packet.seq_num;
    client->handshake_state = HANDSHAKE_SYN_RECEIVED;

    if (strlen(syn_packet.nickname) > 0 && strlen(syn_packet.nickname) < MAX_NICKNAME_LEN) {
        client_session_set_nickname(client, syn_packet.nickname);
    }

    // Step 2: Send SYN-ACK to client
    handshake_packet_t syn_ack_packet = {0};
    syn_ack_packet.type               = HANDSHAKE_TYPE_SYN_ACK;
    syn_ack_packet.seq_num            = handshake_generate_sequence_number();
    syn_ack_packet.ack_num            = client_seq + 1; // Acknowledge client's SYN
    strncpy(syn_ack_packet.nickname, "SERVER", MAX_NICKNAME_LEN - 1);

    LOG_HANDSHAKE("Step 2: Sending SYN-ACK packet to client '%s' on socket %d", client->nickname,
                  client->socket_fd);
    if (handshake_send_packet(client->socket_fd, &syn_ack_packet) < 0) {
        LOG_ERROR("Handshake failed: Could not send SYN-ACK packet to socket %d",
                  client->socket_fd);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }

    // Step 3: Receive ACK from client
    handshake_packet_t ack_packet = {0};
    LOG_HANDSHAKE("Step 3: Waiting for ACK packet from client '%s' on socket %d", client->nickname,
                  client->socket_fd);
    if (handshake_receive_packet(client->socket_fd, &ack_packet) < 0) {
        LOG_ERROR("Handshake failed: Could not receive ACK packet from socket %d",
                  client->socket_fd);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }

    if (ack_packet.type != HANDSHAKE_TYPE_ACK) {
        LOG_ERROR("Handshake failed: Invalid packet type (expected ACK, got %d) from socket %d",
                  ack_packet.type, client->socket_fd);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }

    // Verify ACK number matches our sequence + 1
    if (ack_packet.ack_num != syn_ack_packet.seq_num + 1) {
        LOG_ERROR("Handshake failed: Invalid ACK number (expected %u, got %u) from socket %d",
                  syn_ack_packet.seq_num + 1, ack_packet.ack_num, client->socket_fd);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }

    // Handshake completed successfully
    client->sequence_number = syn_ack_packet.seq_num;
    client->handshake_state = HANDSHAKE_ESTABLISHED;
    handshake_reset_socket_timeout(client->socket_fd);

    LOG_HANDSHAKE("3-way handshake completed successfully with client '%s' on socket %d",
                  client->nickname, client->socket_fd);
    return 0;
}
