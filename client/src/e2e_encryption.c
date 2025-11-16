#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/e2e_encryption.h"
#include "../../utils/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Simple XOR-based encryption for demonstration
// In production, use proper asymmetric crypto like RSA, ECC, or post-quantum algorithms

static void simple_encrypt(const uint8_t *key, const uint8_t *plaintext, size_t len,
                           uint8_t *ciphertext);
static void simple_decrypt(const uint8_t *key, const uint8_t *ciphertext, size_t len,
                           uint8_t *plaintext);
static void generate_random_bytes(uint8_t *buffer, size_t len);
static void derive_shared_secret(const uint8_t *my_private, const uint8_t *peer_public,
                                 uint8_t *shared_secret);

int e2e_context_init(e2e_context_t *ctx, const char *nickname)
{
    if (!ctx || !nickname) {
        return -1;
    }

    memset(ctx, 0, sizeof(e2e_context_t));

    // Generate our keypair
    if (e2e_generate_keypair(&ctx->my_keys, nickname) < 0) {
        LOG_ERROR("Failed to generate E2E keypair");
        return -1;
    }

    // Initialize session entropy
    generate_random_bytes(ctx->session_entropy, sizeof(ctx->session_entropy));

    ctx->num_peers = 0;

    LOG_INFO("E2E encryption context initialized for user: %s", nickname);
    e2e_print_key(ctx->my_keys.public_key, E2E_PUBLIC_KEY_SIZE, "My Public Key");

    return 0;
}

void e2e_context_cleanup(e2e_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    // Securely wipe private key
    memset(ctx->my_keys.private_key, 0, E2E_PRIVATE_KEY_SIZE);
    memset(ctx->session_entropy, 0, sizeof(ctx->session_entropy));
    memset(ctx, 0, sizeof(e2e_context_t));

    LOG_INFO("E2E encryption context cleaned up");
}

int e2e_generate_keypair(e2e_keypair_t *keypair, const char *nickname)
{
    if (!keypair || !nickname) {
        return -1;
    }

    // Generate user ID from nickname
    if (e2e_generate_user_id(keypair->user_id, nickname) < 0) {
        return -1;
    }

    // Generate random private key
    generate_random_bytes(keypair->private_key, E2E_PRIVATE_KEY_SIZE);

    // Generate public key (simplified - in real crypto this would be mathematically derived)
    // For demo: public_key = hash(private_key + user_id)
    uint8_t temp[E2E_PRIVATE_KEY_SIZE + 16];
    memcpy(temp, keypair->private_key, E2E_PRIVATE_KEY_SIZE);
    memcpy(temp + E2E_PRIVATE_KEY_SIZE, keypair->user_id, 16);

    // Simple hash function for public key generation
    for (int i = 0; i < E2E_PUBLIC_KEY_SIZE; i++) {
        keypair->public_key[i] = temp[i] ^ temp[i + 16] ^ (uint8_t)(i * 17 + 42);
    }

    strncpy(keypair->nickname, nickname, sizeof(keypair->nickname) - 1);
    keypair->nickname[sizeof(keypair->nickname) - 1] = '\0';

    LOG_DEBUG("Generated E2E keypair for: %s", nickname);
    return 0;
}

