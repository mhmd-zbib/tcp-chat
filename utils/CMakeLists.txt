cmake_minimum_required(VERSION 3.10)

# Create utils library
add_library(tcp-chat-utils STATIC
  src/logger.c
)

target_include_directories(tcp-chat-utils PUBLIC
  include
)

# Link pthread for thread safety
find_package(Threads REQUIRED)
target_link_libraries(tcp-chat-utils Threads::Threads)

# Set compiler flags
target_compile_options(tcp-chat-utils PRIVATE
  -Wall
  -Wextra
  -std=c99
  -D_GNU_SOURCE
)
