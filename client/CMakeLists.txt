cmake_minimum_required(VERSION 3.16)
project(tcp_chat_client)

# Collect source files
file(GLOB_RECURSE CLIENT_SOURCES "src/*.c")
file(GLOB_RECURSE CLIENT_HEADERS "src/*.h")

# Create the client executable
add_executable(tcp-chat-client ${CLIENT_SOURCES})

# Link pthread library for threading support
target_link_libraries(tcp-chat-client pthread)

# Include directories
target_include_directories(tcp-chat-client PRIVATE src include)

# Set output name
set_target_properties(tcp-chat-client PROPERTIES OUTPUT_NAME "client")