int e2e_add_peer_key(e2e_context_t *ctx, const e2e_key_exchange_packet_t *key_packet)
{
    if (!ctx || !key_packet || ctx->num_peers >= MAX_CLIENTS_E2E) {
        return -1;
    }

    // Verify the key exchange packet first
    if (e2e_verify_key_exchange_packet(key_packet) < 0) {
        LOG_WARN("Invalid key exchange packet - signature verification failed");
        return -1;
    }

    // Check if we already have this peer
    for (int i = 0; i < ctx->num_peers; i++) {
        if (memcmp(ctx->peer_keys[i].user_id, key_packet->user_id, 16) == 0) {
            // Update existing peer
            memcpy(ctx->peer_keys[i].public_key, key_packet->public_key, E2E_PUBLIC_KEY_SIZE);
            strncpy(ctx->peer_keys[i].nickname, key_packet->nickname,
                    sizeof(ctx->peer_keys[i].nickname) - 1);
            ctx->peer_keys[i].nickname[sizeof(ctx->peer_keys[i].nickname) - 1] = '\0';
            ctx->peer_keys[i].active                                           = 1;

            LOG_INFO("Updated E2E peer key for: %s", key_packet->nickname);
            return 0;
        }
    }

    // Add new peer
    e2e_peer_key_t *peer = &ctx->peer_keys[ctx->num_peers];
    memcpy(peer->user_id, key_packet->user_id, 16);
    memcpy(peer->public_key, key_packet->public_key, E2E_PUBLIC_KEY_SIZE);
    strncpy(peer->nickname, key_packet->nickname, sizeof(peer->nickname) - 1);
    peer->nickname[sizeof(peer->nickname) - 1] = '\0';
    peer->active                               = 1;

    ctx->num_peers++;

    LOG_INFO("Added E2E peer key for: %s (total peers: %d)", key_packet->nickname, ctx->num_peers);
    e2e_print_key(peer->public_key, E2E_PUBLIC_KEY_SIZE, peer->nickname);

    return 0;
}

int e2e_remove_peer_key(e2e_context_t *ctx, const uint8_t *user_id)
{
    if (!ctx || !user_id) {
        return -1;
    }

    for (int i = 0; i < ctx->num_peers; i++) {
        if (memcmp(ctx->peer_keys[i].user_id, user_id, 16) == 0) {
            // Mark as inactive instead of removing to avoid array shifts
            ctx->peer_keys[i].active = 0;
            memset(ctx->peer_keys[i].public_key, 0, E2E_PUBLIC_KEY_SIZE);

            LOG_INFO("Removed E2E peer key for: %s", ctx->peer_keys[i].nickname);
            return 0;
        }
    }

    return -1; // Not found
}

const e2e_peer_key_t *e2e_find_peer_key(const e2e_context_t *ctx, const uint8_t *user_id)
{
    if (!ctx || !user_id) {
        return NULL;
    }

    for (int i = 0; i < ctx->num_peers; i++) {
        if (ctx->peer_keys[i].active && memcmp(ctx->peer_keys[i].user_id, user_id, 16) == 0) {
            return &ctx->peer_keys[i];
        }
    }

    return NULL;
}

int e2e_encrypt_message(e2e_context_t *ctx, const char *plaintext, const uint8_t *recipient_id,
                        e2e_encrypted_packet_t *packet)
{
    if (!ctx || !plaintext || !recipient_id || !packet) {
        return -1;
    }

    // Find recipient's public key
    const e2e_peer_key_t *peer = e2e_find_peer_key(ctx, recipient_id);
    if (!peer) {
        LOG_ERROR("Cannot encrypt: recipient's public key not found");
        return -1;
    }

    size_t plaintext_len = strlen(plaintext);
    if (plaintext_len >= E2E_MAX_MESSAGE_SIZE - 32) { // Reserve space for padding/MAC
        LOG_ERROR("Message too long for E2E encryption");
        return -1;
    }

    // Derive shared secret using our private key and their public key
    uint8_t shared_secret[32];
    derive_shared_secret(ctx->my_keys.private_key, peer->public_key, shared_secret);

    // Prepare packet
    memcpy(packet->sender_id, ctx->my_keys.user_id, 16);
    memcpy(packet->recipient_id, recipient_id, 16);

    // Add padding to plaintext
    uint8_t padded_plaintext[E2E_MAX_MESSAGE_SIZE];
    memcpy(padded_plaintext, plaintext, plaintext_len);

    // Simple padding with random bytes
    size_t padded_len = ((plaintext_len + 15) / 16) * 16; // Round up to 16-byte boundary
    for (size_t i = plaintext_len; i < padded_len; i++) {
        padded_plaintext[i] = (uint8_t)(rand() % 256);
    }

    // Encrypt using shared secret
    simple_encrypt(shared_secret, padded_plaintext, padded_len, packet->encrypted_data);
    packet->encrypted_length = padded_len;

    // Add message authentication (simple MAC)
    uint8_t mac_input[64];
    memcpy(mac_input, packet->sender_id, 16);
    memcpy(mac_input + 16, packet->recipient_id, 16);
    memcpy(mac_input + 32, shared_secret, 32);

    for (int i = 0; i < 32; i++) {
        packet->signature[i] = mac_input[i] ^ mac_input[i + 32] ^ (uint8_t)(i * 13);
    }

    // Wipe sensitive data
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(padded_plaintext, 0, sizeof(padded_plaintext));

    LOG_DEBUG("E2E encrypted message for %s (%zu bytes -> %zu bytes)", peer->nickname,
              plaintext_len, packet->encrypted_length);

    return 0;
}

