#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
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

int buildJsonResponse(char *buffer, int buffer_size, const SystemMetrics *metrics) {
    if (metrics == NULL) {
        return -1;
    }

    return snprintf(buffer, buffer_size,
        "{\"sampleTsMs\":%llu,\"sampleAgeMs\":%llu,\"load1\":%.2f,\"load5\":%.2f,\"load15\":%.2f,\"memTotal\":%lu,\"memFree\":%lu,\"memAvailable\":%lu,\"memUsed\":%lu,\"memUsedPercent\":%.2f,\"cpuUser\":%llu,\"cpuNice\":%llu,\"cpuSystem\":%llu,\"cpuIdle\":%llu,\"cpuIOwait\":%llu,\"cpuBusy\":%llu,\"cpuBusyPercent\":%.2f,\"diskReadMBps\":%.2f,\"diskWriteMBps\":%.2f,\"diskTotalMBps\":%.2f}",
        (unsigned long long)metrics->sampleTsMs,
        (unsigned long long)metrics->sampleAgeMs,
        metrics->load.load1,
        metrics->load.load5,
        metrics->load.load15,
        metrics->memory.MemTotal,
        metrics->memory.MemFree,
        metrics->memory.MemAvailable,
        metrics->memory.memUsed,
        metrics->memory.memUsedPercent,
        metrics->cpu.user,
        metrics->cpu.nice,
        metrics->cpu.system,
        metrics->cpu.idle,
        metrics->cpu.iowait,
        metrics->cpu.busy,
        metrics->cpu.busyPercent,
        metrics->disk.readMBps,
        metrics->disk.writeMBps,
        metrics->disk.totalMBps);
}

int sendHttpResponse(int client_socket, int status_code, const char *status_text, const char *body, const char *content_type) {
    char response[4096];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.0 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code,
        status_text,
        content_type,
        strlen(body),
        body);

    if (response_len <= 0 || response_len >= (int)sizeof(response)) {
        return -1;
    }

    int total_sent = 0;
    while (total_sent < response_len) {
        ssize_t sent = send(client_socket, response + total_sent, (size_t)(response_len - total_sent), 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            return -1;
        }
        total_sent += (int)sent;
    }
    return 0;
}
