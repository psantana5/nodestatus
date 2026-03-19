#ifndef HTTP_H
#define HTTP_H

#include "metrics.h"

char* parseRequestPath(const char *request);
int buildJsonResponse(char *buffer, int buffer_size, const SystemMetrics *metrics);
int sendHttpResponse(int client_socket, int status_code, const char *status_text, const char *body, const char *content_type, int keep_alive);

#endif
