#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include "http.h"

char* parseRequestPath(const char *request) {
    static char path[256];
    path[0] = '\0';
    
    if (request == NULL || request[0] == '\0') {
        strcpy(path, "/");
        return path;
    }
    
    const char *start = request;
    if (strncmp(request, "GET ", 4) == 0) {
        start = request + 4;
    }
    
    int i = 0;
    while (start[i] && start[i] != ' ' && i < 255) {
        path[i] = start[i];
        i++;
    }
    path[i] = '\0';
    
    return path;
}

int buildJsonResponse(char *buffer, int buffer_size, LoadMetrics metrics, MemoryMetrics memMetrics) {
    return snprintf(buffer, buffer_size,
        "{\"load1\":%.2f,\"load5\":%.2f,\"load15\":%.2f,\"memTotal\":%lu,\"memFree\":%lu,\"memAvailable\":%lu,\"memUsed\":%lu,\"memUsedPercent\":%.2f}",
        metrics.load1, metrics.load5, metrics.load15, memMetrics.MemTotal, memMetrics.MemFree, memMetrics.MemAvailable, memMetrics.memUsed, memMetrics.memUsedPercent);
}

void sendHttpResponse(int client_socket, const char *body, const char *content_type) {
    char response[1024];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        content_type,
        strlen(body),
        body);
    
    if (response_len > 0) {
        send(client_socket, response, response_len, 0);
    }
}
