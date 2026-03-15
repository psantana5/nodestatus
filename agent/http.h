#ifndef HTTP_H
#define HTTP_H

#include "metrics.h"

char* parseRequestPath(const char *request);
int buildJsonResponse(char *buffer, int buffer_size, LoadMetrics metrics, MemoryMetrics memMetrics);
void sendHttpResponse(int client_socket, const char *body, const char *content_type);

#endif
