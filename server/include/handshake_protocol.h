#ifndef HANDSHAKE_PROTOCOL_H
#define HANDSHAKE_PROTOCOL_H
#include <stdint.h>
#ifndef MAX_NICKNAME_LEN
#include "types.h"
#endif
typedef struct client_session_s client_session_t;
typedef enum {
    HANDSHAKE_IDLE,
    HANDSHAKE_SYN_SENT,
    HANDSHAKE_ESTABLISHED,
    HANDSHAKE_FAILED
} handshake_state_t;
typedef struct {
    uint8_t  type;
    uint32_t seq_num;
    char     nickname[MAX_NICKNAME_LEN];
    uint8_t  padding[3];
} handshake_packet_t;
#define HANDSHAKE_TYPE_SYN 0
#define HANDSHAKE_TYPE_ACK 1
uint32_t handshake_generate_sequence_number(void);
int      handshake_send_packet(int socket_fd, const handshake_packet_t *packet);
int      handshake_receive_packet(int socket_fd, handshake_packet_t *packet);
int      handshake_perform_server_side(client_session_t *client);
#endif