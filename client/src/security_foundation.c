#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/security_foundation.h"
#include "../include/hardware_security.h"
#include "../include/security_context.h"
#include "../include/anti_debugging.h"
#include "../../utils/include/logger.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Global security state
static bool                g_security_initialized    = false;
static security_context_t *g_global_security_context = NULL;
static pthread_mutex_t     g_security_mutex          = PTHREAD_MUTEX_INITIALIZER;

// Static function declarations
static int  initialize_global_context(void);
static void log_hardware_capabilities(void);
static int  perform_security_checks(void);

int security_foundation_init(void)
{
    pthread_mutex_lock(&g_security_mutex);

    if (g_security_initialized) {
        pthread_mutex_unlock(&g_security_mutex);
        return 0;
    }

    LOG_INFO("Initializing military-grade security foundation...");

    if (initialize_global_context() != 0) {
        pthread_mutex_unlock(&g_security_mutex);
        return -1;
    }

    g_global_security_context->hw_caps = detect_hardware_security();

    if (enable_hardware_security_features(&g_global_security_context->hw_caps) < 0) {
        LOG_WARN("Some hardware security features could not be enabled");
    }

    if (mix_entropy_sources(&g_global_security_context->entropy) < 0) {
        LOG_ERROR("Failed to initialize entropy collection");
        security_context_destroy(g_global_security_context);
        g_global_security_context = NULL;
        pthread_mutex_unlock(&g_security_mutex);
        return -1;
    }

    if (perform_security_checks() != 0) {
        LOG_WARN("Security checks detected potential threats");
    }

    g_global_security_context->integrity_checksum = calculate_integrity_checksum();
    g_security_initialized = true;

    pthread_mutex_unlock(&g_security_mutex);

    log_hardware_capabilities();
    LOG_INFO("Security foundation initialized successfully");
    return 0;
}

void security_foundation_cleanup(void)
{
    pthread_mutex_lock(&g_security_mutex);

    if (!g_security_initialized) {
        pthread_mutex_unlock(&g_security_mutex);
        return;
    }

    LOG_INFO("Cleaning up security foundation...");

    if (g_global_security_context) {
        security_context_destroy(g_global_security_context);
        g_global_security_context = NULL;
    }

    g_security_initialized = false;
    pthread_mutex_unlock(&g_security_mutex);

    LOG_INFO("Security foundation cleanup complete");
}

void *get_global_security_context(void)
{
    return g_global_security_context;
}

void update_global_performance_counters(const char *operation)
{
    if (g_global_security_context) {
        update_performance_counters(g_global_security_context, operation);
    }
}

uint64_t get_global_integrity_checksum(void)
{
    if (g_global_security_context) {
        return g_global_security_context->integrity_checksum;
    }
    return 0;
}

void set_global_integrity_checksum(uint64_t checksum)
{
    if (g_global_security_context) {
        g_global_security_context->integrity_checksum = checksum;
    }
}

static int initialize_global_context(void)
{
    g_global_security_context = security_context_create(SECURITY_LEVEL_HIGH);
    if (!g_global_security_context) {
        LOG_ERROR("Failed to create global security context");
        return -1;
    }
    return 0;
}

static void log_hardware_capabilities(void)
{
    if (!g_global_security_context) {
        return;
    }

    LOG_INFO("Hardware capabilities: CET=%s, RDRAND=%s, AES-NI=%s, MPK=%s, TSX=%s",
             g_global_security_context->hw_caps.cet_supported ? "YES" : "NO",
             g_global_security_context->hw_caps.rdrand_available ? "YES" : "NO",
             g_global_security_context->hw_caps.aes_ni_present ? "YES" : "NO",
             g_global_security_context->hw_caps.mpk_supported ? "YES" : "NO",
             g_global_security_context->hw_caps.tsx_available ? "YES" : "NO");
}

static int perform_security_checks(void)
{
    int threats_detected = 0;

    if (detect_debugger()) {
        LOG_WARN("SECURITY ALERT: Debugger detected during initialization!");
        g_global_security_context->debugger_detected = true;
        threats_detected++;
    }

    if (verify_process_integrity() < 0) {
        LOG_WARN("Process integrity verification failed");
        threats_detected++;
    }

    return threats_detected;
}
