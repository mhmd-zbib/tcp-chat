#ifndef ANTI_DEBUGGING_H
#define ANTI_DEBUGGING_H

#include <stdint.h>

// Function declarations
int      detect_debugger(void);
int      verify_process_integrity(void);
uint64_t calculate_integrity_checksum(void);

#endif // ANTI_DEBUGGING_H
