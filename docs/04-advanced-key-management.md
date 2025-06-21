# 04. Advanced Key Management & Perfect Forward Secrecy

## Overview

This phase implements a sophisticated key management system with perfect forward secrecy, quantum-resistant key distribution, and advanced key lifecycle management. The system ensures that even if long-term keys are compromised, past communications remain secure.

## Key Management Architecture

### 1. Hierarchical Key Structure

#### Key Hierarchy Design:

```
Hardware Root Key (HSM/TPM)
├── Server Master Key (SMK)
│   ├── Client Authentication Keys (CAK)
│   ├── Session Master Keys (SMK-S)
│   │   ├── Room Encryption Keys (REK)
│   │   ├── Message Encryption Keys (MEK)
│   │   └── Ephemeral Keys (EK)
│   └── Administrative Keys (ADK)
└── Backup Root Key (BRK)
```

#### Key Types and Properties:

```c
typedef enum {
    KEY_TYPE_ROOT_MASTER,        // Hardware-protected root
    KEY_TYPE_SERVER_MASTER,      // Server-wide master key
    KEY_TYPE_CLIENT_AUTH,        // Client authentication
    KEY_TYPE_SESSION_MASTER,     // Session management
    KEY_TYPE_ROOM_ENCRYPTION,    // Room-specific encryption
    KEY_TYPE_MESSAGE_EPHEMERAL,  // Per-message keys
    KEY_TYPE_FORWARD_SECRECY,    // PFS keys
    KEY_TYPE_EMERGENCY_REVOKE    // Emergency revocation
} key_type_t;

typedef struct {
    key_type_t type;
    uint32_t key_id;
    uint64_t generation_time;
    uint64_t expiry_time;
    uint32_t usage_counter;
    uint32_t max_usage;
    uint8_t key_material[MAX_KEY_SIZE];
    uint8_t derived_keys[MAX_DERIVED_KEYS][32];
    uint32_t security_level;
    bool revoked;
    uint8_t checksum[32];
} key_descriptor_t;
```

### 2. Perfect Forward Secrecy Implementation

#### Double Ratchet Algorithm:

```c
typedef struct {
    // Root chain
    uint8_t root_key[32];
    uint8_t chain_key_send[32];
    uint8_t chain_key_recv[32];

    // DH ratchet
    ecc_keypair_t dh_keypair_current;
    ecc_keypair_t dh_keypair_next;
    ecc_point_t remote_dh_public;

    // Message chain
    uint32_t send_counter;
    uint32_t recv_counter;
    uint32_t dh_ratchet_counter;

    // Skipped message keys
    skipped_key_t skipped_keys[MAX_SKIPPED_KEYS];
    uint32_t num_skipped;

    // Post-quantum enhancement
    kyber_keypair_t pq_keypair_current;
    kyber_keypair_t pq_keypair_next;

} double_ratchet_state_t;
```

#### Ratchet Operations:

1. **DH Ratchet Step**

   - Generate new DH keypair
   - Compute shared secret
   - Update root key
   - Derive new chain keys

2. **Symmetric Ratchet Step**
   - Advance chain key
   - Derive message key
   - Increment counters
   - Store for out-of-order messages

```c
int double_ratchet_encrypt(double_ratchet_state_t *state,
                          const uint8_t *plaintext, size_t plaintext_len,
                          uint8_t *ciphertext, size_t *ciphertext_len,
                          ratchet_header_t *header) {

    // Check if DH ratchet step needed
    if (state->send_counter >= DH_RATCHET_THRESHOLD) {
        if (perform_dh_ratchet_step(state) < 0) {
            return -1;
        }
    }

    // Derive message key from chain key
    uint8_t message_key[32];
    kdf_derive_message_key(message_key, state->chain_key_send, state->send_counter);

    // Advance sending chain
    kdf_advance_chain_key(state->chain_key_send, state->chain_key_send);

    // Prepare header
    header->dh_public_key = state->dh_keypair_current.public_key;
    header->pq_public_key = state->pq_keypair_current.public_key;
    header->send_counter = state->send_counter;
    header->dh_ratchet_counter = state->dh_ratchet_counter;

    // Encrypt message
    if (aes_256_gcm_encrypt(message_key, NULL, 0,
                           plaintext, plaintext_len,
                           ciphertext, ciphertext_len) < 0) {
        secure_zero(message_key, 32);
        return -1;
    }

    // Increment counter
    state->send_counter++;

    // Securely erase message key
    secure_zero(message_key, 32);

    return 0;
}
```

