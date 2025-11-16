#include "../include/cli_args.h"
#include "../include/client.h"
#include "../include/e2e_encryption.h"
#include "../include/types.h"
#include "logger.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static client_t *g_client = NULL;

void signal_handler(int sig)
{
    LOG_INFO("Received signal %d. Shutting down...", sig);
    if (g_client) {
        client_disconnect(g_client);
    }
    logger_cleanup();
    exit(0);
}

void *message_receiver_thread(void *arg)
{
    client_t *client = (client_t *)arg;
    if (!client) {
        LOG_ERROR("message_receiver_thread: client is NULL");
        pthread_exit(NULL);
    }

    char buffer[BUFFER_SIZE];
    while (client->connected) {
        // Set up a timeout on recv to allow checking connected flag periodically
        struct timeval tv;
        tv.tv_sec  = 1; // 1 second timeout
        tv.tv_usec = 0;
        setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int bytes_received = recv(client->socket_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            // Null-terminate the buffer
            buffer[bytes_received] = '\0';

            // Check for special E2E messages
            if (strncmp(buffer, "/keyexchange:", 13) == 0) {
                // Handle key exchange
                e2e_key_exchange_packet_t key_packet;
                if (client_parse_key_exchange(buffer + 13, &key_packet) == 0) {
                    client_handle_key_exchange(client, &key_packet);
                }
                continue; // Don't display key exchange messages
            } else if (strncmp(buffer, "/e2emsg:", 8) == 0) {
                // Handle E2E encrypted message
                client_handle_e2e_message(client, buffer + 8);
                continue; // E2E messages are displayed by the handler
            }

            // Print regular server messages
            printf("%s", buffer);

            // If the message doesn't end with newline, add one for proper formatting
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] != '\n') {
                printf("\n");
            }

            // Flush stdout to ensure message is displayed immediately
            fflush(stdout);
        } else if (bytes_received == 0) {
            // Connection closed by the server
            LOG_CONNECTION("Server disconnected");
            client->connected = 0;
            break;
        } else if (bytes_received < 0) {
            // Error handling
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This is just a timeout, continue the loop
                continue;
            } else {
                // Real error
                LOG_ERRNO("Error receiving data from server");
                client->connected = 0;
                break;
            }
        }
    }

    LOG_INFO("Message receiver thread exiting");
    pthread_exit(NULL);
}

void main_show_room_selection_menu(void)
{
    // Server already sends the room selection menu
    // Just show a simple prompt
    printf("Enter your choice: ");
    fflush(stdout);
}

int main_handle_room_selection(void)
{
    char input[BUFFER_SIZE];
    int  room_selected = 0;

    main_show_room_selection_menu();

    while (!room_selected && g_client->connected) {
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        // Skip empty input
        if (strlen(input) == 0) {
            printf("Enter your choice: ");
            fflush(stdout);
            continue;
        }

        // Handle quit command
        if (strcmp(input, "/quit") == 0) {
            LOG_INFO("Quitting chat...");
            return -1;
        }

        if (client_send_message(g_client, input) < 0) {
            LOG_ERROR("Failed to send command");
            return -1;
        }

        if (strncmp(input, "/create ", 8) == 0 || strncmp(input, "/join ", 6) == 0) {
            printf("Processing command, please wait...\n");
            usleep(1000000); // 1 second to allow server response

            // The server automatically adds the client to the room upon creation/join
            // Server sends success messages that include room ID information
            printf("\n✓ Command sent! Check the server response above.\n");
            printf("If successful, you should now be in the room!\n\n");
            printf("Type your messages to chat, or use commands:\n");
            printf("  /leave - Leave current room\n");
            printf("  /users - Show users in room\n");
            printf("  /quit  - Exit chat\n\n");
            room_selected = 1;
        } else if (strncmp(input, "/rooms", 6) == 0 || strncmp(input, "/help", 5) == 0) {
            // These commands don't change room state, continue waiting for response
            usleep(200000); // 200ms for server response
            printf("\nEnter your choice: ");
            fflush(stdout);
        } else {
            printf("Invalid command. Use /create <name>, /join <id>, /rooms, /help, or /quit\n");
            printf("Enter your choice: ");
            fflush(stdout);
        }
    }

    return room_selected ? 0 : -1;
}

