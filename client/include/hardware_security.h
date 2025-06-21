#ifndef HARDWARE_SECURITY_H
#define HARDWARE_SECURITY_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Hardware capabilities
typedef struct {
    bool cet_supported;
    bool rdrand_available;
    bool aes_ni_present;
    bool mpk_supported;
    bool tsx_available;
} hw_security_caps_t;

// Entropy pool
typedef struct {
    uint8_t         hw_entropy[1024];     // Hardware RNG
    uint8_t         timing_entropy[512];  // Timing variations
    uint8_t         thermal_entropy[256]; // Temperature data
    uint8_t         mixed_pool[2048];     // Combined entropy
    uint32_t        entropy_estimate;     // Quality estimate
    pthread_mutex_t pool_mutex;
} entropy_pool_t;

// Function declarations
hw_security_caps_t detect_hardware_security(void);
int                enable_hardware_security_features(const hw_security_caps_t *caps);
int                collect_hardware_entropy(uint8_t *buffer, size_t size);
int                collect_timing_entropy(uint8_t *buffer, size_t size);
int                mix_entropy_sources(entropy_pool_t *pool);

#endif // HARDWARE_SECURITY_H
