#include <stdio.h>
#include <string.h>
#include "table.h"

static const char *fetch_state_to_string(FetchState state) {
    switch (state) {
        case FETCH_OK:
            return "OK";
        case FETCH_TIMEOUT:
            return "TIMEOUT";
        case FETCH_DNS_ERROR:
            return "DNS_ERROR";
        case FETCH_CONNECT_ERROR:
            return "CONNECT_ERR";
        case FETCH_HTTP_ERROR:
            return "HTTP_ERROR";
        case FETCH_PARSE_ERROR:
            return "PARSE_ERR";
        case FETCH_IO_ERROR:
            return "IO_ERROR";
        default:
            return "UNKNOWN";
    }
}

void printTableHeader() {
    printf("%-20s %-11s %-8s %-8s %-8s %-8s\n", "HOST", "STATE", "CPU(%)", "MEM(%)", "LOAD", "DISK (MB/s)");
    printf("-------------------------------------------------------------------------\n");
}

void printTableRow(const NodeStatus *status) {
    if (status->has_metrics) {
        printf(
            "%-20s %-11s %6.1f%% %6.1f%% %7.2f %7.2f\n",
            status->hostname,
            fetch_state_to_string(status->state),
            status->cpu_percent,
            status->mem_percent,
            status->load,
            status->disk_mb_s
        );
        return;
    }

    printf(
        "%-20s %-11s %-8s %-8s %-8s %-8s\n",
        status->hostname,
        fetch_state_to_string(status->state),
        "N/A",
        "N/A",
        "N/A",
        "N/A"
    );
}

void printTableFooter(int ok_count, int fail_count) {
    printf("-------------------------------------------------------------------------\n");
    printf("OK: %d  Failed: %d\n\n", ok_count, fail_count);
}

int parseJsonMetrics(const char *json, NodeStatus *status) {
    if (!json || !status) {
        return -1;
    }

    status->cpu_percent = 0.0f;
    status->mem_percent = 0.0f;
    status->load = 0.0f;
    status->disk_mb_s = 0.0f;

    int matches = 0;

    const char *cpu_ptr = strstr(json, "\"cpuBusyPercent\":");
    if (cpu_ptr) {
        if (sscanf(cpu_ptr, "\"cpuBusyPercent\":%f", &status->cpu_percent) == 1) {
            matches++;
        }
    }

    const char *mem_ptr = strstr(json, "\"memUsedPercent\":");
    if (mem_ptr) {
        if (sscanf(mem_ptr, "\"memUsedPercent\":%f", &status->mem_percent) == 1) {
            matches++;
        }
    }

    const char *load_ptr = strstr(json, "\"load1\":");
    if (load_ptr) {
        if (sscanf(load_ptr, "\"load1\":%f", &status->load) == 1) {
            matches++;
        }
    }

    const char *disk_ptr = strstr(json, "\"diskTotalMBps\":");
    if (disk_ptr) {
        if (sscanf(disk_ptr, "\"diskTotalMBps\":%f", &status->disk_mb_s) == 1) {
            matches++;
        }
    }

    return (matches == 4) ? 0 : -1;
}
