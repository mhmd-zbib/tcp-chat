#ifndef SECURITY_CONTEXT_H
#define SECURITY_CONTEXT_H

#include "encryption_context.h"
#include "hardware_security.h"
#include "secure_memory.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Security context structure
typedef struct {
    uint8_t               canary_start[16];   // Stack canary
    encryption_context_t *enc_ctx;            // Encrypted pointer
    uint8_t               entropy_pool[1024]; // Hardware entropy
    uint8_t               key_material[256];  // Encrypted keys
    uint8_t               canary_end[16];     // End canary

    // Hardware capabilities
    hw_security_caps_t hw_caps;

    // Memory protection
    memory_protection_t mem_protection;

    // Entropy management
    entropy_pool_t entropy;

    // Security level
    security_level_t current_level;

    // Performance counters
    uint64_t crypto_operations;
    uint64_t entropy_collected;
    uint64_t memory_allocations;

    // Anti-debugging
    bool     debugger_detected;
    uint64_t integrity_checksum;

    pthread_mutex_t context_mutex;
} security_context_t;

// Function declarations
security_context_t *security_context_create(security_level_t level);
void                security_context_destroy(security_context_t *ctx);
int                 security_context_validate(const security_context_t *ctx);
void                update_performance_counters(security_context_t *ctx, const char *operation);

#endif // SECURITY_CONTEXT_H