int e2e_decrypt_message(e2e_context_t *ctx, const e2e_encrypted_packet_t *packet, char *plaintext,
                        size_t *plaintext_len)
{
    if (!ctx || !packet || !plaintext || !plaintext_len) {
        return -1;
    }

    // Verify this message is for us
    if (memcmp(packet->recipient_id, ctx->my_keys.user_id, 16) != 0) {
        LOG_WARN("E2E message not intended for us");
        return -1;
    }

    // Find sender's public key
    const e2e_peer_key_t *peer = e2e_find_peer_key(ctx, packet->sender_id);
    if (!peer) {
        LOG_ERROR("Cannot decrypt: sender's public key not found");
        return -1;
    }

    // Derive shared secret using our private key and their public key
    uint8_t shared_secret[32];
    derive_shared_secret(ctx->my_keys.private_key, peer->public_key, shared_secret);

    // Verify message authentication
    uint8_t mac_input[64];
    memcpy(mac_input, packet->sender_id, 16);
    memcpy(mac_input + 16, packet->recipient_id, 16);
    memcpy(mac_input + 32, shared_secret, 32);

    uint8_t expected_signature[32];
    for (int i = 0; i < 32; i++) {
        expected_signature[i] = mac_input[i] ^ mac_input[i + 32] ^ (uint8_t)(i * 13);
    }

    if (memcmp(packet->signature, expected_signature, 32) != 0) {
        LOG_ERROR("E2E message authentication failed");
        memset(shared_secret, 0, sizeof(shared_secret));
        return -1;
    }

    // Decrypt message
    uint8_t decrypted[E2E_MAX_MESSAGE_SIZE];
    simple_decrypt(shared_secret, packet->encrypted_data, packet->encrypted_length, decrypted);

    // Find actual message length (remove padding)
    size_t actual_len = packet->encrypted_length;
    for (size_t i = packet->encrypted_length - 1; i > 0; i--) {
        if (decrypted[i] == '\0' || (i < packet->encrypted_length - 16)) {
            actual_len = i + 1;
            break;
        }
    }

    // Ensure null termination and length fits
    if (actual_len >= *plaintext_len) {
        actual_len = *plaintext_len - 1;
    }

    memcpy(plaintext, decrypted, actual_len);
    plaintext[actual_len] = '\0';
    *plaintext_len        = actual_len;

    // Wipe sensitive data
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(decrypted, 0, sizeof(decrypted));

    LOG_DEBUG("E2E decrypted message from %s (%zu bytes -> %zu bytes)", peer->nickname,
              packet->encrypted_length, actual_len);

    return 0;
}

int e2e_create_key_exchange_packet(const e2e_context_t *ctx, e2e_key_exchange_packet_t *packet)
{
    if (!ctx || !packet) {
        return -1;
    }

    memcpy(packet->user_id, ctx->my_keys.user_id, 16);
    memcpy(packet->public_key, ctx->my_keys.public_key, E2E_PUBLIC_KEY_SIZE);
    strncpy(packet->nickname, ctx->my_keys.nickname, sizeof(packet->nickname) - 1);
    packet->nickname[sizeof(packet->nickname) - 1] = '\0';

    // Create self-signature (simple hash of the packet content)
    uint8_t sig_input[16 + E2E_PUBLIC_KEY_SIZE + 64];
    memcpy(sig_input, packet->user_id, 16);
    memcpy(sig_input + 16, packet->public_key, E2E_PUBLIC_KEY_SIZE);
    memcpy(sig_input + 16 + E2E_PUBLIC_KEY_SIZE, packet->nickname, 64);

    for (int i = 0; i < 32; i++) {
        packet->signature[i] =
            sig_input[i] ^ sig_input[i + 32] ^ sig_input[i + 64] ^ (uint8_t)(i * 19);
    }

    LOG_DEBUG("Created E2E key exchange packet for: %s", packet->nickname);
    return 0;
}

