#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
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

static int yaml_indent_spaces(const char *line) {
    int indent = 0;
    while (*line == ' ') {
        indent++;
        line++;
    }
    return indent;
}

static int loadNodesByGroupFromYaml(const char *filename, Node nodes[], int max_nodes, const char *group_filter) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return -1;
    }

    char line[256];
    char current_group[MAX_GROUPNAME] = "";
    int in_groups_section = 0;
    int in_hosts_list = 0;
    int count = 0;

    while (fgets(line, sizeof(line), file) && count < max_nodes) {
        line[strcspn(line, "\r\n")] = 0;

        int indent = yaml_indent_spaces(line);
        char *content = line + indent;
        trim_whitespace(content);

        if (content[0] == '\0' || content[0] == '#') {
            continue;
        }

        if (indent == 0 && strcmp(content, "groups:") == 0) {
            in_groups_section = 1;
            in_hosts_list = 0;
            current_group[0] = '\0';
            continue;
        }

        if (!in_groups_section) {
            continue;
        }

        if (indent == 2) {
            size_t len = strlen(content);
            if (len > 1 && content[len - 1] == ':') {
                content[len - 1] = '\0';
                trim_whitespace(content);
                strncpy(current_group, content, MAX_GROUPNAME - 1);
                current_group[MAX_GROUPNAME - 1] = '\0';
                in_hosts_list = 0;
            }
            continue;
        }

        if (indent == 4 && strcmp(content, "hosts:") == 0) {
            in_hosts_list = 1;
            continue;
        }

        if (indent == 6 && in_hosts_list && content[0] == '-') {
            char *host = content + 1;
            trim_whitespace(host);
            if (host[0] == '\0') {
                continue;
            }

            if (group_filter && strcmp(group_filter, current_group) != 0) {
                continue;
            }

            strncpy(nodes[count].hostname, host, MAX_HOSTNAME - 1);
            nodes[count].hostname[MAX_HOSTNAME - 1] = '\0';
            strncpy(nodes[count].group, current_group, MAX_GROUPNAME - 1);
            nodes[count].group[MAX_GROUPNAME - 1] = '\0';
            count++;
            continue;
        }
    }

    fclose(file);
    return count;
}

int loadNodesByGroup(const char *filename, Node nodes[], int max_nodes, const char *group_filter) {
    if (strstr(filename, ".yaml") || strstr(filename, ".yml")) {
        int yaml_count = loadNodesByGroupFromYaml(filename, nodes, max_nodes, group_filter);
        if (yaml_count >= 0) {
            return yaml_count;
        }
        perror("fopen");
        return -1;
    }

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

static int set_socket_timeouts_ms(int sockfd, int timeout_ms) {
    struct timeval socket_timeout = {
        timeout_ms / 1000,
        (timeout_ms % 1000) * 1000
    };
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout)) < 0) {
        return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout)) < 0) {
        return -1;
    }
    return 0;
}

static int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, int timeout_ms) {
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

    struct timeval timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
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

static int parse_content_length(const char *response) {
    const char *content_len = strstr(response, "\r\nContent-Length:");
    if (!content_len) {
        return -1;
    }

    int length = -1;
    if (sscanf(content_len, "\r\nContent-Length: %d", &length) != 1) {
        return -1;
    }
    return length;
}

static int receive_http_response(int sockfd, char *buffer, int buffer_size, FetchState *state_out) {
    int total_received = 0;
    int header_end = -1;
    int content_length = -1;

    while (total_received < buffer_size - 1) {
        int bytes_received = recv(sockfd, buffer + total_received, buffer_size - 1 - total_received, 0);
        if (bytes_received > 0) {
            total_received += bytes_received;
            buffer[total_received] = '\0';

            if (header_end < 0) {
                char *header_ptr = strstr(buffer, "\r\n\r\n");
                if (header_ptr) {
                    header_end = (int)(header_ptr - buffer) + 4;
                    content_length = parse_content_length(buffer);
                    if (content_length >= 0) {
                        int needed = header_end + content_length;
                        if (total_received >= needed) {
                            return total_received;
                        }
                    }
                }
            } else if (content_length >= 0) {
                int needed = header_end + content_length;
                if (total_received >= needed) {
                    return total_received;
                }
            }
            continue;
        }

        if (bytes_received == 0) {
            return total_received;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *state_out = (total_received == 0) ? FETCH_TIMEOUT : FETCH_IO_ERROR;
            return -1;
        }

        *state_out = FETCH_IO_ERROR;
        return -1;
    }

    *state_out = FETCH_IO_ERROR;
    return -1;
}

static int elapsed_ms_since(const struct timespec *start_ts) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    long seconds = now.tv_sec - start_ts->tv_sec;
    long nanoseconds = now.tv_nsec - start_ts->tv_nsec;
    if (nanoseconds < 0) {
        seconds -= 1;
        nanoseconds += 1000000000L;
    }
    if (seconds < 0) {
        return 0;
    }
    return (int)(seconds * 1000L + nanoseconds / 1000000L);
}

