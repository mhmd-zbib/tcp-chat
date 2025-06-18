#!/bin/bash

# TCP Chat Client Runner Script
# Usage: ./run.sh [nickname] [server_ip] [port]

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default values
DEFAULT_PORT=8080
DEFAULT_IP="127.0.0.1"
DEFAULT_NICKNAME="ChatUser"

# Function to display usage
show_usage() {
    echo -e "${BLUE}TCP Chat Client Runner${NC}"
    echo -e "${YELLOW}Usage:${NC}"
    echo "  ./run.sh                              # Run with defaults"
    echo "  ./run.sh [nickname]                   # Run with custom nickname"
    echo "  ./run.sh [nickname] [server_ip]       # Run with custom nickname and server IP"
    echo "  ./run.sh [nickname] [server_ip] [port] # Run with all custom options"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  ./run.sh                              # Default: ChatUser@127.0.0.1:8080"
    echo "  ./run.sh Alice                        # Alice@127.0.0.1:8080"
    echo "  ./run.sh Bob 192.168.1.100            # Bob@192.168.1.100:8080"
    echo "  ./run.sh Charlie localhost 9999       # Charlie@localhost:9999"
    echo ""
    echo -e "${YELLOW}Chat Commands:${NC}"
    echo "  /help     - Show help"
    echo "  /list     - Show online users"
    echo "  /quit     - Disconnect and exit"
    echo ""
}

# Function to build the client (clean build)
build_client() {
    echo -e "${BLUE}Building TCP Chat Client...${NC}"

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

# Function to check if server is running
check_server() {
    local ip=${1:-$DEFAULT_IP}
    local port=${2:-$DEFAULT_PORT}
    
    echo -e "${CYAN}Checking if server is available at ${ip}:${port}...${NC}"
    
    # Use timeout with nc (netcat) to check if port is open
    if timeout 3 nc -z "$ip" "$port" 2>/dev/null; then
        echo -e "${GREEN}✓ Server is reachable${NC}"
        return 0
    else
        echo -e "${YELLOW}⚠ Server not reachable. Make sure the server is running.${NC}"
        echo -e "${YELLOW}  To start the server: cd ../server && ./run.sh${NC}"
        return 1
    fi
}

# Function to run the client
run_client() {
    local nickname=${1:-$DEFAULT_NICKNAME}
    local ip=${2:-$DEFAULT_IP}
    local port=${3:-$DEFAULT_PORT}
    
    echo -e "${BLUE}Starting TCP Chat Client...${NC}"
    echo -e "${YELLOW}Configuration:${NC}"
    echo -e "  Nickname: ${CYAN}${nickname}${NC}"
    echo -e "  Server: ${CYAN}${ip}:${port}${NC}"
    echo ""
    
    # Check if executable exists
    if [ ! -f "build/client" ]; then
        echo -e "${RED}✗ Client executable not found. Building first...${NC}"
        if ! build_client; then
            exit 1
        fi
    fi

    # Run the client with correct argument format
    echo -e "${GREEN}Connecting to chat server...${NC}"
    echo -e "${YELLOW}Type '/help' for commands or '/quit' to exit${NC}"
    echo ""
    ./build/client "$nickname" --host "$ip" --port "$port"
}

# Function to generate a random nickname
generate_nickname() {
    local adjectives=("Cool" "Smart" "Fast" "Brave" "Swift" "Bright" "Happy" "Lucky")
    local nouns=("User" "Chat" "Player" "Guest" "Visitor" "Friend" "Buddy" "Pal")
    
    local adj=${adjectives[$RANDOM % ${#adjectives[@]}]}
    local noun=${nouns[$RANDOM % ${#nouns[@]}]}
    local num=$((RANDOM % 100))
    
    echo "${adj}${noun}${num}"
}

# Parse command line arguments
case "$1" in
    -h|--help)
        show_usage
        exit 0
        ;;
    -b|--build)
        build_client
        exit $?
        ;;
    -r|--rebuild)
        echo -e "${YELLOW}Cleaning and rebuilding...${NC}"
        build_client
        exit $?
        ;;
    --random)
        # Generate random nickname
        random_nick=$(generate_nickname)
        echo -e "${CYAN}Generated random nickname: ${random_nick}${NC}"
        build_client || exit 1
        echo ""
        run_client "$random_nick" "$2" "$3"
        ;;
    *)
        # Check if build is needed
        if [ ! -f "build/client" ] || [ ! -d "build" ]; then
            echo -e "${YELLOW}Client executable not found. Building...${NC}"
            build_client || exit 1
            echo ""
        else
            echo -e "${GREEN}Using existing build${NC}"
        fi
        
        # Run the client with provided arguments
        run_client "$1" "$2" "$3"
        ;;
esac
