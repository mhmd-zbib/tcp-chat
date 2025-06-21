#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/hardware_security.h"
#include "../../utils/include/logger.h"
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

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

int enable_hardware_security_features(const hw_security_caps_t *caps)
{
    if (!caps) {
        return -1;
    }

    int features_enabled = 0;

    if (caps->cet_supported) {
        LOG_INFO("CET (Control Flow Integrity) available");
        features_enabled++;
    }

    if (caps->rdrand_available) {
        uint32_t      random_test;
        unsigned char rdrand_success = 0;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(random_test), "=qm"(rdrand_success));

        if (rdrand_success) {
            LOG_INFO("RDRAND hardware random number generator enabled");
            features_enabled++;
        } else {
            LOG_WARN("RDRAND available but test failed");
        }
    }

    if (caps->aes_ni_present) {
        LOG_INFO("AES-NI hardware acceleration available");
        features_enabled++;
    }

    if (caps->mpk_supported) {
        LOG_INFO("Memory Protection Keys (MPK) available");
        features_enabled++;
    }

    if (caps->tsx_available) {
        LOG_INFO("TSX (Transaction Synchronization Extensions) available");
        features_enabled++;
    }

    LOG_INFO("Enabled %d hardware security features", features_enabled);
    return features_enabled > 0 ? 0 : -1;
}

int collect_hardware_entropy(uint8_t *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return -1;
    }

    size_t collected = 0;

    // TODO: Implement hardware entropy sources beyond RDRAND
    hw_security_caps_t caps = detect_hardware_security();
    if (caps.rdrand_available) {
        for (size_t i = 0; i < size && collected < size - sizeof(uint32_t); i += sizeof(uint32_t)) {
            uint32_t      random_val;
            unsigned char rdrand_success = 0;
            __asm__ volatile("rdrand %0; setc %1" : "=r"(random_val), "=qm"(rdrand_success));

            if (rdrand_success) {
                memcpy(buffer + collected, &random_val, sizeof(uint32_t));
                collected += sizeof(uint32_t);
            } else {
                LOG_WARN("RDRAND failed at byte %zu", collected);
                break;
            }
        }
    }

    while (collected < size) {
        uint64_t tsc = __builtin_ia32_rdtsc();
        size_t   copy_size =
            (size - collected) > sizeof(uint64_t) ? sizeof(uint64_t) : (size - collected);
        memcpy(buffer + collected, &tsc, copy_size);
        collected += copy_size;
    }

    LOG_DEBUG("Collected %zu bytes of hardware entropy", collected);
    return (int)collected;
}

int collect_timing_entropy(uint8_t *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return -1;
    }

    struct timespec ts;
    struct timeval  tv;
    size_t          collected = 0;

    for (size_t i = 0; i < size && collected < size; i++) {
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
            buffer[collected++] = (uint8_t)(ts.tv_nsec & 0xFF);
            if (collected >= size)
                break;
            buffer[collected++] = (uint8_t)((ts.tv_nsec >> 8) & 0xFF);
        }

        if (collected >= size)
            break;

        if (gettimeofday(&tv, NULL) == 0) {
            buffer[collected++] = (uint8_t)(tv.tv_usec & 0xFF);
        }

        if (collected >= size)
            break;

        volatile int dummy = 0;
        for (int j = 0; j < (i % 100); j++) {
            dummy += j;
        }
        (void)dummy;
    }

    LOG_DEBUG("Collected %zu bytes of timing entropy", collected);
    return (int)collected;
}

int mix_entropy_sources(entropy_pool_t *pool)
{
    if (!pool) {
        return -1;
    }

    pthread_mutex_lock(&pool->pool_mutex);

    collect_hardware_entropy(pool->hw_entropy, sizeof(pool->hw_entropy));
    collect_timing_entropy(pool->timing_entropy, sizeof(pool->timing_entropy));

    // TODO: Add thermal sensor entropy collection
    int thermal_fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
    if (thermal_fd >= 0) {
        char    temp_str[32];
        ssize_t read_bytes = read(thermal_fd, temp_str, sizeof(temp_str) - 1);
        if (read_bytes > 0) {
            temp_str[read_bytes] = '\0';
            uint32_t temp        = (uint32_t)atoi(temp_str);
            memcpy(pool->thermal_entropy, &temp, sizeof(temp));

            for (size_t i = sizeof(temp); i < sizeof(pool->thermal_entropy); i++) {
                pool->thermal_entropy[i] = (uint8_t)(temp >> ((i % 4) * 8));
            }
        }
        close(thermal_fd);
    }

    memset(pool->mixed_pool, 0, sizeof(pool->mixed_pool));

    for (size_t i = 0; i < sizeof(pool->mixed_pool); i++) {
        uint8_t mixed_byte = 0;

        if (i < sizeof(pool->hw_entropy)) {
            mixed_byte ^= pool->hw_entropy[i];
        }

        if (i < sizeof(pool->timing_entropy)) {
            mixed_byte ^= pool->timing_entropy[i % sizeof(pool->timing_entropy)];
        }

        mixed_byte ^= pool->thermal_entropy[i % sizeof(pool->thermal_entropy)];
        mixed_byte          = (mixed_byte << 3) | (mixed_byte >> 5);
        pool->mixed_pool[i] = mixed_byte;
    }

    // TODO: Enhance entropy quality estimation algorithms
    pool->entropy_estimate  = 0;
    uint32_t bit_count[256] = {0};

    for (size_t i = 0; i < sizeof(pool->mixed_pool); i++) {
        bit_count[pool->mixed_pool[i]]++;
    }

    double shannon_entropy = 0.0;
    size_t total_bytes     = sizeof(pool->mixed_pool);

    for (int i = 0; i < 256; i++) {
        if (bit_count[i] > 0) {
            double probability = (double)bit_count[i] / total_bytes;
            shannon_entropy -= probability * log2(probability);
        }
    }

    uint32_t transitions = 0;
    for (size_t i = 1; i < sizeof(pool->mixed_pool); i++) {
        uint8_t diff = pool->mixed_pool[i] ^ pool->mixed_pool[i - 1];
        while (diff) {
            transitions += diff & 1;
            diff >>= 1;
        }
    }

    double transition_entropy = (double)transitions / (sizeof(pool->mixed_pool) * 8);
    double combined_entropy   = (shannon_entropy + transition_entropy) / 2.0;

    pool->entropy_estimate = (uint32_t)(combined_entropy * 1000);

    pthread_mutex_unlock(&pool->pool_mutex);

    LOG_DEBUG("Mixed entropy sources, Shannon entropy: %.2f bits/byte, transitions: %.2f "
              "bits/byte, combined: %.2f bits/byte",
              shannon_entropy, transition_entropy * 8, combined_entropy);

    // TODO: Adjust entropy threshold for production use
    return combined_entropy >= 3.0 ? 0 : -1;
}
