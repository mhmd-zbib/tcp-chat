#include "../include/security_foundation.h"
#include "../../utils/include/logger.h"
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

// TODO: Move shared security functions to a common library

static security_manager_t *g_security_manager      = NULL;
static pthread_mutex_t     g_server_security_mutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// SECURITY MANAGER OPERATIONS (SERVER-SIDE)
// ============================================================================

security_manager_t *security_manager_create(void)
{
    security_manager_t *manager = (security_manager_t *)malloc(sizeof(security_manager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate security manager");
        return NULL;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        manager->contexts[i] = NULL;
    }

    manager->num_contexts = 0;

    if (pthread_mutex_init(&manager->manager_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize security manager mutex");
        free(manager);
        return NULL;
    }

    manager->min_security_level        = SECURITY_LEVEL_HIGH;
    manager->enforce_hardware_security = true;
    manager->key_rotation_interval     = 1000;

    manager->failed_auth_attempts = 0;
    manager->last_attack_time     = 0;
    manager->emergency_mode       = false;

    LOG_INFO("Security manager created with HIGH security policy");
    return manager;
}

void security_manager_destroy(security_manager_t *manager)
{
    if (!manager) {
        return;
    }

    LOG_INFO("Destroying security manager...");

    pthread_mutex_lock(&manager->manager_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (manager->contexts[i]) {
            security_context_destroy(manager->contexts[i]);
            manager->contexts[i] = NULL;
        }
    }

    pthread_mutex_unlock(&manager->manager_mutex);
    pthread_mutex_destroy(&manager->manager_mutex);

    free(manager);
    LOG_INFO("Security manager destroyed");
}

int security_manager_add_client(security_manager_t *manager, int client_id, security_level_t level)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return -1;
    }

    // Enforce minimum security level
    if (level < manager->min_security_level) {
        LOG_WARN("Client %d requested security level %d, enforcing minimum level %d", client_id,
                 level, manager->min_security_level);
        level = manager->min_security_level;
    }

    pthread_mutex_lock(&manager->manager_mutex);

    if (manager->contexts[client_id]) {
        LOG_WARN("Client %d already has security context - replacing", client_id);
        security_context_destroy(manager->contexts[client_id]);
    }

    manager->contexts[client_id] = security_context_create(level);
    if (!manager->contexts[client_id]) {
        LOG_ERROR("Failed to create security context for client %d", client_id);
        pthread_mutex_unlock(&manager->manager_mutex);
        return -1;
    }

    manager->num_contexts++;

    pthread_mutex_unlock(&manager->manager_mutex);

    LOG_INFO("Added security context for client %d (level %d)", client_id, level);
    return 0;
}

void security_manager_remove_client(security_manager_t *manager, int client_id)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return;
    }

    pthread_mutex_lock(&manager->manager_mutex);

    if (manager->contexts[client_id]) {
        security_context_destroy(manager->contexts[client_id]);
        manager->contexts[client_id] = NULL;
        manager->num_contexts--;
        LOG_INFO("Removed security context for client %d", client_id);
    }

    pthread_mutex_unlock(&manager->manager_mutex);
}

int security_manager_validate_client(security_manager_t *manager, int client_id)
{
    if (!manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return -1;
    }

    pthread_mutex_lock(&manager->manager_mutex);

    if (!manager->contexts[client_id]) {
        pthread_mutex_unlock(&manager->manager_mutex);
        return -1;
    }

    int result = security_context_validate(manager->contexts[client_id]);

    pthread_mutex_unlock(&manager->manager_mutex);

    if (result < 0) {
        LOG_WARN("Security validation failed for client %d", client_id);

        // Increment failed attempts
        manager->failed_auth_attempts++;
        manager->last_attack_time = time(NULL);

        if (manager->failed_auth_attempts >= 5) {
            manager->emergency_mode = true;
            LOG_FATAL("SECURITY ALERT: Multiple validation failures - entering emergency mode");
        }
    }

    return result;
}

