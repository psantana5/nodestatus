#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "nodes.h"

int loadNodes(const char *filename, Node nodes[], int max_nodes) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    char line[256];
    int count = 0;

    while (fgets(line, sizeof(line), file) && count < max_nodes) {
        // Remove newline character if present
        line[strcspn(line, "\r\n")] = 0;
        strncpy(nodes[count].hostname, line, MAX_HOSTNAME - 1);
        nodes[count].hostname[MAX_HOSTNAME - 1] = '\0'; // Ensure null-termination
        count++;
    }

    fclose(file);
    return count;
}

int fetchStatus(const char *hostname, char *response, int response_size) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, "9002", &hints, &res) != 0) {
        perror("getaddrinfo");
        return -1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    const char *request = "GET /status HTTP/1.0\r\nHost: %s\r\n\r\n";
    char request_buffer[512];
    snprintf(request_buffer, sizeof(request_buffer), request, hostname);

    if (send(sockfd, request_buffer, strlen(request_buffer), 0) < 0) {
        perror("send");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    int bytes_received = recv(sockfd, response, response_size - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    response[bytes_received] = '\0'; // Null-terminate the response
    close(sockfd);
    freeaddrinfo(res);
    return bytes_received;
}