#ifndef HANDSHAKE_PROTOCOL_H
#define HANDSHAKE_PROTOCOL_H

#include <stdint.h>

#ifndef MAX_NICKNAME_LEN
#include "types.h"
#endif

typedef enum {
    HANDSHAKE_IDLE,
    HANDSHAKE_SYN_SENT,
    HANDSHAKE_SYN_RECEIVED,
    HANDSHAKE_ESTABLISHED,
    HANDSHAKE_FAILED
} handshake_state_t;

typedef struct {
    uint8_t  type;
    uint32_t seq_num;
    uint32_t ack_num;
    char     nickname[MAX_NICKNAME_LEN];
    uint8_t  padding[7];
} handshake_packet_t;

#define HANDSHAKE_TYPE_SYN     0
#define HANDSHAKE_TYPE_SYN_ACK 1
#define HANDSHAKE_TYPE_ACK     2

uint32_t handshake_generate_sequence_number(void);
int      handshake_send_packet(int socket_fd, const handshake_packet_t *packet);
int      handshake_receive_packet(int socket_fd, handshake_packet_t *packet);
int      handshake_perform_client_side(int socket_fd, const char *nickname);

#endif
