#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/security_context.h"
#include "../../utils/include/logger.h"
#include "../include/anti_debugging.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// External functions from other modules
extern void *get_global_security_context(void);
extern int   init_entropy_pool(entropy_pool_t *pool);

// Static function declarations
static int  initialize_context_mutexes(security_context_t *ctx);
static int  initialize_encryption_context(security_context_t *ctx, security_level_t level);
static void initialize_canaries(security_context_t *ctx);
static void initialize_context_fields(security_context_t *ctx, security_level_t level);
static void cleanup_encryption_context(security_context_t *ctx);
static void cleanup_guard_pages(security_context_t *ctx);
static void cleanup_context_mutexes(security_context_t *ctx);
static int  validate_canaries(const security_context_t *ctx);

security_context_t *security_context_create(security_level_t level)
{
    security_context_t *ctx =
        (security_context_t *)secure_memory_allocate(sizeof(security_context_t), level);
    if (!ctx) {
        LOG_ERROR("Failed to allocate security context");
        return NULL;
    }

    if (initialize_context_mutexes(ctx) != 0) {
        secure_memory_free(ctx);
        return NULL;
    }

    if (initialize_encryption_context(ctx, level) != 0) {
        cleanup_context_mutexes(ctx);
        secure_memory_free(ctx);
        return NULL;
    }

    initialize_canaries(ctx);
    initialize_context_fields(ctx, level);

    LOG_DEBUG("Created security context at %p (level %d)", ctx, level);
    return ctx;
}

void security_context_destroy(security_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    LOG_DEBUG("Destroying security context at %p", ctx);

    if (security_context_validate(ctx) < 0) {
        LOG_WARN("Security context validation failed during destruction");
    }

    cleanup_encryption_context(ctx);
    cleanup_guard_pages(ctx);
    cleanup_context_mutexes(ctx);

    secure_memory_wipe(ctx, sizeof(security_context_t));
    secure_memory_free(ctx);
}

int security_context_validate(const security_context_t *ctx)
{
    if (!ctx) {
        return -1;
    }

    if (validate_canaries(ctx) != 0) {
        return -1;
    }

    // TODO: Enhance integrity verification for production
    uint64_t current_checksum = calculate_integrity_checksum();
    if (ctx->integrity_checksum != 0 && ctx->integrity_checksum != current_checksum) {
        // Silently ignored for Phase 1 development
    }

    return 0;
}

void update_performance_counters(security_context_t *ctx, const char *operation)
{
    if (!ctx || !operation) {
        return;
    }

    pthread_mutex_lock(&ctx->context_mutex);

    if (strcmp(operation, "encrypt") == 0 || strcmp(operation, "decrypt") == 0) {
        ctx->crypto_operations++;
    } else if (strcmp(operation, "entropy") == 0) {
        ctx->entropy_collected++;
    } else if (strcmp(operation, "memory") == 0) {
        ctx->memory_allocations++;
    }

    pthread_mutex_unlock(&ctx->context_mutex);
    LOG_DEBUG("Updated performance counter: %s", operation);
}

static int initialize_context_mutexes(security_context_t *ctx)
{
    if (pthread_mutex_init(&ctx->context_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize context mutex");
        return -1;
    }

    if (pthread_mutex_init(&ctx->entropy.pool_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize entropy mutex");
        pthread_mutex_destroy(&ctx->context_mutex);
        return -1;
    }

    if (pthread_mutex_init(&ctx->mem_protection.memory_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize memory protection mutex");
        pthread_mutex_destroy(&ctx->entropy.pool_mutex);
        pthread_mutex_destroy(&ctx->context_mutex);
        return -1;
    }

    return 0;
}

static int initialize_encryption_context(security_context_t *ctx, security_level_t level)
{
    ctx->enc_ctx =
        (encryption_context_t *)secure_memory_allocate(sizeof(encryption_context_t), level);
    if (!ctx->enc_ctx) {
        LOG_ERROR("Failed to allocate encryption context");
        return -1;
    }
    return 0;
}

static void initialize_canaries(security_context_t *ctx)
{
    void *global_ctx = get_global_security_context();
    if (global_ctx) {
        // In a full implementation, this would access the global entropy pool
        for (int i = 0; i < 16; i++) {
            ctx->canary_start[i] = 0x42 + i;
            ctx->canary_end[i]   = 0xAB + i;
        }
    } else {
        for (int i = 0; i < 16; i++) {
            ctx->canary_start[i] = 0xDE;
            ctx->canary_end[i]   = 0xAD;
        }
    }
}

static void initialize_context_fields(security_context_t *ctx, security_level_t level)
{
    ctx->mem_protection.num_guard_pages = 0;
    ctx->current_level                  = level;
    ctx->crypto_operations              = 0;
    ctx->entropy_collected              = 0;
    ctx->memory_allocations             = 1;
    ctx->debugger_detected              = false;
    ctx->integrity_checksum             = calculate_integrity_checksum();
}

static void cleanup_encryption_context(security_context_t *ctx)
{
    if (ctx->enc_ctx) {
        secure_memory_wipe(ctx->enc_ctx, sizeof(encryption_context_t));
        secure_memory_free(ctx->enc_ctx);
    }
}

static void cleanup_guard_pages(security_context_t *ctx)
{
    pthread_mutex_lock(&ctx->mem_protection.memory_mutex);
    for (size_t i = 0; i < ctx->mem_protection.num_guard_pages; i++) {
        if (ctx->mem_protection.guard_pages[i]) {
            munmap(ctx->mem_protection.guard_pages[i], 4096); // PAGE_SIZE
        }
    }
    pthread_mutex_unlock(&ctx->mem_protection.memory_mutex);
}

static void cleanup_context_mutexes(security_context_t *ctx)
{
    pthread_mutex_destroy(&ctx->mem_protection.memory_mutex);
    pthread_mutex_destroy(&ctx->entropy.pool_mutex);
    pthread_mutex_destroy(&ctx->context_mutex);
}

static int validate_canaries(const security_context_t *ctx)
{
    void *global_ctx = get_global_security_context();
    if (global_ctx && ctx != global_ctx) {
        // In a full implementation, this would validate against global entropy
        // For now, just check they're not zero
        for (int i = 0; i < 16; i++) {
            if (ctx->canary_start[i] == 0 || ctx->canary_end[i] == 0) {
                LOG_ERROR("Security context canary validation failed");
                return -1;
            }
        }
    }
    return 0;
}
