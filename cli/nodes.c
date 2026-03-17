#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include "nodes.h"

static void trim_whitespace(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }

    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') {
        start++;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

int loadNodesByGroup(const char *filename, Node nodes[], int max_nodes, const char *group_filter) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    char line[256];
    char current_group[MAX_GROUPNAME] = "default";
    int count = 0;

    while (fgets(line, sizeof(line), file) && count < max_nodes) {
        line[strcspn(line, "\r\n")] = 0;
        trim_whitespace(line);

        if (line[0] == '\0') {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        size_t len = strlen(line);
        if (line[0] == '[' && len > 2 && line[len - 1] == ']') {
            line[len - 1] = '\0';
            strncpy(current_group, line + 1, MAX_GROUPNAME - 1);
            current_group[MAX_GROUPNAME - 1] = '\0';
            trim_whitespace(current_group);
            if (current_group[0] == '\0') {
                strncpy(current_group, "default", MAX_GROUPNAME - 1);
                current_group[MAX_GROUPNAME - 1] = '\0';
            }
            continue;
        }

        if (group_filter && strcmp(group_filter, current_group) != 0) {
            continue;
        }

        strncpy(nodes[count].hostname, line, MAX_HOSTNAME - 1);
        nodes[count].hostname[MAX_HOSTNAME - 1] = '\0';
        strncpy(nodes[count].group, current_group, MAX_GROUPNAME - 1);
        nodes[count].group[MAX_GROUPNAME - 1] = '\0';
        count++;
    }

    fclose(file);
    return count;
}

int loadNodes(const char *filename, Node nodes[], int max_nodes) {
    return loadNodesByGroup(filename, nodes, max_nodes, NULL);
}

static int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, int timeout_seconds) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }

    if (connect(sockfd, addr, addrlen) == 0) {
        (void)fcntl(sockfd, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS) {
        return -1;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sockfd, &write_fds);

    struct timeval timeout = {timeout_seconds, 0};
    int ready = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
    if (ready <= 0) {
        errno = (ready == 0) ? ETIMEDOUT : errno;
        return -1;
    }

    int so_error = 0;
    socklen_t so_len = sizeof(so_error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &so_len) < 0) {
        return -1;
    }
    if (so_error != 0) {
        errno = so_error;
        return -1;
    }

    if (fcntl(sockfd, F_SETFL, flags) < 0) {
        return -1;
    }
    return 0;
}

FetchResult fetchStatus(const char *hostname, int timeout_seconds) {
    FetchResult result;
    result.state = FETCH_CONNECT_ERROR;
    result.bytes_received = 0;
    result.response[0] = '\0';

    struct addrinfo hints = {0}, *res = NULL, *rp = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, "9002", &hints, &res) != 0) {
        result.state = FETCH_DNS_ERROR;
        return result;
    }

    FetchState last_state = FETCH_CONNECT_ERROR;

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        int sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (connect_with_timeout(sockfd, rp->ai_addr, rp->ai_addrlen, timeout_seconds) < 0) {
            last_state = (errno == ETIMEDOUT) ? FETCH_TIMEOUT : FETCH_CONNECT_ERROR;
            close(sockfd);
            continue;
        }

        struct timeval socket_timeout = {timeout_seconds, 0};
        (void)setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
        (void)setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout));

        char request_buffer[512];
        int request_len = snprintf(
            request_buffer,
            sizeof(request_buffer),
            "GET /status HTTP/1.0\r\nHost: %s\r\n\r\n",
            hostname
        );
        if (request_len <= 0 || request_len >= (int)sizeof(request_buffer)) {
            result.state = FETCH_IO_ERROR;
            close(sockfd);
            break;
        }

        if (send(sockfd, request_buffer, (size_t)request_len, 0) < 0) {
            last_state = (errno == EAGAIN || errno == EWOULDBLOCK) ? FETCH_TIMEOUT : FETCH_IO_ERROR;
            close(sockfd);
            continue;
        }

        int total_received = 0;
        while (total_received < RESPONSE_BUFFER_SIZE - 1) {
            int bytes_received = recv(
                sockfd,
                result.response + total_received,
                RESPONSE_BUFFER_SIZE - 1 - total_received,
                0
            );

            if (bytes_received > 0) {
                total_received += bytes_received;
                continue;
            }

            if (bytes_received == 0) {
                break;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (total_received == 0) {
                    last_state = FETCH_TIMEOUT;
                }
                break;
            }

            last_state = FETCH_IO_ERROR;
            total_received = 0;
            break;
        }

        if (total_received <= 0) {
            close(sockfd);
            continue;
        }

        result.response[total_received] = '\0';
        result.bytes_received = total_received;

        int http_code = 0;
        if (sscanf(result.response, "HTTP/%*d.%*d %d", &http_code) != 1) {
            result.state = FETCH_HTTP_ERROR;
        } else if (http_code != 200) {
            result.state = FETCH_HTTP_ERROR;
        } else {
            result.state = FETCH_OK;
        }

        close(sockfd);
        freeaddrinfo(res);
        return result;
    }

    freeaddrinfo(res);
    result.state = last_state;
    return result;
}
