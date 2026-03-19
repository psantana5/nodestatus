#ifndef HTTP_H
#define HTTP_H

#include "metrics.h"

char* parseRequestPath(const char *request);
int buildJsonResponse(char *buffer, int buffer_size, LoadMetrics metrics, MemoryMetrics memMetrics, CpuMetrics cpuMetrics, DiskMetrics diskMetrics);
int sendHttpResponse(int client_socket, int status_code, const char *status_text, const char *body, const char *content_type);

#endif
