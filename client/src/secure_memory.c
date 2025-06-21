#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/secure_memory.h"
#include "../../utils/include/logger.h"
#include "../include/hardware_security.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

// External global context - will be properly handled in main file
extern void *get_global_security_context(void);

// Static function declarations
static void initialize_memory_content(void *user_ptr, size_t size, security_level_t level);
static void initialize_maximum_security_memory(void *user_ptr, size_t size);
static void update_allocation_counters(void);

void *secure_memory_allocate(size_t size, security_level_t level)
{
    if (size == 0) {
        return NULL;
    }

    // TODO: Implement hardware-backed secure memory allocation
    size_t total_size = sizeof(allocation_header_t) + size + (PAGE_SIZE * 2);
    total_size        = (total_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    void *mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED) {
        LOG_ERROR("Failed to allocate secure memory: %s", strerror(errno));
        return NULL;
    }

    if (mlock(mem, total_size) != 0) {
        LOG_WARN("Failed to lock secure memory: %s", strerror(errno));
    }

    if (mprotect(mem, PAGE_SIZE, PROT_NONE) != 0 ||
        mprotect((char *)mem + total_size - PAGE_SIZE, PAGE_SIZE, PROT_NONE) != 0) {
        LOG_WARN("Failed to set up guard pages");
    }

    allocation_header_t *header = (allocation_header_t *)((char *)mem + PAGE_SIZE);
    header->magic               = ALLOCATION_MAGIC;
    header->size                = size;
    header->security_level      = level;
    header->allocation_time     = time(NULL);
    header->canary              = CANARY_VALUE;

    void *user_ptr = (char *)header + sizeof(allocation_header_t);

    initialize_memory_content(user_ptr, size, level);
    update_allocation_counters();

    LOG_DEBUG("Allocated %zu bytes of secure memory at %p (level %d)", size, user_ptr, level);
    return user_ptr;
}

void secure_memory_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    allocation_header_t *header =
        (allocation_header_t *)((char *)ptr - sizeof(allocation_header_t));

    if (header->magic != ALLOCATION_MAGIC || header->canary != CANARY_VALUE) {
        LOG_ERROR("Memory corruption detected in secure allocation at %p", ptr);
        return;
    }

    size_t size = header->size;
    secure_memory_wipe(ptr, size);

    size_t total_size  = sizeof(allocation_header_t) + size + (PAGE_SIZE * 2);
    total_size         = (total_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    void *original_mem = (char *)header - PAGE_SIZE;

    munlock(original_mem, total_size);
    munmap(original_mem, total_size);

    LOG_DEBUG("Freed %zu bytes of secure memory", size);
}

int secure_memory_wipe(void *ptr, size_t size)
{
    if (!ptr || size == 0) {
        return -1;
    }

    // TODO: Implement cryptographically secure memory wiping
    volatile uint8_t *volatile_ptr = (volatile uint8_t *)ptr;

    for (size_t i = 0; i < size; i++) {
        volatile_ptr[i] = 0x00;
    }

    for (size_t i = 0; i < size; i++) {
        volatile_ptr[i] = 0xFF;
    }

    for (size_t i = 0; i < size; i++) {
        volatile_ptr[i] = (uint8_t)(i ^ 0xAA);
    }

    for (size_t i = 0; i < size; i++) {
        volatile_ptr[i] = 0x00;
    }

    __asm__ __volatile__("" ::: "memory");
    return 0;
}

static void initialize_memory_content(void *user_ptr, size_t size, security_level_t level)
{
    switch (level) {
        case SECURITY_LEVEL_MAXIMUM:
            initialize_maximum_security_memory(user_ptr, size);
            break;
        case SECURITY_LEVEL_HIGH:
            memset(user_ptr, 0xAA, size);
            break;
        default:
            memset(user_ptr, 0, size);
            break;
    }
}

static void initialize_maximum_security_memory(void *user_ptr, size_t size)
{
    void *global_ctx = get_global_security_context();
    if (global_ctx) {
        // In the actual implementation, this would access the entropy pool
        // For now, use a simple pattern
        for (size_t i = 0; i < size; i++) {
            ((uint8_t *)user_ptr)[i] = (uint8_t)(i ^ 0x42);
        }
    } else {
        memset(user_ptr, 0xAA, size);
    }
}

static void update_allocation_counters(void)
{
    void *global_ctx = get_global_security_context();
    if (global_ctx) {
        // In the actual implementation, this would update performance counters
        // This will be handled properly when integrating with the main context
    }
}
