#ifndef SECURITY_FOUNDATION_H
#define SECURITY_FOUNDATION_H

#include "security_context.h"
#include "encryption_context.h"
#include "secure_memory.h"
#include <stdint.h>
#include <stdlib.h>

// Constants for backward compatibility with integer usage
#define SECURITY_LEVEL_LOW_INT    1
#define SECURITY_LEVEL_MEDIUM_INT 2  
#define SECURITY_LEVEL_HIGH_INT   3

// Function declarations for security foundation (using existing types)
int                 security_foundation_init(void);
void                security_foundation_cleanup(void);

// Wrapper functions for compatibility with int parameter
security_context_t *security_context_create_compat(int security_level);

// Additional security functions not in existing headers
int security_context_init(security_context_t *ctx);
void security_context_cleanup(security_context_t *ctx);

// Anti-debugging and integrity functions
int detect_debugger(void);
int verify_process_integrity(void);

// Phase 1: Basic security operations (placeholders)
int security_authenticate_user(security_context_t *ctx, const char *username, const char *password);
int security_generate_session_id(security_context_t *ctx);
int security_encrypt_message(security_context_t *ctx, const char *plaintext, char **ciphertext);
int security_decrypt_message(security_context_t *ctx, const char *ciphertext, char **plaintext);

#endif // SECURITY_FOUNDATION_H
