#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../../utils/include/logger.h"
#include "../include/security_context.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>

// Global security state
static uint64_t            g_integrity_checksum      = 0;
static pthread_mutex_t     g_global_mutex            = PTHREAD_MUTEX_INITIALIZER;
static security_context_t *g_global_security_context = NULL;

// Global integrity checksum functions
uint64_t get_global_integrity_checksum(void)
{
    pthread_mutex_lock(&g_global_mutex);
    uint64_t checksum = g_integrity_checksum;
    pthread_mutex_unlock(&g_global_mutex);
    return checksum;
}

void set_global_integrity_checksum(uint64_t checksum)
{
    pthread_mutex_lock(&g_global_mutex);
    g_integrity_checksum = checksum;
    pthread_mutex_unlock(&g_global_mutex);
}

// Global security context functions
void *get_global_security_context(void)
{
    pthread_mutex_lock(&g_global_mutex);
    void *ctx = g_global_security_context;
    pthread_mutex_unlock(&g_global_mutex);
    return ctx;
}

void set_global_security_context(security_context_t *ctx)
{
    pthread_mutex_lock(&g_global_mutex);
    g_global_security_context = ctx;
    pthread_mutex_unlock(&g_global_mutex);
}

// Global performance counter functions
void update_global_performance_counters(const char *operation)
{
    if (!operation) {
        return;
    }

    pthread_mutex_lock(&g_global_mutex);

    // Update global performance counters
    if (g_global_security_context) {
        update_performance_counters(g_global_security_context, operation);
    }

    pthread_mutex_unlock(&g_global_mutex);

    LOG_DEBUG("Updated global performance counter: %s", operation);
}

// Initialize global security state
int init_global_security(void)
{
    pthread_mutex_lock(&g_global_mutex);

    g_integrity_checksum      = 0;
    g_global_security_context = NULL;

    pthread_mutex_unlock(&g_global_mutex);

    LOG_INFO("Global security state initialized");
    return 0;
}

// Cleanup global security state
void cleanup_global_security(void)
{
    pthread_mutex_lock(&g_global_mutex);

    g_integrity_checksum      = 0;
    g_global_security_context = NULL;

    pthread_mutex_unlock(&g_global_mutex);

    LOG_INFO("Global security state cleaned up");
}

// Entropy pool initialization function
int init_entropy_pool(entropy_pool_t *pool)
{
    if (!pool) {
        return -1;
    }

    // Initialize entropy pool arrays
    memset(pool->hw_entropy, 0, sizeof(pool->hw_entropy));
    memset(pool->timing_entropy, 0, sizeof(pool->timing_entropy));
    memset(pool->thermal_entropy, 0, sizeof(pool->thermal_entropy));
    memset(pool->mixed_pool, 0, sizeof(pool->mixed_pool));

    pool->entropy_estimate = 0;

    if (pthread_mutex_init(&pool->pool_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize entropy pool mutex");
        return -1;
    }

    LOG_DEBUG("Entropy pool initialized");
    return 0;
}
