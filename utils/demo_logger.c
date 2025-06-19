#include "logger.h"
#include <unistd.h>

int main()
{
    // Initialize logger with INFO level
    logger_init("DEMO", LOG_LEVEL_DEBUG);

    LOG_INFO("=== Logger Demo Started ===");

    // Test different log levels
    LOG_DEBUG("This is a debug message - shows internal details");
    LOG_INFO("This is an info message - general information");
    LOG_WARN("This is a warning message - something might be wrong");
    LOG_ERROR("This is an error message - something went wrong");

    // Test specialized macros
    LOG_CONNECTION("Client connected from 192.168.1.100:5432");
    LOG_HANDSHAKE("3-way handshake initiated with client 'john'");
    LOG_PROTOCOL("Sending SYN packet (seq=12345, ack=0)");
    LOG_SERVER("Server started on port 8080");
    LOG_CLIENT("Client 'alice' joined the chat room");

    // Test performance logging
    LOG_PERF_START(database_query);
    usleep(50000); // Simulate some work (50ms)
    LOG_PERF_END(database_query);

    // Test conditional logging
    int connection_count = 5;
    LOG_IF(connection_count > 3, LOG_LEVEL_WARN, "High connection count detected: %d connections",
           connection_count);

    // Test file logging
    LOG_INFO("Switching to file logging...");
    logger_cleanup();

    // Initialize with file output
    logger_init_with_file("DEMO-FILE", LOG_LEVEL_DEBUG, "/tmp/chat_demo.log");

    LOG_INFO("This message goes to file: /tmp/chat_demo.log");
    LOG_ERROR("Error messages are also written to file");
    LOG_HANDSHAKE("File logging preserves all formatting");

    logger_cleanup();

    printf("\n=== Demo Complete ===\n");
    printf("Check /tmp/chat_demo.log for file output\n");

    return 0;
}
