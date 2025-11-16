#ifndef GLOBAL_SECURITY_H
#define GLOBAL_SECURITY_H

#include "security_context.h"
#include <stdint.h>

// Global integrity checksum functions
uint64_t get_global_integrity_checksum(void);
void     set_global_integrity_checksum(uint64_t checksum);

// Global security context functions
void *get_global_security_context(void);
void  set_global_security_context(security_context_t *ctx);

// Global performance counter functions
void update_global_performance_counters(const char *operation);

// Entropy pool functions
int init_entropy_pool(entropy_pool_t *pool);

// Global security state management
int  init_global_security(void);
void cleanup_global_security(void);

#endif // GLOBAL_SECURITY_H