int main(int argc, char *argv[])
{
    logger_init("TCP-CHAT-CLIENT", LOG_LEVEL_DEBUG);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    LOG_INFO("TCP Chat Client starting...");

    cli_args_t args = parse_args(argc, argv);
    if (!args.valid) {
        LOG_ERROR("Invalid command line arguments");
        print_usage(argv[0]);
        logger_cleanup();
        return 1;
    }

    g_client = client_create(args.server_ip, args.server_port, args.nickname);
    if (!g_client) {
        LOG_ERROR("Failed to create client");
        logger_cleanup();
        return 1;
    }

    if (client_connect_to_server(g_client) < 0) {
        LOG_ERROR("Failed to connect to server");
        client_destroy(g_client);
        logger_cleanup();
        return 1;
    }

    LOG_INFO("\nConnection established! Welcome to TCP Chat!");
    LOG_INFO("You need to create or join a room before chatting.");

    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, message_receiver_thread, g_client) != 0) {
        LOG_ERROR("Failed to create receiver thread: %s", strerror(errno));
        client_destroy(g_client);
        logger_cleanup();
        return 1;
    }

    // Wait a moment for welcome messages from server
    usleep(200000); // 200ms

    // Handle room selection first
    if (main_handle_room_selection() < 0) {
        LOG_INFO("Room selection cancelled or failed");
        g_client->connected = 0;
        pthread_join(receive_thread, NULL);
        client_destroy(g_client);
        logger_cleanup();
        return 0;
    }

    // Main chat loop after room selection
    char input[BUFFER_SIZE];
    while (1) {
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        // Check for quit command
        if (strcmp(input, "/quit") == 0) {
            LOG_INFO("Quitting chat...");
            break;
        }

        // Check for E2E message command: /e2e <nickname> <message>
        if (strncmp(input, "/e2e ", 5) == 0) {
            char *space_pos = strchr(input + 5, ' ');
            if (space_pos) {
                *space_pos            = '\0'; // Split recipient nickname and message
                const char *recipient = input + 5;
                const char *message   = space_pos + 1;

                if (strlen(message) > 0) {
                    if (client_send_e2e_message(g_client, message, recipient) < 0) {
                        printf("❌ Failed to send E2E message to %s\n", recipient);
                    } else {
                        printf("🔒 E2E message sent to %s\n", recipient);
                    }
                } else {
                    printf("Usage: /e2e <nickname> <message>\n");
                }
            } else {
                printf("Usage: /e2e <nickname> <message>\n");
            }
            continue;
        }

        // Check for help command to show E2E commands
        if (strcmp(input, "/help") == 0 || strcmp(input, "/e2ehelp") == 0) {
            printf("\n=== Available Commands ===\n");
            printf("🔒 E2E Encryption Commands:\n");
            printf("  /e2e <nickname> <message> - Send encrypted message to specific user\n");
            printf("  /peers                    - Show available E2E peers\n");
            printf("  /keyrefresh              - Re-broadcast your public key\n");
            printf("\n📢 Regular Commands:\n");
            printf("  /help                    - Show this help\n");
            printf("  /quit                    - Exit chat\n");
            printf("  (type normal messages for room chat)\n\n");
            continue;
        }

        // Check for peers list command
        if (strcmp(input, "/peers") == 0) {
            printf("\n🔑 Available E2E Peers:\n");
            if (g_client->e2e_ctx->num_peers == 0) {
                printf("  No peers available for E2E messaging\n");
            } else {
                for (int i = 0; i < g_client->e2e_ctx->num_peers; i++) {
                    if (g_client->e2e_ctx->peer_keys[i].active) {
                        printf("  🔐 %s\n", g_client->e2e_ctx->peer_keys[i].nickname);
                    }
                }
            }
            printf("Use: /e2e <nickname> <message> to send encrypted messages\n\n");
            continue;
        }

        // Check for key refresh command
        if (strcmp(input, "/keyrefresh") == 0) {
            if (client_broadcast_public_key(g_client) < 0) {
                printf("❌ Failed to re-broadcast public key\n");
            } else {
                printf("🔑 Public key re-broadcasted\n");
            }
            continue;
        }

        // Skip empty messages
        if (strlen(input) == 0) {
            continue;
        }

        if (!g_client->connected) {
            LOG_ERROR("Cannot send message: Not connected to server");
            break;
        }

        if (client_send_message(g_client, input) < 0) {
            LOG_ERROR("Failed to send message");
            break;
        }
    }

    // Cleanup and exit
    LOG_INFO("Exiting...");
    g_client->connected = 0;
    pthread_join(receive_thread, NULL);
    client_destroy(g_client);
    logger_cleanup();
    return 0;
}
