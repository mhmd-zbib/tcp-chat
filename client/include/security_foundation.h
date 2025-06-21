#ifndef SECURITY_FOUNDATION_H
#define SECURITY_FOUNDATION_H

#include "types.h"
#include "secure_memory.h"
#include "hardware_security.h"
#include "encryption_context.h"
#include "security_context.h"
#include "anti_debugging.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Security manager for server
typedef struct {
    security_context_t *contexts[MAX_CLIENTS];
    uint32_t            num_contexts;
    pthread_mutex_t     manager_mutex;

    // Global security policies
    security_level_t min_security_level;
    bool             enforce_hardware_security;
    uint32_t         key_rotation_interval;

    // Threat detection
    uint32_t failed_auth_attempts;
    uint64_t last_attack_time;
    bool     emergency_mode;
} security_manager_t;

// Main foundation functions
int  security_foundation_init(void);
void security_foundation_cleanup(void);

// Global context access
void    *get_global_security_context(void);
void     update_global_performance_counters(const char *operation);
uint64_t get_global_integrity_checksum(void);
void     set_global_integrity_checksum(uint64_t checksum);

// Constants
#define MAX_ENTROPY_SOURCES 8
#define MIN_ENTROPY_QUALITY 7.9

#endif // SECURITY_FOUNDATION_H