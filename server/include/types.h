#ifndef TYPES_H
#define TYPES_H
#define MAX_CLIENTS          100
#define BUFFER_SIZE          1024
#define MAX_NICKNAME_LEN     32
#define MAX_ROOMS            50
#define MAX_ROOM_NAME        64
#define ROOM_ID_LENGTH       6
#define MAX_CLIENTS_PER_ROOM 20

typedef enum {
    CLIENT_STATE_CONNECTED = 0,
    CLIENT_STATE_IN_LOBBY  = 1,
    CLIENT_STATE_IN_ROOM   = 2
} client_state_t;

#endif