// Server-specific initialization function
int server_security_foundation_init(void)
{
    pthread_mutex_lock(&g_server_security_mutex);

    if (security_foundation_init() < 0) {
        LOG_ERROR("Failed to initialize base security foundation");
        pthread_mutex_unlock(&g_server_security_mutex);
        return -1;
    }

    g_security_manager = security_manager_create();
    if (!g_security_manager) {
        LOG_ERROR("Failed to create security manager");
        security_foundation_cleanup();
        pthread_mutex_unlock(&g_server_security_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_server_security_mutex);

    LOG_INFO("Server security foundation initialized successfully");
    return 0;
}

void server_security_foundation_cleanup(void)
{
    pthread_mutex_lock(&g_server_security_mutex);

    if (g_security_manager) {
        security_manager_destroy(g_security_manager);
        g_security_manager = NULL;
    }

    security_foundation_cleanup();

    pthread_mutex_unlock(&g_server_security_mutex);

    LOG_INFO("Server security foundation cleanup complete");
}

// Get global security manager
security_manager_t *get_security_manager(void)
{
    return g_security_manager;
}

// Server-specific message encryption/decryption functions
int server_encrypt_message(int client_id, const uint8_t *plaintext, size_t plaintext_len,
                           uint8_t *ciphertext, size_t *ciphertext_len)
{
    if (!g_security_manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return -1;
    }

    pthread_mutex_lock(&g_security_manager->manager_mutex);

    security_context_t *ctx = g_security_manager->contexts[client_id];
    if (!ctx || !ctx->enc_ctx) {
        pthread_mutex_unlock(&g_security_manager->manager_mutex);
        return -1;
    }

    int result = encrypt_data(ctx->enc_ctx, plaintext, plaintext_len, ciphertext, ciphertext_len);

    pthread_mutex_unlock(&g_security_manager->manager_mutex);

    return result;
}

int server_decrypt_message(int client_id, const uint8_t *ciphertext, size_t ciphertext_len,
                           uint8_t *plaintext, size_t *plaintext_len)
{
    if (!g_security_manager || client_id < 0 || client_id >= MAX_CLIENTS) {
        return -1;
    }

    pthread_mutex_lock(&g_security_manager->manager_mutex);

    security_context_t *ctx = g_security_manager->contexts[client_id];
    if (!ctx || !ctx->enc_ctx) {
        pthread_mutex_unlock(&g_security_manager->manager_mutex);
        return -1;
    }

    int result = decrypt_data(ctx->enc_ctx, ciphertext, ciphertext_len, plaintext, plaintext_len);

    pthread_mutex_unlock(&g_security_manager->manager_mutex);

    return result;
}

hw_security_caps_t detect_hardware_security(void)
{
    hw_security_caps_t caps = {0};
    uint32_t           eax, ebx, ecx, edx;

    LOG_DEBUG("Detecting hardware security capabilities...");

    if (__get_cpuid_max(0, NULL) < 1) {
        LOG_WARN("CPUID not available - cannot detect hardware features");
        return caps;
    }

    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        caps.rdrand_available = (ecx & bit_RDRND) != 0;
        caps.aes_ni_present   = (ecx & bit_AES) != 0;
    }

    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        caps.cet_supported = (ecx & (1 << 7)) != 0;
        caps.mpk_supported = (ecx & (1 << 3)) != 0;
        caps.tsx_available = (ebx & (1 << 11)) != 0;
    }

    LOG_DEBUG("Hardware detection complete");
    return caps;
}

// Basic security foundation functions (simplified versions)
int security_foundation_init(void)
{
    LOG_INFO("Initializing basic security foundation...");
    return 0;
}

void security_foundation_cleanup(void)
{
    LOG_INFO("Basic security foundation cleanup complete");
}

security_context_t *security_context_create(security_level_t level)
{
    security_context_t *ctx = (security_context_t *)malloc(sizeof(security_context_t));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(security_context_t));

    ctx->enc_ctx = (encryption_context_t *)malloc(sizeof(encryption_context_t));
    if (!ctx->enc_ctx) {
        free(ctx);
        return NULL;
    }

    memset(ctx->enc_ctx, 0, sizeof(encryption_context_t));

    if (pthread_mutex_init(&ctx->context_mutex, NULL) != 0) {
        free(ctx->enc_ctx);
        free(ctx);
        return NULL;
    }

    if (pthread_mutex_init(&ctx->enc_ctx->crypto_mutex, NULL) != 0) {
        pthread_mutex_destroy(&ctx->context_mutex);
        free(ctx->enc_ctx);
        free(ctx);
        return NULL;
    }

    ctx->current_level = level;
    ctx->hw_caps       = detect_hardware_security();

    LOG_DEBUG("Created security context (level %d)", level);
    return ctx;
}

