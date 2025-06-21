#ifndef ENCRYPTION_CONTEXT_H
#define ENCRYPTION_CONTEXT_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// AES-256-GCM context
typedef struct {
    uint8_t  key[32];
    uint8_t  iv[16];
    uint64_t counter;
    bool     initialized;
} aes256_gcm_context_t;

// ChaCha20-Poly1305 context
typedef struct {
    uint8_t  key[32];
    uint8_t  nonce[12];
    uint64_t counter;
    bool     initialized;
} chacha20_poly1305_context_t;

// Encryption context
typedef struct {
    aes256_gcm_context_t        primary_cipher;
    chacha20_poly1305_context_t backup_cipher;
    uint8_t                     session_key[32];      // Current session key
    uint8_t                     message_key[32];      // Current message key
    uint64_t                    key_rotation_counter; // Auto-rotation trigger
    uint32_t                    sequence_number;      // Message ordering
    pthread_mutex_t             crypto_mutex;
} encryption_context_t;

// Function declarations
int init_encryption_context(encryption_context_t *enc_ctx, const uint8_t *key);
int encrypt_data(encryption_context_t *enc_ctx, const uint8_t *plaintext, size_t plaintext_len,
                 uint8_t *ciphertext, size_t *ciphertext_len);
int decrypt_data(encryption_context_t *enc_ctx, const uint8_t *ciphertext, size_t ciphertext_len,
                 uint8_t *plaintext, size_t *plaintext_len);

#endif // ENCRYPTION_CONTEXT_H