int e2e_verify_key_exchange_packet(const e2e_key_exchange_packet_t *packet)
{
    if (!packet) {
        return -1;
    }

    // Verify self-signature
    uint8_t sig_input[16 + E2E_PUBLIC_KEY_SIZE + 64];
    memcpy(sig_input, packet->user_id, 16);
    memcpy(sig_input + 16, packet->public_key, E2E_PUBLIC_KEY_SIZE);
    memcpy(sig_input + 16 + E2E_PUBLIC_KEY_SIZE, packet->nickname, 64);

    uint8_t expected_signature[32];
    for (int i = 0; i < 32; i++) {
        expected_signature[i] =
            sig_input[i] ^ sig_input[i + 32] ^ sig_input[i + 64] ^ (uint8_t)(i * 19);
    }

    if (memcmp(packet->signature, expected_signature, 32) != 0) {
        LOG_WARN("Key exchange packet signature verification failed");
        return -1;
    }

    return 0;
}

int e2e_generate_user_id(uint8_t *user_id, const char *nickname)
{
    if (!user_id || !nickname) {
        return -1;
    }

    // Generate user ID from nickname + timestamp + process ID
    uint32_t timestamp = (uint32_t)time(NULL);
    uint32_t pid       = (uint32_t)getpid();

    // Simple hash function
    for (int i = 0; i < 16; i++) {
        uint8_t nick_byte = (i < strlen(nickname)) ? nickname[i] : 0;
        user_id[i] = nick_byte ^ ((timestamp >> (i % 4)) & 0xFF) ^ ((pid >> (i % 4)) & 0xFF) ^
                     (uint8_t)(i * 23);
    }

    return 0;
}

void e2e_print_key(const uint8_t *key, size_t key_len, const char *label)
{
    if (!key || !label) {
        return;
    }

    char hex_str[key_len * 2 + 1];
    for (size_t i = 0; i < key_len && i < 16; i++) { // Only print first 16 bytes for brevity
        sprintf(hex_str + i * 2, "%02x", key[i]);
    }
    hex_str[32] = '\0'; // Cap at 32 chars

    LOG_DEBUG("%s: %s...", label, hex_str);
}

// Static helper functions

static void simple_encrypt(const uint8_t *key, const uint8_t *plaintext, size_t len,
                           uint8_t *ciphertext)
{
    for (size_t i = 0; i < len; i++) {
        ciphertext[i] = plaintext[i] ^ key[i % 32] ^ (uint8_t)(i & 0xFF);
    }
}

static void simple_decrypt(const uint8_t *key, const uint8_t *ciphertext, size_t len,
                           uint8_t *plaintext)
{
    for (size_t i = 0; i < len; i++) {
        plaintext[i] = ciphertext[i] ^ key[i % 32] ^ (uint8_t)(i & 0xFF);
    }
}

static void generate_random_bytes(uint8_t *buffer, size_t len)
{
    // Simple random number generation (use proper crypto RNG in production)
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        seeded = 1;
    }

    for (size_t i = 0; i < len; i++) {
        buffer[i] = (uint8_t)(rand() % 256);
    }
}

static void derive_shared_secret(const uint8_t *my_private, const uint8_t *peer_public,
                                 uint8_t *shared_secret)
{
    // Simple key derivation (use proper ECDH in production)
    for (int i = 0; i < 32; i++) {
        shared_secret[i] = my_private[i] ^ peer_public[i] ^ (uint8_t)(i * 31 + 73);
    }
}
