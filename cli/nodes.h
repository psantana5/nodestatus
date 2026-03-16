#ifndef NODES_H
#define NODES_H

#define MAX_NODES 128
#define MAX_HOSTNAME 256
#define RESPONSE_BUFFER_SIZE 2048
#define NODE_TIMEOUT_SECONDS 2

typedef struct {
    char hostname[MAX_HOSTNAME];
} Node;

typedef enum {
    FETCH_OK = 0,
    FETCH_TIMEOUT,
    FETCH_DNS_ERROR,
    FETCH_CONNECT_ERROR,
    FETCH_HTTP_ERROR,
    FETCH_PARSE_ERROR,
    FETCH_IO_ERROR
} FetchState;

typedef struct {
    FetchState state;
    int bytes_received;
    char response[RESPONSE_BUFFER_SIZE];
} FetchResult;

int loadNodes(const char *filename, Node *nodes, int max_nodes);
FetchResult fetchStatus(const char *hostname, int timeout_seconds);

#endif // NODES_H
