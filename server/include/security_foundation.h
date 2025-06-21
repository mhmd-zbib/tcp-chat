#ifndef SECURITY_FOUNDATION_H
#define SECURITY_FOUNDATION_H

#include "types.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

// Security levels
typedef enum {
    SECURITY_LEVEL_LOW     = 1,
    SECURITY_LEVEL_MEDIUM  = 2,
    SECURITY_LEVEL_HIGH    = 3,
    SECURITY_LEVEL_MAXIMUM = 4
} security_level_t;

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

// Secure memory allocation header
typedef struct {
    uint32_t         magic;
    size_t           size;
    security_level_t security_level;
    uint64_t         allocation_time;
    uint32_t         canary;
} allocation_header_t;

// Memory protection context
typedef struct {
    void           *guard_pages[1024];
    size_t          num_guard_pages;
    pthread_mutex_t memory_mutex;
} memory_protection_t;

// AES-256-GCM context
typedef struct {
    uint8_t  key[32];
    uint8_t  iv[16];
    uint64_t counter;
    bool     initialized;
} aes256_gcm_context_t;

// ChaCha20-Poly1305 context
typedef struct {
    uint8_t  key[32];
    uint8_t  nonce[12];
    uint64_t counter;
    bool     initialized;
} chacha20_poly1305_context_t;

// Encryption context
typedef struct {
    aes256_gcm_context_t        primary_cipher;
    chacha20_poly1305_context_t backup_cipher;
    uint8_t                     session_key[32];      // Current session key
    uint8_t                     message_key[32];      // Current message key
    uint64_t                    key_rotation_counter; // Auto-rotation trigger
    uint32_t                    sequence_number;      // Message ordering
    pthread_mutex_t             crypto_mutex;
} encryption_context_t;

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

// Function declarations
int  security_foundation_init(void);
void security_foundation_cleanup(void);

// Hardware detection
hw_security_caps_t detect_hardware_security(void);
int                enable_hardware_security_features(const hw_security_caps_t *caps);

// Entropy collection
int collect_hardware_entropy(uint8_t *buffer, size_t size);
int collect_timing_entropy(uint8_t *buffer, size_t size);
int mix_entropy_sources(entropy_pool_t *pool);

// Secure memory management
void *secure_memory_allocate(size_t size, security_level_t level);
void  secure_memory_free(void *ptr);
int   secure_memory_wipe(void *ptr, size_t size);

// Security context management
security_context_t *security_context_create(security_level_t level);
void                security_context_destroy(security_context_t *ctx);
int                 security_context_validate(const security_context_t *ctx);

// Security manager operations (server-side)
security_manager_t *security_manager_create(void);
void                security_manager_destroy(security_manager_t *manager);
int security_manager_add_client(security_manager_t *manager, int client_id, security_level_t level);
void security_manager_remove_client(security_manager_t *manager, int client_id);
int  security_manager_validate_client(security_manager_t *manager, int client_id);

// Server-specific functions
int                 server_security_foundation_init(void);
void                server_security_foundation_cleanup(void);
security_manager_t *get_security_manager(void);
int server_encrypt_message(int client_id, const uint8_t *plaintext, size_t plaintext_len,
                           uint8_t *ciphertext, size_t *ciphertext_len);
int server_decrypt_message(int client_id, const uint8_t *ciphertext, size_t ciphertext_len,
                           uint8_t *plaintext, size_t *plaintext_len);

// Encryption operations
int init_encryption_context(encryption_context_t *enc_ctx, const uint8_t *key);
int encrypt_data(encryption_context_t *enc_ctx, const uint8_t *plaintext, size_t plaintext_len,
                 uint8_t *ciphertext, size_t *ciphertext_len);
int decrypt_data(encryption_context_t *enc_ctx, const uint8_t *ciphertext, size_t ciphertext_len,
                 uint8_t *plaintext, size_t *plaintext_len);

// Anti-debugging
int      detect_debugger(void);
int      verify_process_integrity(void);
uint64_t calculate_integrity_checksum(void);

// Performance monitoring
void update_performance_counters(security_context_t *ctx, const char *operation);

// Constants
#define ALLOCATION_MAGIC    0xDEADBEEF
#define PAGE_SIZE           4096
#define MAX_ENTROPY_SOURCES 8
#define CANARY_VALUE        0xCAFEBABE
#define MIN_ENTROPY_QUALITY 7.9

#endif // SECURITY_FOUNDATION_H
