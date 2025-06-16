#include "../include/handshake_protocol.h"
#include "../include/client_session.h"
#include "../include/types.h"
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
    printf("handshake_send_packet: Sent %s packet (seq=%u)\n",
           packet->type == HANDSHAKE_TYPE_SYN ? "SYN" : "ACK", packet->seq_num);
    return 0;
}
int handshake_receive_packet(int socket_fd, handshake_packet_t *packet)
{
    if (!packet) {
        fprintf(stderr, "handshake_receive_packet: packet is NULL\n");
        return -1;
    }
    struct timeval timeout;
    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "handshake_receive_packet: Failed to set socket timeout: %s\n",
                strerror(errno));
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
    printf("handshake_receive_packet: Received %s packet (seq=%u)\n",
           packet->type == HANDSHAKE_TYPE_SYN ? "SYN" : "ACK", packet->seq_num);
    return 0;
}
static int handshake_reset_socket_timeout(int socket_fd)
{
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr,
                "handshake_reset_socket_timeout: Warning: Failed to reset socket timeout: %s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}
int handshake_perform_server_side(client_session_t *client)
{
    if (!client) {
        fprintf(stderr, "handshake_perform_server_side: client is NULL\n");
        return -1;
    }
    printf("=== Starting Server-side 2-Way Handshake ===\n");
    printf("handshake_perform_server_side: Handling handshake for client on socket %d\n",
           client->socket_fd);
    client->handshake_state = HANDSHAKE_IDLE;
    handshake_packet_t syn_packet = {0};
    printf("handshake_perform_server_side: Step 1: Waiting for SYN from client...\n");
    if (handshake_receive_packet(client->socket_fd, &syn_packet) < 0) {
        fprintf(stderr, "handshake_perform_server_side: Failed to receive SYN packet\n");
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }
    if (syn_packet.type != HANDSHAKE_TYPE_SYN) {
        fprintf(stderr,
                "handshake_perform_server_side: Invalid packet type: expected SYN (%d), got %d\n",
                HANDSHAKE_TYPE_SYN, syn_packet.type);
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }
    client->sequence_number = syn_packet.seq_num;
    client->handshake_state = HANDSHAKE_SYN_SENT;
    if (strlen(syn_packet.nickname) > 0 && strlen(syn_packet.nickname) < MAX_NICKNAME_LEN) {
        client_session_set_nickname(client, syn_packet.nickname);
        printf("handshake_perform_server_side: Step 1: Received SYN (seq=%u) with nickname '%s'\n",
               syn_packet.seq_num, client->nickname);
    } else {
        printf(
            "handshake_perform_server_side: Step 1: Received SYN (seq=%u) with default nickname\n",
            syn_packet.seq_num);
    }
    handshake_packet_t ack_packet = {0};
    ack_packet.type               = HANDSHAKE_TYPE_ACK;
    ack_packet.seq_num            = handshake_generate_sequence_number();
    strncpy(ack_packet.nickname, "SERVER", MAX_NICKNAME_LEN - 1);
    printf("handshake_perform_server_side: Step 2: Sending ACK (seq=%u)\n", ack_packet.seq_num);
    if (handshake_send_packet(client->socket_fd, &ack_packet) < 0) {
        fprintf(stderr, "handshake_perform_server_side: Failed to send ACK packet\n");
        client->handshake_state = HANDSHAKE_FAILED;
        return -1;
    }
    client->handshake_state = HANDSHAKE_ESTABLISHED;
    handshake_reset_socket_timeout(client->socket_fd);
    printf("handshake_perform_server_side: Step 2: Sent ACK (seq=%u)\n", ack_packet.seq_num);
    printf("=== 2-Way Handshake Completed Successfully ===\n");
    printf("handshake_perform_server_side: Connection established with client '%s'\n",
           client->nickname);
    return 0;
}