### 3. Quantum-Enhanced Key Agreement

#### Hybrid Key Agreement Protocol:

```c
typedef struct {
    // Classical components
    ecc_keypair_t ecdh_ephemeral;
    uint8_t ecdh_shared_secret[32];

    // Post-quantum components
    kyber_keypair_t kyber_ephemeral;
    uint8_t kyber_shared_secret[KYBER_SHARED_SECRET_SIZE];

    // Hybrid result
    uint8_t hybrid_shared_secret[64];
    uint8_t session_key[32];

    // Authentication
    dilithium_signature_t auth_signature;
    uint8_t transcript[1024];
    size_t transcript_len;

    // Security parameters
    uint32_t security_level;
    uint64_t agreement_timestamp;
    uint32_t agreement_id;
} hybrid_key_agreement_t;
```

#### Three-Phase Key Agreement:

1. **Initiation Phase**

   - Generate ephemeral keypairs (ECDH + Kyber)
   - Create initial transcript
   - Send public keys with authentication

2. **Response Phase**

   - Receive and validate public keys
   - Perform key agreement computations
   - Combine classical and post-quantum secrets

3. **Confirmation Phase**
   - Derive session keys
   - Sign transcript with long-term keys
   - Confirm key agreement success

```c
int perform_hybrid_key_agreement(hybrid_key_agreement_t *agreement,
                                const ecc_point_t *remote_ecdh_public,
                                const kyber_public_key_t *remote_kyber_public,
                                const dilithium_public_key_t *auth_public_key,
                                bool is_initiator) {

    // Perform ECDH computation
    if (ecdh_compute_shared_secret(agreement->ecdh_shared_secret,
                                  &agreement->ecdh_ephemeral.private_key,
                                  remote_ecdh_public) < 0) {
        return -1;
    }

    // Perform Kyber encapsulation/decapsulation
    if (is_initiator) {
        if (kyber_encaps(&agreement->kyber_shared_secret,
                        NULL, // ciphertext stored elsewhere
                        remote_kyber_public) < 0) {
            return -1;
        }
    } else {
        // Decapsulation handled by initiator's ciphertext
        // This is the responder path
    }

    // Combine secrets using KDF
    uint8_t combined_secret[sizeof(agreement->ecdh_shared_secret) +
                           sizeof(agreement->kyber_shared_secret)];
    memcpy(combined_secret, agreement->ecdh_shared_secret,
           sizeof(agreement->ecdh_shared_secret));
    memcpy(combined_secret + sizeof(agreement->ecdh_shared_secret),
           agreement->kyber_shared_secret, sizeof(agreement->kyber_shared_secret));

    // Derive hybrid shared secret
    hkdf_sha3_512(agreement->hybrid_shared_secret, 64,
                  combined_secret, sizeof(combined_secret),
                  NULL, 0,  // No salt
                  "HYBRID-KEY-AGREEMENT", 20);

    // Derive session key
    hkdf_sha3_512(agreement->session_key, 32,
                  agreement->hybrid_shared_secret, 64,
                  NULL, 0,
                  "SESSION-KEY", 11);

    // Build and sign transcript
    build_key_agreement_transcript(agreement);

    // Securely erase intermediate secrets
    secure_zero(combined_secret, sizeof(combined_secret));
    secure_zero(agreement->ecdh_shared_secret, sizeof(agreement->ecdh_shared_secret));
    secure_zero(agreement->kyber_shared_secret, sizeof(agreement->kyber_shared_secret));

    return 0;
}
```

### 4. Advanced Key Derivation

#### Multi-Context Key Derivation:

```c
typedef struct {
    uint8_t master_key[32];
    uint8_t context_info[256];
    size_t context_len;
    uint32_t derivation_counter;
    uint8_t salt[32];
    uint32_t security_level;
} key_derivation_context_t;
```

#### Context-Specific Derivation:

1. **Room Keys**: Derived per room with room metadata
2. **User Keys**: Per-user keys with identity context
3. **Message Keys**: Per-message with sequence numbers
4. **Emergency Keys**: Special derivation for revocation