void security_context_destroy(security_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->enc_ctx) {
        pthread_mutex_destroy(&ctx->enc_ctx->crypto_mutex);
        memset(ctx->enc_ctx, 0, sizeof(encryption_context_t));
        free(ctx->enc_ctx);
    }

    pthread_mutex_destroy(&ctx->context_mutex);
    memset(ctx, 0, sizeof(security_context_t));
    free(ctx);
}

int security_context_validate(const security_context_t *ctx)
{
    return (ctx && ctx->enc_ctx) ? 0 : -1;
}

int init_encryption_context(encryption_context_t *enc_ctx, const uint8_t *key)
{
    if (!enc_ctx || !key) {
        return -1;
    }

    pthread_mutex_lock(&enc_ctx->crypto_mutex);

    memcpy(enc_ctx->session_key, key, 32);
    memcpy(enc_ctx->message_key, key, 32);
    enc_ctx->key_rotation_counter = 0;
    enc_ctx->sequence_number      = 0;

    pthread_mutex_unlock(&enc_ctx->crypto_mutex);
    return 0;
}

int encrypt_data(encryption_context_t *enc_ctx, const uint8_t *plaintext, size_t plaintext_len,
                 uint8_t *ciphertext, size_t *ciphertext_len)
{
    if (!enc_ctx || !plaintext || !ciphertext || !ciphertext_len) {
        return -1;
    }

    pthread_mutex_lock(&enc_ctx->crypto_mutex);

    if (*ciphertext_len < plaintext_len + 16) {
        pthread_mutex_unlock(&enc_ctx->crypto_mutex);
        return -1;
    }

    // Simple XOR encryption for Phase 1
    for (size_t i = 0; i < plaintext_len; i++) {
        ciphertext[i] = plaintext[i] ^ enc_ctx->session_key[i % 32];
    }

    uint8_t tag[16] = {0};
    for (size_t i = 0; i < plaintext_len; i++) {
        tag[i % 16] ^= plaintext[i];
    }
    memcpy(ciphertext + plaintext_len, tag, 16);

    *ciphertext_len = plaintext_len + 16;
    enc_ctx->sequence_number++;

    pthread_mutex_unlock(&enc_ctx->crypto_mutex);
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

    // Simple XOR decryption
    for (size_t i = 0; i < data_len; i++) {
        plaintext[i] = ciphertext[i] ^ enc_ctx->session_key[i % 32];
    }

    // Verify authentication tag
    uint8_t expected_tag[16] = {0};
    for (size_t i = 0; i < data_len; i++) {
        expected_tag[i % 16] ^= plaintext[i];
    }

    if (memcmp(ciphertext + data_len, expected_tag, 16) != 0) {
        pthread_mutex_unlock(&enc_ctx->crypto_mutex);
        return -1;
    }

    *plaintext_len = data_len;
    enc_ctx->sequence_number++;

    pthread_mutex_unlock(&enc_ctx->crypto_mutex);
    return 0;
}

// Anti-debugging functions
int detect_debugger(void)
{
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
        return 1;
    }
    return 0;
}

int verify_process_integrity(void)
{
    return 0; // Simplified implementation
}

uint64_t calculate_integrity_checksum(void)
{
    return (uint64_t)time(NULL) ^ (uint64_t)getpid();
}

void update_performance_counters(security_context_t *ctx, const char *operation)
{
    if (!ctx || !operation) {
        return;
    }

    pthread_mutex_lock(&ctx->context_mutex);

    if (strcmp(operation, "encrypt") == 0 || strcmp(operation, "decrypt") == 0) {
        ctx->crypto_operations++;
    }

    pthread_mutex_unlock(&ctx->context_mutex);
}
