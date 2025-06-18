#include "../include/handshake_protocol.h"
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
        fprintf(stderr, "handshake_send_packet: packet is NULL\n");
        return -1;
    }

    ssize_t bytes_sent = send(socket_fd, packet, sizeof(handshake_packet_t), MSG_NOSIGNAL);
    if (bytes_sent != sizeof(handshake_packet_t)) {
        if (bytes_sent < 0) {
            fprintf(stderr, "handshake_send_packet: Failed to send packet: %s\n", strerror(errno));
        } else {
            fprintf(stderr, "handshake_send_packet: Partial packet sent: %zd/%zu bytes\n",
                    bytes_sent, sizeof(handshake_packet_t));
        }
        return -1;
    }

    printf("handshake_send_packet: Sent %s packet (seq=%u, ack=%u)\n",
           packet->type == HANDSHAKE_TYPE_SYN       ? "SYN"
           : packet->type == HANDSHAKE_TYPE_SYN_ACK ? "SYN-ACK"
                                                    : "ACK",
           packet->seq_num, packet->ack_num);
    return 0;
}

int handshake_receive_packet(int socket_fd, handshake_packet_t *packet)
{
    if (!packet) {
        fprintf(stderr, "handshake_receive_packet: packet is NULL\n");
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "handshake_receive_packet: Failed to set socket timeout: %s\n",
                strerror(errno));
        return -1;
    }

    ssize_t bytes_received = recv(socket_fd, packet, sizeof(handshake_packet_t), 0);
    if (bytes_received != sizeof(handshake_packet_t)) {
        if (bytes_received < 0) {
            fprintf(stderr, "handshake_receive_packet: Failed to receive packet: %s\n",
                    strerror(errno));
        } else if (bytes_received == 0) {
            fprintf(stderr, "handshake_receive_packet: Connection closed by peer\n");
        } else {
            fprintf(stderr, "handshake_receive_packet: Partial packet received: %zd/%zu bytes\n",
                    bytes_received, sizeof(handshake_packet_t));
        }
        return -1;
    }

    printf("handshake_receive_packet: Received %s packet (seq=%u, ack=%u)\n",
           packet->type == HANDSHAKE_TYPE_SYN       ? "SYN"
           : packet->type == HANDSHAKE_TYPE_SYN_ACK ? "SYN-ACK"
                                                    : "ACK",
           packet->seq_num, packet->ack_num);
    return 0;
}

int handshake_perform_client_side(int socket_fd, const char *nickname)
{
    if (!nickname) {
        fprintf(stderr, "handshake_perform_client_side: nickname is NULL\n");
        return -1;
    }

    printf("Starting 3-way handshake with server...\n");

    // Step 1: Send SYN to server
    handshake_packet_t syn_packet = {0};
    syn_packet.type               = HANDSHAKE_TYPE_SYN;
    syn_packet.seq_num            = handshake_generate_sequence_number();
    syn_packet.ack_num            = 0; // No acknowledgment in SYN
    strncpy(syn_packet.nickname, nickname, MAX_NICKNAME_LEN - 1);
    syn_packet.nickname[MAX_NICKNAME_LEN - 1] = '\0';

    printf("Step 1: Sending SYN packet with nickname '%s'\n", nickname);
    if (handshake_send_packet(socket_fd, &syn_packet) < 0) {
        fprintf(stderr, "handshake_perform_client_side: Failed to send SYN packet\n");
        return -1;
    }

    // Step 2: Receive SYN-ACK from server
    handshake_packet_t syn_ack_packet = {0};
    printf("Step 2: Waiting for SYN-ACK packet from server\n");
    if (handshake_receive_packet(socket_fd, &syn_ack_packet) < 0) {
        fprintf(stderr, "handshake_perform_client_side: Failed to receive SYN-ACK packet\n");
        return -1;
    }

    if (syn_ack_packet.type != HANDSHAKE_TYPE_SYN_ACK) {
        fprintf(stderr, "handshake_perform_client_side: Expected SYN-ACK packet, got type %d\n",
                syn_ack_packet.type);
        return -1;
    }

    // Verify that server acknowledged our SYN
    if (syn_ack_packet.ack_num != syn_packet.seq_num + 1) {
        fprintf(stderr, "handshake_perform_client_side: Invalid ACK number (expected %u, got %u)\n",
                syn_packet.seq_num + 1, syn_ack_packet.ack_num);
        return -1;
    }

    // Step 3: Send ACK to server
    handshake_packet_t ack_packet = {0};
    ack_packet.type               = HANDSHAKE_TYPE_ACK;
    ack_packet.seq_num            = syn_packet.seq_num + 1;     // Next sequence number
    ack_packet.ack_num            = syn_ack_packet.seq_num + 1; // Acknowledge server's SYN
    strncpy(ack_packet.nickname, nickname, MAX_NICKNAME_LEN - 1);
    ack_packet.nickname[MAX_NICKNAME_LEN - 1] = '\0';

    printf("Step 3: Sending ACK packet to complete handshake\n");
    if (handshake_send_packet(socket_fd, &ack_packet) < 0) {
        fprintf(stderr, "handshake_perform_client_side: Failed to send ACK packet\n");
        return -1;
    }

    printf("3-way handshake completed successfully!\n");
    printf("Connected to server with nickname: %s\n", nickname);

    return 0;
}