```c
int derive_contextual_key(key_derivation_context_t *ctx,
                         const char *context_label,
                         uint8_t *derived_key, size_t key_len) {

    uint8_t kdf_input[512];
    size_t input_len = 0;

    // Build KDF input
    memcpy(kdf_input + input_len, ctx->master_key, 32);
    input_len += 32;

    memcpy(kdf_input + input_len, ctx->context_info, ctx->context_len);
    input_len += ctx->context_len;

    memcpy(kdf_input + input_len, context_label, strlen(context_label));
    input_len += strlen(context_label);

    memcpy(kdf_input + input_len, &ctx->derivation_counter, 4);
    input_len += 4;

    // Perform HKDF
    if (hkdf_sha3_512(derived_key, key_len,
                      kdf_input, input_len,
                      ctx->salt, 32,
                      context_label, strlen(context_label)) < 0) {
        secure_zero(kdf_input, sizeof(kdf_input));
        return -1;
    }

    // Increment counter
    ctx->derivation_counter++;

    // Securely erase input
    secure_zero(kdf_input, sizeof(kdf_input));

    return 0;
}
```

## Implementation Phases

### Phase 1: Key Infrastructure Setup (Days 1-3)

#### Core Key Manager:

```c
typedef struct {
    key_descriptor_t keys[MAX_KEYS];
    uint32_t num_keys;
    pthread_mutex_t key_mutex;

    // Root key protection
    uint8_t root_key_encrypted[64];
    uint8_t root_key_iv[16];
    bool root_key_loaded;

    // Key rotation schedule
    uint64_t next_rotation_time;
    uint32_t rotation_interval;

    // Emergency procedures
    uint8_t emergency_revoke_key[32];
    bool emergency_mode;

    // Performance tracking
    uint64_t key_operations_count;
    uint64_t key_derivations_count;

} key_manager_t;
```

#### Client-Side Key Management:

1. **Initialization**

   - Generate client master keys
   - Setup key rotation timers
   - Initialize ratchet states
   - Create secure storage

2. **Runtime Operations**
   - Handle automatic key rotation
   - Manage ephemeral keys
   - Process key exchange requests
   - Monitor key usage

```c
int key_manager_init(key_manager_t *km, const uint8_t *root_seed) {
    pthread_mutex_init(&km->key_mutex, NULL);

    // Initialize root key from hardware
    if (derive_root_key_from_hardware(km->root_key_encrypted,
                                     km->root_key_iv,
                                     root_seed) < 0) {
        return -1;
    }

    // Setup key rotation schedule
    km->rotation_interval = calculate_rotation_interval();
    km->next_rotation_time = get_current_time() + km->rotation_interval;

    // Generate emergency revocation key
    if (generate_secure_random(km->emergency_revoke_key, 32) < 0) {
        return -1;
    }

    // Initialize performance counters
    km->key_operations_count = 0;
    km->key_derivations_count = 0;
    km->emergency_mode = false;

    LOG_INFO("Key manager initialized with %d-second rotation interval",
             km->rotation_interval);

    return 0;
}
```

#### Server-Side Key Distribution:

1. **Multi-Client Management**

   - Maintain key states per client
   - Handle concurrent key operations
   - Implement key distribution protocols
   - Monitor system-wide key health

2. **Room Key Management**
   - Generate room-specific keys
   - Handle user join/leave events
   - Implement key escrow for compliance
   - Manage key lifecycle

### Phase 2: Perfect Forward Secrecy (Days 4-6)

#### Ratchet State Management:

```c
typedef struct {
    double_ratchet_state_t ratchet_state;
    uint32_t client_id;
    uint32_t room_id;
    uint64_t last_activity;

    // Out-of-order message handling
    message_buffer_t pending_messages[MAX_PENDING];
    uint32_t num_pending;

    // Performance optimization
    uint8_t cached_message_keys[CACHE_SIZE][32];
    uint32_t cache_counters[CACHE_SIZE];
    uint32_t cache_next_slot;

} ratchet_session_t;
```

#### Message Encryption with PFS:

```c
int encrypt_message_with_pfs(ratchet_session_t *session,
                            const uint8_t *plaintext, size_t plaintext_len,
                            encrypted_message_t *encrypted_msg) {

    ratchet_header_t header;

    // Encrypt with double ratchet
    if (double_ratchet_encrypt(&session->ratchet_state,
                              plaintext, plaintext_len,
                              encrypted_msg->ciphertext,
                              &encrypted_msg->ciphertext_len,
                              &header) < 0) {
        return -1;
    }

    // Add message authentication
    if (compute_message_mac(&header, encrypted_msg->ciphertext,
                           encrypted_msg->ciphertext_len,
                           encrypted_msg->mac) < 0) {
        return -1;
    }

    // Serialize header
    if (serialize_ratchet_header(&header, encrypted_msg->header) < 0) {
        return -1;
    }

    // Update session state
    session->last_activity = get_current_time();

    return 0;
}
```

