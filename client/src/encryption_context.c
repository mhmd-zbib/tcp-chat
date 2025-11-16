#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/encryption_context.h"
#include "../../utils/include/logger.h"
#include "../include/global_security.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External function declarations are now in global_security.h

// Static function declarations
static void perform_xor_encryption(encryption_context_t *enc_ctx, const uint8_t *plaintext,
                                   size_t len, uint8_t *ciphertext);
static void perform_xor_decryption(encryption_context_t *enc_ctx, const uint8_t *ciphertext,
                                   size_t len, uint8_t *plaintext);
static void add_authentication_tag(const uint8_t *plaintext, size_t len, uint8_t *ciphertext);
static int  verify_authentication_tag(const uint8_t *plaintext, size_t len, const uint8_t *tag);
static void check_key_rotation(encryption_context_t *enc_ctx);

int init_encryption_context(encryption_context_t *enc_ctx, const uint8_t *key)
{
    if (!enc_ctx || !key) {
        return -1;
    }

    if (pthread_mutex_init(&enc_ctx->crypto_mutex, NULL) != 0) {
        return -1;
    }

    pthread_mutex_lock(&enc_ctx->crypto_mutex);

    memcpy(enc_ctx->primary_cipher.key, key, 32);
    memset(enc_ctx->primary_cipher.iv, 0, 16);
    enc_ctx->primary_cipher.counter     = 0;
    enc_ctx->primary_cipher.initialized = true;

    memcpy(enc_ctx->backup_cipher.key, key, 32);
    memset(enc_ctx->backup_cipher.nonce, 0, 12);
    enc_ctx->backup_cipher.counter     = 0;
    enc_ctx->backup_cipher.initialized = true;

    memcpy(enc_ctx->session_key, key, 32);
    memcpy(enc_ctx->message_key, key, 32);

    enc_ctx->key_rotation_counter = 0;
    enc_ctx->sequence_number      = 0;

    pthread_mutex_unlock(&enc_ctx->crypto_mutex);

    LOG_DEBUG("Encryption context initialized");
    return 0;
}

int encrypt_data(encryption_context_t *enc_ctx, const uint8_t *plaintext, size_t plaintext_len,
                 uint8_t *ciphertext, size_t *ciphertext_len)
{
    if (!enc_ctx || !plaintext || !ciphertext || !ciphertext_len) {
        return -1;
    }

    pthread_mutex_lock(&enc_ctx->crypto_mutex);

    // TODO: Replace with AES-256-GCM for production
    if (*ciphertext_len < plaintext_len + 16) {
        pthread_mutex_unlock(&enc_ctx->crypto_mutex);
        return -1;
    }

    perform_xor_encryption(enc_ctx, plaintext, plaintext_len, ciphertext);
    add_authentication_tag(plaintext, plaintext_len, ciphertext);

    *ciphertext_len = plaintext_len + 16;

    enc_ctx->sequence_number++;
    enc_ctx->key_rotation_counter++;

    check_key_rotation(enc_ctx);

    pthread_mutex_unlock(&enc_ctx->crypto_mutex);

    update_global_performance_counters("encrypt");
    return 0;
}

int decrypt_data(encryption_context_t *enc_ctx, const uint8_t *ciphertext, size_t ciphertext_len,
                 uint8_t *plaintext, size_t *plaintext_len)
{
    if (!enc_ctx || !ciphertext || !plaintext || !plaintext_len) {
        return -1;
    }

    if (ciphertext_len < 16) {
        return -1;
    }

    pthread_mutex_lock(&enc_ctx->crypto_mutex);

    size_t data_len = ciphertext_len - 16;

    if (*plaintext_len < data_len) {
        pthread_mutex_unlock(&enc_ctx->crypto_mutex);
        return -1;
    }

    perform_xor_decryption(enc_ctx, ciphertext, data_len, plaintext);

    if (verify_authentication_tag(plaintext, data_len, ciphertext + data_len) != 0) {
        LOG_ERROR("Authentication tag verification failed");
        pthread_mutex_unlock(&enc_ctx->crypto_mutex);
        return -1;
    }

    *plaintext_len = data_len;
    enc_ctx->sequence_number++;

    pthread_mutex_unlock(&enc_ctx->crypto_mutex);

    update_global_performance_counters("decrypt");
    return 0;
}

static void perform_xor_encryption(encryption_context_t *enc_ctx, const uint8_t *plaintext,
                                   size_t len, uint8_t *ciphertext)
{
    for (size_t i = 0; i < len; i++) {
        ciphertext[i] = plaintext[i] ^ enc_ctx->session_key[i % 32];
    }
}

static void perform_xor_decryption(encryption_context_t *enc_ctx, const uint8_t *ciphertext,
                                   size_t len, uint8_t *plaintext)
{
    for (size_t i = 0; i < len; i++) {
        plaintext[i] = ciphertext[i] ^ enc_ctx->session_key[i % 32];
    }
}

static void add_authentication_tag(const uint8_t *plaintext, size_t len, uint8_t *ciphertext)
{
    uint8_t tag[16] = {0};
    for (size_t i = 0; i < len; i++) {
        tag[i % 16] ^= plaintext[i];
    }
    memcpy(ciphertext + len, tag, 16);
}

static int verify_authentication_tag(const uint8_t *plaintext, size_t len, const uint8_t *tag)
{
    uint8_t expected_tag[16] = {0};
    for (size_t i = 0; i < len; i++) {
        expected_tag[i % 16] ^= plaintext[i];
    }
    return memcmp(tag, expected_tag, 16);
}

static void check_key_rotation(encryption_context_t *enc_ctx)
{
    // TODO: Implement proper key rotation schedule
    if (enc_ctx->key_rotation_counter >= 1000) {
        for (int i = 0; i < 32; i++) {
            enc_ctx->session_key[i] = (enc_ctx->session_key[i] + 1) & 0xFF;
        }
        enc_ctx->key_rotation_counter = 0;
        LOG_DEBUG("Encryption key rotated");
    }
}