static FetchResult fetch_status_internal(const char *hostname, int timeout_ms, int *persistent_sockfd) {
    FetchResult result;
    result.state = FETCH_CONNECT_ERROR;
    result.bytes_received = 0;
    result.latency_ms = 0;
    result.response[0] = '\0';
    struct timespec start_ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &start_ts);

    int use_persistent = (persistent_sockfd != NULL);
    int sockfd = use_persistent ? *persistent_sockfd : -1;
    int attempted_reconnect = 0;

retry_with_fresh_connection:
    struct addrinfo hints = {0}, *res = NULL, *rp = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, "9002", &hints, &res) != 0) {
        result.state = FETCH_DNS_ERROR;
        result.latency_ms = elapsed_ms_since(&start_ts);
        return result;
    }

    FetchState last_state = FETCH_CONNECT_ERROR;

    if (sockfd >= 0) {
        if (set_socket_timeouts_ms(sockfd, timeout_ms) < 0) {
            close(sockfd);
            sockfd = -1;
            if (use_persistent) {
                *persistent_sockfd = -1;
            }
        }
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (sockfd < 0) {
            sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sockfd < 0) {
                continue;
            }

            if (connect_with_timeout(sockfd, rp->ai_addr, rp->ai_addrlen, timeout_ms) < 0) {
                last_state = (errno == ETIMEDOUT) ? FETCH_TIMEOUT : FETCH_CONNECT_ERROR;
                close(sockfd);
                sockfd = -1;
                continue;
            }

            if (set_socket_timeouts_ms(sockfd, timeout_ms) < 0) {
                last_state = FETCH_IO_ERROR;
                close(sockfd);
                sockfd = -1;
                continue;
            }
        }

        char request_buffer[512];
        int request_len = snprintf(
            request_buffer,
            sizeof(request_buffer),
            use_persistent
                ? "GET /status HTTP/1.0\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n"
                : "GET /status HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
            hostname
        );
        if (request_len <= 0 || request_len >= (int)sizeof(request_buffer)) {
            last_state = FETCH_IO_ERROR;
            close(sockfd);
            sockfd = -1;
            break;
        }

        if (send(sockfd, request_buffer, (size_t)request_len, 0) < 0) {
            if (use_persistent && !attempted_reconnect) {
                attempted_reconnect = 1;
                close(sockfd);
                sockfd = -1;
                freeaddrinfo(res);
                goto retry_with_fresh_connection;
            }
            last_state = (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) ? FETCH_TIMEOUT : FETCH_IO_ERROR;
            close(sockfd);
            sockfd = -1;
            continue;
        }

        FetchState recv_state = FETCH_IO_ERROR;
        int total_received = receive_http_response(
            sockfd,
            result.response,
            RESPONSE_BUFFER_SIZE,
            &recv_state
        );

        if (total_received < 0) {
            if (use_persistent && !attempted_reconnect) {
                attempted_reconnect = 1;
                close(sockfd);
                sockfd = -1;
                freeaddrinfo(res);
                goto retry_with_fresh_connection;
            }
            last_state = recv_state;
            close(sockfd);
            sockfd = -1;
            continue;
        }

        if (total_received <= 0) {
            if (use_persistent && !attempted_reconnect) {
                attempted_reconnect = 1;
                close(sockfd);
                sockfd = -1;
                freeaddrinfo(res);
                goto retry_with_fresh_connection;
            }
            close(sockfd);
            sockfd = -1;
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

        if (use_persistent) {
            *persistent_sockfd = sockfd;
        } else {
            close(sockfd);
        }
        freeaddrinfo(res);
        result.latency_ms = elapsed_ms_since(&start_ts);
        return result;
    }

    if (sockfd >= 0) {
        close(sockfd);
    }
    if (use_persistent) {
        *persistent_sockfd = -1;
    }
    freeaddrinfo(res);
    result.state = last_state;
    result.latency_ms = elapsed_ms_since(&start_ts);
    return result;
}

FetchResult fetchStatus(const char *hostname, int timeout_ms) {
    return fetch_status_internal(hostname, timeout_ms, NULL);
}

FetchResult fetchStatusWithConnection(const char *hostname, int timeout_ms, int *persistent_sockfd) {
    return fetch_status_internal(hostname, timeout_ms, persistent_sockfd);
}
