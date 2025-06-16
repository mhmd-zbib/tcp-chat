#!/bin/bash

# TCP Chat Server Runner Script
# Usage: ./run.sh [port] [ip]

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
DEFAULT_PORT=8080
DEFAULT_IP="127.0.0.1"

# Function to display usage
show_usage() {
    echo -e "${BLUE}TCP Chat Server Runner${NC}"
    echo -e "${YELLOW}Usage:${NC}"
    echo "  ./run.sh                    # Run with defaults (127.0.0.1:8080)"
    echo "  ./run.sh [port]             # Run with custom port"
    echo "  ./run.sh [port] [ip]        # Run with custom port and IP"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  ./run.sh                    # Default: 127.0.0.1:8080"
    echo "  ./run.sh 9999               # Custom port: 127.0.0.1:9999"
    echo "  ./run.sh 9999 0.0.0.0       # Listen on all interfaces: 0.0.0.0:9999"
    echo ""
}

# Function to build the server (clean build)
build_server() {
    echo -e "${BLUE}Building TCP Chat Server...${NC}"

    # Clean build directory
    rm -rf build
    mkdir build
    cd build

    # Run cmake and make
    if cmake .. && make; then
        echo -e "${GREEN}✓ Build successful!${NC}"
        cd ..
        return 0
    else
        echo -e "${RED}✗ Build failed!${NC}"
        cd ..
        return 1
    fi
}

# Function to run the server
run_server() {
    local port=${1:-$DEFAULT_PORT}
    local ip=${2:-$DEFAULT_IP}
    
    echo -e "${BLUE}Starting TCP Chat Server...${NC}"
    echo -e "${YELLOW}Configuration: ${ip}:${port}${NC}"
    echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
    echo ""
    
    # Check if executable exists
    if [ ! -f "build/server" ]; then
        echo -e "${RED}✗ Server executable not found. Building first...${NC}"
        if ! build_server; then
            exit 1
        fi
    fi

    # Run the server
    ./build/server "$port" "$ip"
}

# Parse command line arguments
case "$1" in
    -h|--help)
        show_usage
        exit 0
        ;;
    -b|--build)
        build_server
        exit $?
        ;;
    -r|--rebuild)
        echo -e "${YELLOW}Cleaning and rebuilding...${NC}"
        build_server
        exit $?
        ;;
    *)
        # Always clean build before running
        echo -e "${YELLOW}Ensuring fresh build...${NC}"
        build_server || exit 1
        echo ""
        
        # Run the server with provided arguments
        run_server "$1" "$2"
        ;;
esac
