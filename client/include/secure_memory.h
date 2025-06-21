#ifndef SECURE_MEMORY_H
#define SECURE_MEMORY_H

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

// Function declarations
void *secure_memory_allocate(size_t size, security_level_t level);
void  secure_memory_free(void *ptr);
int   secure_memory_wipe(void *ptr, size_t size);

// Constants
#define ALLOCATION_MAGIC 0xDEADBEEF
#define PAGE_SIZE        4096
#define CANARY_VALUE     0xCAFEBABE

#endif // SECURE_MEMORY_H
