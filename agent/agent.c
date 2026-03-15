#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "metrics.h"
#include "http.h"

// Author: Pau Santana
// Date: 2026-03-15
// Latest revision: 2026-03-15
// Current version: 0.5

// If you are reading this, peace and love to you <3

// Simple HTTP server to handle requests and return system metrics
// Listens on port 9002 and responds to /status with JSON metrics
// For simplicity, this server handles one request at a time and does not implement concurrency

int socketServer(){
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
    address.sin_port = htons(9002); // Convert port number to network byte order

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) { // Bind the socket to the specified address and port
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

    printf("Server listening on port 9002...\n");

    while (1) { // Main loop to accept and handle incoming connections
        int client_socket; // Socket for the accepted client connection
        socklen_t addrlen = sizeof(address); // Length of the address structure, needed for accept()
        client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen); // Accept an incoming connection
        if (client_socket < 0) { // Same POSIX standard with socket() as earlier
            perror("accept");
            continue;
        }

        char request[1024];
        ssize_t bytes = read(client_socket, request, sizeof(request) - 1); // Read the client's request into the buffer, leaving space for a null terminator
        if (bytes > 0) {
            request[bytes] = '\0'; // Null-terminate the request string to safely use it as a C string
        } else {
            request[0] = '\0'; // If read fails, we set the request to an empty string to avoid undefined behavior when processing it
        }

        char *path = parseRequestPath(request); // Extract the requested path from the HTTP request
        if (path) {
            printf("Request: %s\n", path); // Log the requested path for debugging purposes. This can help us verify that the server is correctly parsing incoming requests and can be useful for troubleshooting issues with client requests.
        }

        char body[512]; // Buffer to hold the response body, which will contain the JSON metrics if the request is for /status
        
        if (path && strcmp(path, "/health") == 0) { // If the requested path is /health, we can return a simple response indicating that the server is running. This can be used for health checks by monitoring systems.
            sendHttpResponse(client_socket, "OK", "text/plain");
        }
        else if (path && strcmp(path, "/status") == 0) { // If the requested path is /status, we gather the system metrics and build a JSON response
            LoadMetrics metrics = getLoadAverage(); // Get the system load average metrics
            MemoryMetrics memMetrics = getMemoryMetrics(); // Get the memory metrics
            CpuMetrics cpuMetrics = getCpuMetrics(); // Get the CPU usage metrics
            buildJsonResponse(body, sizeof(body), metrics, memMetrics, cpuMetrics); // Build a JSON response string containing the metrics, which will be sent back to the client
            sendHttpResponse(client_socket, body, "application/json"); // Send the HTTP response with the JSON body and the appropriate content type header
        } else {
            sendHttpResponse(client_socket, "404 Not Found", "text/plain");
        }
        close(client_socket);
        
    }

    close(server_socket);
    return 0;
}

int main(){
    printf("Agent starting...\n");
    socketServer();
    return 0;
}
