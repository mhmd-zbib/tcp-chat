# TCP Chat System

A secure, multi-threaded TCP-based chat application with military-grade security features including end-to-end encryption, hardware-level security, and quantum-resistant cryptography.

## Features

- **Secure Communication**: End-to-end encryption with advanced cryptographic protocols
- **Multi-threaded Architecture**: Concurrent handling of multiple clients
- **Hardware Security**: CPU-level protections and hardware entropy collection
- **Anti-debugging**: Advanced protection against reverse engineering
- **Room Management**: Support for multiple chat rooms
- **Real-time Messaging**: Instant message delivery between clients
- **Cross-platform**: Compatible with Linux systems

## Project Structure

```
tcp-chat/
├── client/          # TCP chat client implementation
├── server/          # TCP chat server implementation
├── utils/           # Shared utilities and logging
├── docs/           # Comprehensive security documentation
└── README.md       # This file
```

## Prerequisites

- **Operating System**: Linux (tested on modern distributions)
- **Compiler**: GCC with C99 support
- **Build System**: CMake 3.16+
- **Dependencies**:
  - pthread library
  - math library (`libm`)

## Quick Start

### Building the Project

1. **Clone and navigate to the project:**

   ```bash
   cd tcp-chat
   ```

2. **Build the server:**

   ```bash
   cd server
   mkdir -p build && cd build
   cmake ..
   make
   ```

3. **Build the client:**
   ```bash
   cd ../../client
   mkdir -p build && cd build
   cmake ..
   make
   ```

### Running the Application

1. **Start the server:**

   ```bash
   cd server
   ./run.sh
   # Or manually: ./build/server --ip 127.0.0.1 --port 8080
   ```

2. **Connect with client(s):**
   ```bash
   cd client
   ./run.sh
   # Or manually: ./build/client --ip 127.0.0.1 --port 8080
   ```

## Usage

### Server Options

- `--ip <address>`: Bind to specific IP address (default: 127.0.0.1)
- `--port <port>`: Listen on specific port (default: 8080)

### Client Options

- `--ip <address>`: Connect to server IP address
- `--port <port>`: Connect to server port

### Chat Commands

- Type messages and press Enter to send
- Use Ctrl+C to disconnect gracefully

## Architecture

### Security Features

- **Hardware-Level Security**: Utilizes CPU security features and hardware RNG
- **End-to-End Encryption**: Messages encrypted before transmission
- **Key Exchange Protocol**: Secure key negotiation between clients
- **Anti-Debugging**: Protection against reverse engineering attempts
- **Secure Memory**: Protected memory allocation and cleanup

### Network Protocol

- **TCP-based**: Reliable message delivery
- **Custom Protocol**: Specialized chat protocol with security headers
- **Room Support**: Multiple chat rooms with isolated conversations
- **Handshake Protocol**: Secure connection establishment

## Documentation

Comprehensive security documentation is available in the `docs/` directory:

- **[Security Architecture](docs/01-foundation-security-architecture.md)**: Core security principles
- **[Hardware Security](docs/02-hardware-level-security.md)**: CPU-level protections
- **[Quantum Cryptography](docs/03-quantum-resistant-cryptography.md)**: Future-proof encryption
- **[Key Management](docs/04-advanced-key-management.md)**: Secure key handling
- **[Network Security](docs/05-network-steganography.md)**: Advanced network protection
- **[Implementation Guide](docs/08-master-implementation-roadmap.md)**: Development roadmap

## Development

### Project Layout

```
client/
├── src/            # Client source code
├── include/        # Client headers
├── build/          # Build artifacts
└── CMakeLists.txt  # Client build configuration

server/
├── src/            # Server source code
├── include/        # Server headers
├── build/          # Build artifacts
└── CMakeLists.txt  # Server build configuration

utils/
├── src/            # Shared utilities
├── include/        # Shared headers
└── CMakeLists.txt  # Utils build configuration
```

### Key Components

- **Client**: Multi-threaded chat client with encryption
- **Server**: Concurrent server handling multiple connections
- **Security Layer**: Hardware security and encryption modules
- **Protocol Handler**: Custom chat protocol implementation
- **Logger**: Comprehensive logging system