### Phase 3: Key Rotation & Lifecycle (Days 7-9)

#### Automatic Key Rotation:

```c
typedef struct {
    uint64_t rotation_timer;
    uint32_t rotation_triggers;
    key_rotation_policy_t policy;

    // Rotation events
    rotation_event_t pending_rotations[MAX_ROTATIONS];
    uint32_t num_pending;

    // Emergency rotation
    bool emergency_rotation_active;
    uint64_t emergency_start_time;

} key_rotation_manager_t;
```

#### Rotation Triggers:

1. **Time-Based**: Regular interval rotation
2. **Usage-Based**: After N operations
3. **Event-Based**: User join/leave, security events
4. **Emergency**: Compromise detection

```c
void check_key_rotation_schedule(key_manager_t *km,
                                key_rotation_manager_t *rm) {
    uint64_t current_time = get_current_time();

    // Check time-based rotation
    if (current_time >= km->next_rotation_time) {
        schedule_key_rotation(rm, ROTATION_TRIGGER_TIME);
        km->next_rotation_time = current_time + km->rotation_interval;
    }

    // Check usage-based rotation
    if (km->key_operations_count >= MAX_KEY_OPERATIONS) {
        schedule_key_rotation(rm, ROTATION_TRIGGER_USAGE);
        km->key_operations_count = 0;
    }

    // Check emergency conditions
    if (detect_security_anomaly()) {
        schedule_emergency_rotation(rm);
    }

    // Process pending rotations
    process_rotation_queue(rm);
}
```

### Phase 4: Emergency Key Procedures (Days 10-11)

#### Emergency Response System:

```c
typedef struct {
    bool emergency_active;
    uint64_t emergency_start_time;
    emergency_trigger_t trigger_type;

    // Emergency keys
    uint8_t emergency_master_key[32];
    uint8_t revocation_signature[128];

    // Recovery procedures
    recovery_plan_t recovery_plan;
    uint32_t recovery_step;

} emergency_key_system_t;
```

#### Emergency Procedures:

1. **Immediate Response**

   - Revoke all current keys
   - Switch to emergency keys
   - Notify all clients
   - Log security event

2. **Recovery Process**
   - Generate new key hierarchy
   - Re-authenticate all clients
   - Restore normal operations
   - Conduct security audit

```c
int trigger_emergency_key_revocation(emergency_key_system_t *ems,
                                    emergency_trigger_t trigger) {

    ems->emergency_active = true;
    ems->emergency_start_time = get_current_time();
    ems->trigger_type = trigger;

    LOG_SECURITY("Emergency key revocation triggered: %s",
                 emergency_trigger_name(trigger));

    // Broadcast revocation to all clients
    broadcast_emergency_revocation();

    // Switch to emergency keys
    activate_emergency_keys(ems);

    // Start recovery timer
    schedule_key_recovery(ems);

    return 0;
}
```

## Security Validation

### Key Quality Testing:

1. **Entropy Analysis**

   - Test key randomness
   - Verify unpredictability
   - Check for patterns

2. **Forward Secrecy Validation**
   - Test key independence
   - Verify secure deletion
   - Check ratchet progression

### Performance Metrics:

- **Key Generation**: < 5ms per key
- **Key Rotation**: < 100ms system-wide
- **Ratchet Step**: < 1ms per operation
- **Emergency Response**: < 5 seconds

## Integration Points

### Client Integration:

1. **Startup**: Initialize key manager and ratchet states
2. **Message Handling**: Integrate PFS encryption/decryption
3. **Event Handling**: Process key rotation events
4. **Shutdown**: Secure key cleanup

### Server Integration:

1. **Client Management**: Per-client key contexts
2. **Room Management**: Room-specific key distribution
3. **Security Monitoring**: Key usage analytics
4. **Emergency Response**: System-wide emergency procedures

This advanced key management system provides military-grade security with quantum resistance and perfect forward secrecy while maintaining real-time performance.
