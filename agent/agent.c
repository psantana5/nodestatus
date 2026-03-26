#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "metrics.h"
#include "http.h"

// Author: Pau Santana
// Date: 2026-03-15
// Latest revision: 2026-03-15
// Current version for agent.c: 1.0

// If you are reading this, peace and love to you <3

// Simple HTTP server to handle requests and return system metrics
// Listens on port 9002 and responds to /status with JSON metrics
// Supports HTTP keep-alive connections with queued connection backlog

static void print_usage(const char *progname) {
    printf("Usage: %s [-p port|--port port|--port=port]\n", progname);
}

static int parse_port_value(const char *text, int *port_out) {
    if (!text || !*text) {
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    long value = strtol(text, &endptr, 10);
    if (errno != 0 || endptr == text || *endptr != '\0' || value < 1 || value > 65535) {
        return -1;
    }

    *port_out = (int)value;
    return 0;
}

int socketServer(int port){
    if (initMetricsSampler() != 0) {
        perror("initMetricsSampler");
        return 1;
    }

    int server_socket;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) { // In standard POSIX sockets, socket() returns -1 on error.
        perror("socket"); 
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { // This allows us to reuse the address if the server is restarted quickly
        perror("setsockopt"); // This is not a critical error, so we can continue even if it fails
        close(server_socket);
        return 1;
    }

    struct sockaddr_in address = {0}; // Zero-initialize the structure to avoid any garbage values
    address.sin_family = AF_INET; // Use IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    address.sin_port = htons((unsigned short)port); // Convert port number to network byte order

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) { // Bind the socket to the specified address and port
        fprintf(stderr, "bind failed on port %d\n", port);
        perror("bind");
        close(server_socket);
        return 1;
    }

    // Listen for incoming connections (backlog of 16)
    if (listen(server_socket, 16) < 0) {
        perror("listen");
        close(server_socket);
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    while (1) { // Main loop to accept and handle incoming connections
        int client_socket; // Socket for the accepted client connection
        socklen_t addrlen = sizeof(address); // Length of the address structure, needed for accept()
        client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen); // Accept an incoming connection
        if (client_socket < 0) { // Same POSIX standard with socket() as earlier
            perror("accept");
            continue;
        }

        while (1) {
            char request[1024];
            ssize_t bytes = read(client_socket, request, sizeof(request) - 1); // Read the client's request into the buffer, leaving space for a null terminator
            if (bytes > 0) {
                request[bytes] = '\0'; // Null-terminate the request string to safely use it as a C string
            } else {
                break;
            }

            char *path = parseRequestPath(request); // Extract the requested path from the HTTP request
            if (path) {
                printf("Request: %s\n", path); // Log the requested path for debugging purposes. This can help us verify that the server is correctly parsing incoming requests and can be useful for troubleshooting issues with client requests.
            }

            int keep_alive = (strstr(request, "Connection: keep-alive") != NULL);
            char body[2048]; // Buffer to hold the response body, which will contain the JSON metrics if the request is for /status
            
            if (path && strcmp(path, "/health") == 0) { // If the requested path is /health, we can return a simple response indicating that the server is running. This can be used for health checks by monitoring systems.
                (void)sendHttpResponse(client_socket, 200, "OK", "OK", "text/plain", keep_alive);
            }
            else if (path && strcmp(path, "/status") == 0) { // If the requested path is /status, we gather the system metrics and build a JSON response
                SystemMetrics snapshot = {0};
                if (getMetricsSnapshot(&snapshot) != 0) {
                    (void)sendHttpResponse(client_socket, 500, "Internal Server Error", "Internal Server Error", "text/plain", 0);
                    break;
                }

                int body_len = buildJsonResponse(body, sizeof(body), &snapshot); // Build a JSON response string containing the metrics, which will be sent back to the client
                if (body_len > 0 && body_len < (int)sizeof(body)) {
                    (void)sendHttpResponse(client_socket, 200, "OK", body, "application/json", keep_alive); // Send the HTTP response with the JSON body and the appropriate content type header
                } else {
                    (void)sendHttpResponse(client_socket, 500, "Internal Server Error", "Internal Server Error", "text/plain", 0);
                    break;
                }
            } else {
                (void)sendHttpResponse(client_socket, 404, "Not Found", "404 Not Found", "text/plain", keep_alive);
            }

            if (!keep_alive) {
                break;
            }
        }
        close(client_socket);
        
    }

    close(server_socket);
    stopMetricsSampler();
    return 0;
}

int main(int argc, char *argv[]){
    int port = 9002;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc || parse_port_value(argv[i + 1], &port) != 0) {
                fprintf(stderr, "Invalid port value\n");
                print_usage(argv[0]);
                return 1;
            }
            i++;
            continue;
        }

        if (strncmp(argv[i], "--port=", 7) == 0) {
            if (parse_port_value(argv[i] + 7, &port) != 0) {
                fprintf(stderr, "Invalid port value\n");
                print_usage(argv[0]);
                return 1;
            }
            continue;
        }

        fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }

    printf("Agent starting on port %d...\n", port);
    return socketServer(port);
}
