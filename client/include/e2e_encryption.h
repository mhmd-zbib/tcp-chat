#ifndef E2E_ENCRYPTION_H
#define E2E_ENCRYPTION_H

#include <stdint.h>
#include <stdlib.h>

// Simple RSA-like key sizes (for demonstration - in production use proper crypto libraries)
#define E2E_KEY_SIZE         32
#define E2E_PUBLIC_KEY_SIZE  32
#define E2E_PRIVATE_KEY_SIZE 32
#define E2E_MAX_MESSAGE_SIZE 1024
#define MAX_CLIENTS_E2E      64

// Key pair structure
typedef struct {
    uint8_t public_key[E2E_PUBLIC_KEY_SIZE];
    uint8_t private_key[E2E_PRIVATE_KEY_SIZE];
    uint8_t user_id[16]; // Unique identifier
    char    nickname[64];
} e2e_keypair_t;

// Public key storage for other users
typedef struct {
    uint8_t public_key[E2E_PUBLIC_KEY_SIZE];
    uint8_t user_id[16];
    char    nickname[64];
    int     active;
} e2e_peer_key_t;

// E2E encryption context
typedef struct {
    e2e_keypair_t  my_keys;
    e2e_peer_key_t peer_keys[MAX_CLIENTS_E2E];
    int            num_peers;
    uint8_t        session_entropy[64]; // For session-based encryption
} e2e_context_t;

// Encrypted message packet
typedef struct {
    uint8_t sender_id[16];
    uint8_t recipient_id[16];
    uint8_t encrypted_data[E2E_MAX_MESSAGE_SIZE];
    size_t  encrypted_length;
    uint8_t signature[32]; // Message authentication
} e2e_encrypted_packet_t;

// Key exchange packet
typedef struct {
    uint8_t user_id[16];
    uint8_t public_key[E2E_PUBLIC_KEY_SIZE];
    char    nickname[64];
    uint8_t signature[32]; // Self-signed
} e2e_key_exchange_packet_t;

// Function declarations
int  e2e_context_init(e2e_context_t *ctx, const char *nickname);
void e2e_context_cleanup(e2e_context_t *ctx);

// Key management
int e2e_generate_keypair(e2e_keypair_t *keypair, const char *nickname);
int e2e_add_peer_key(e2e_context_t *ctx, const e2e_key_exchange_packet_t *key_packet);
int e2e_remove_peer_key(e2e_context_t *ctx, const uint8_t *user_id);
const e2e_peer_key_t *e2e_find_peer_key(const e2e_context_t *ctx, const uint8_t *user_id);

// Encryption/Decryption
int e2e_encrypt_message(e2e_context_t *ctx, const char *plaintext, const uint8_t *recipient_id,
                        e2e_encrypted_packet_t *packet);
int e2e_decrypt_message(e2e_context_t *ctx, const e2e_encrypted_packet_t *packet, char *plaintext,
                        size_t *plaintext_len);

// Key exchange
int e2e_create_key_exchange_packet(const e2e_context_t *ctx, e2e_key_exchange_packet_t *packet);
int e2e_verify_key_exchange_packet(const e2e_key_exchange_packet_t *packet);

// Utility functions
int  e2e_generate_user_id(uint8_t *user_id, const char *nickname);
void e2e_print_key(const uint8_t *key, size_t key_len, const char *label);

#endif // E2E_ENCRYPTION_H
