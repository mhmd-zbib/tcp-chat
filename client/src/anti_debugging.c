#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/anti_debugging.h"
#include "../../utils/include/logger.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// External function to get global integrity checksum
extern uint64_t get_global_integrity_checksum(void);
extern void     set_global_integrity_checksum(uint64_t checksum);

// Static function declarations
static int check_tracer_pid(void);
static int check_execution_timing(void);

int detect_debugger(void)
{
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
        LOG_WARN("PTRACE_TRACEME failed - possible debugger attached");
        return 1;
    }

    if (check_tracer_pid() == 1) {
        return 1;
    }

    // TODO: Add more sophisticated debugger detection methods
    if (check_execution_timing() == 1) {
        return 1;
    }

    return 0;
}

int verify_process_integrity(void)
{
    uint64_t current_checksum = calculate_integrity_checksum();
    uint64_t stored_checksum  = get_global_integrity_checksum();

    if (stored_checksum != 0 && current_checksum != stored_checksum) {
        LOG_WARN("Process integrity verification failed - checksum mismatch");
        return 0; // TODO: Change to -1 for production
    }

    return 0;
}

uint64_t calculate_integrity_checksum(void)
{
    // TODO: Implement comprehensive integrity checking for production
    uint64_t checksum = 0;

    checksum ^= (uint64_t)getpid();
    checksum ^= 0x1337DEADBEEF1337ULL;
    checksum ^= ((uint64_t)(uintptr_t)&checksum) & 0xFFFFFF;
    checksum ^= ((uint64_t)(uintptr_t)detect_debugger) & 0xFFFFFF;

    return checksum;
}

static int check_tracer_pid(void)
{
    FILE *status_file = fopen("/proc/self/status", "r");
    if (status_file) {
        char line[256];
        while (fgets(line, sizeof(line), status_file)) {
            if (strncmp(line, "TracerPid:", 10) == 0) {
                int tracer_pid = atoi(line + 10);
                if (tracer_pid != 0) {
                    fclose(status_file);
                    LOG_WARN("Tracer PID detected: %d", tracer_pid);
                    return 1;
                }
                break;
            }
        }
        fclose(status_file);
    }
    return 0;
}

static int check_execution_timing(void)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    volatile int dummy = 0;
    for (int i = 0; i < 1000; i++) {
        dummy += i;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);

    if (elapsed_ns > 100000000L) {
        LOG_WARN("Suspicious execution timing detected: %ld ns", elapsed_ns);
        return 1;
    }

    return 0;
}
