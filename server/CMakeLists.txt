cmake_minimum_required(VERSION 3.16)
project(tcp_chat_server)

# Add utils subdirectory
add_subdirectory(../utils utils)

# Collect source files
file(GLOB_RECURSE SERVER_SOURCES "src/*.c")
file(GLOB_RECURSE SERVER_HEADERS "src/*.h")

# Create the server executable
add_executable(tcp-chat-server ${SERVER_SOURCES})

# Link pthread library and utils library
target_link_libraries(tcp-chat-server pthread tcp-chat-utils)

# Include directories
target_include_directories(tcp-chat-server PRIVATE src include ../utils/include)

# Set output name
set_target_properties(tcp-chat-server PROPERTIES OUTPUT_NAME "server")
