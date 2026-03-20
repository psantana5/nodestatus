#include <stdio.h>
#include <string.h>
#include "table.h"

static int table_color_enabled = 1;

static const char *color_for_state(FetchState state) {
    switch (state) {
        case FETCH_OK:
            return "\033[32m";
        case FETCH_TIMEOUT:
        case FETCH_DNS_ERROR:
        case FETCH_CONNECT_ERROR:
        case FETCH_HTTP_ERROR:
        case FETCH_PARSE_ERROR:
        case FETCH_IO_ERROR:
            return "\033[31m";
        default:
            return "\033[33m";
    }
}

static const char *color_for_latency(int latency_ms) {
    if (latency_ms <= 100) {
        return "\033[32m";
    }
    if (latency_ms <= 300) {
        return "\033[33m";
    }
    return "\033[31m";
}

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

void setTableColorEnabled(int enabled) {
    table_color_enabled = enabled ? 1 : 0;
}

void printTableHeader() {
    printf("%-20s %-11s %-8s %-8s %-8s %-8s %-11s\n", "HOST", "STATE", "RESP(ms)", "CPU(%)", "MEM(%)", "LOAD", "DISK (MB/s)");
    printf("------------------------------------------------------------------------------------\n");
}

void printTableRow(const NodeStatus *status) {
    const char *state_str = fetch_state_to_string(status->state);
    const char *state_color = table_color_enabled ? color_for_state(status->state) : "";
    const char *latency_color = table_color_enabled ? color_for_latency(status->latency_ms) : "";
    const char *color_reset = table_color_enabled ? "\033[0m" : "";

    if (status->has_metrics) {
        printf(
            "%-20s %s%-11s%s %s%8d%s %6.1f%% %6.1f%% %7.2f %7.2f\n",
            status->hostname,
            state_color,
            state_str,
            color_reset,
            latency_color,
            status->latency_ms,
            color_reset,
            status->cpu_percent,
            status->mem_percent,
            status->load,
            status->disk_mb_s
        );
        return;
    }

    printf(
        "%-20s %s%-11s%s %s%8d%s %-8s %-8s %-8s %-11s\n",
        status->hostname,
        state_color,
        state_str,
        color_reset,
        latency_color,
        status->latency_ms,
        color_reset,
        "N/A",
        "N/A",
        "N/A",
        "N/A"
    );
}

void printTableFooter(int ok_count, int fail_count) {
    printf("------------------------------------------------------------------------------------\n");
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

void printCpuDetailHeader() {
    printf("%-20s %-11s %8s %8s %8s %8s %8s %8s\n", 
           "HOST", "STATE", "USER%", "NICE%", "SYS%", "IDLE%", "IOWAIT%", "BUSY%");
    printf("------------------------------------------------------------------------------\n");
}

void printCpuDetailRow(const CpuDetailStatus *status) {
    const char *state_str = fetch_state_to_string(status->state);
    const char *state_color = table_color_enabled ? color_for_state(status->state) : "";
    const char *color_reset = table_color_enabled ? "\033[0m" : "";

    if (status->has_metrics) {
        printf("%-20s %s%-11s%s %7.1f%% %7.1f%% %7.1f%% %7.1f%% %7.1f%% %7.1f%%\n",
               status->hostname,
               state_color,
               state_str,
               color_reset,
               status->user_percent,
               status->nice_percent,
               status->system_percent,
               status->idle_percent,
               status->iowait_percent,
               status->busy_percent);
        return;
    }

    printf("%-20s %s%-11s%s %8s %8s %8s %8s %8s %8s\n",
           status->hostname,
           state_color,
           state_str,
           color_reset,
           "N/A", "N/A", "N/A", "N/A", "N/A", "N/A");
}

int parseJsonCpuDetail(const char *json, CpuDetailStatus *status) {
    if (!json || !status) {
        return -1;
    }

    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
    float busy_percent = 0.0f;

    const char *cpu_user = strstr(json, "\"cpuUser\":");
    const char *cpu_nice = strstr(json, "\"cpuNice\":");
    const char *cpu_system = strstr(json, "\"cpuSystem\":");
    const char *cpu_idle = strstr(json, "\"cpuIdle\":");
    const char *cpu_iowait = strstr(json, "\"cpuIOwait\":");
    const char *cpu_busy_pct = strstr(json, "\"cpuBusyPercent\":");

    if (!cpu_user || !cpu_nice || !cpu_system || !cpu_idle || !cpu_iowait || !cpu_busy_pct) {
        return -1;
    }

    sscanf(cpu_user, "\"cpuUser\":%llu", &user);
    sscanf(cpu_nice, "\"cpuNice\":%llu", &nice);
    sscanf(cpu_system, "\"cpuSystem\":%llu", &system);
    sscanf(cpu_idle, "\"cpuIdle\":%llu", &idle);
    sscanf(cpu_iowait, "\"cpuIOwait\":%llu", &iowait);
    sscanf(cpu_busy_pct, "\"cpuBusyPercent\":%f", &busy_percent);

    unsigned long long total = user + nice + system + idle + iowait;
    if (total > 0) {
        status->user_percent = ((float)user / total) * 100.0f;
        status->nice_percent = ((float)nice / total) * 100.0f;
        status->system_percent = ((float)system / total) * 100.0f;
        status->idle_percent = ((float)idle / total) * 100.0f;
        status->iowait_percent = ((float)iowait / total) * 100.0f;
        status->busy_percent = busy_percent;
        return 0;
    }

    return -1;
}

void printMemDetailHeader() {
    printf("%-20s %-11s %10s %10s %8s %10s %10s %8s\n",
           "HOST", "STATE", "TOTAL(MB)", "AVAIL(MB)", "MEM%", "SWAP(MB)", "USED(MB)", "SWAP%");
    printf("--------------------------------------------------------------------------------------------\n");
}

void printMemDetailRow(const MemDetailStatus *status) {
    const char *state_str = fetch_state_to_string(status->state);
    const char *state_color = table_color_enabled ? color_for_state(status->state) : "";
    const char *color_reset = table_color_enabled ? "\033[0m" : "";

    if (status->has_metrics) {
        printf("%-20s %s%-11s%s %10lu %10lu %7.1f%% %10lu %10lu %7.1f%%\n",
               status->hostname,
               state_color,
               state_str,
               color_reset,
               status->mem_total_mb,
               status->mem_avail_mb,
               status->mem_percent,
               status->swap_total_mb,
               status->swap_used_mb,
               status->swap_percent);
        return;
    }

    printf("%-20s %s%-11s%s %10s %10s %8s %10s %10s %8s\n",
           status->hostname,
           state_color,
           state_str,
           color_reset,
           "N/A", "N/A", "N/A", "N/A", "N/A", "N/A");
}

int parseJsonMemDetail(const char *json, MemDetailStatus *status) {
    if (!json || !status) {
        return -1;
    }

    unsigned long mem_total = 0, mem_avail = 0, swap_total = 0, swap_used = 0;
    float mem_pct = 0.0f, swap_pct = 0.0f;

    const char *ptr;
    
    if ((ptr = strstr(json, "\"memTotal\":")) != NULL) {
        sscanf(ptr, "\"memTotal\":%lu", &mem_total);
    }
    if ((ptr = strstr(json, "\"memAvailable\":")) != NULL) {
        sscanf(ptr, "\"memAvailable\":%lu", &mem_avail);
    }
    if ((ptr = strstr(json, "\"memUsedPercent\":")) != NULL) {
        sscanf(ptr, "\"memUsedPercent\":%f", &mem_pct);
    }
    if ((ptr = strstr(json, "\"swapTotal\":")) != NULL) {
        sscanf(ptr, "\"swapTotal\":%lu", &swap_total);
    }
    if ((ptr = strstr(json, "\"swapUsed\":")) != NULL) {
        sscanf(ptr, "\"swapUsed\":%lu", &swap_used);
    }
    if ((ptr = strstr(json, "\"swapUsedPercent\":")) != NULL) {
        sscanf(ptr, "\"swapUsedPercent\":%f", &swap_pct);
    }

    status->mem_total_mb = mem_total / 1024;
    status->mem_avail_mb = mem_avail / 1024;
    status->mem_percent = mem_pct;
    status->swap_total_mb = swap_total / 1024;
    status->swap_used_mb = swap_used / 1024;
    status->swap_percent = swap_pct;

    return 0;
}

void printDiskDetailHeader() {
    printf("%-20s %-11s %10s %10s %10s %10s %10s %10s\n",
           "HOST", "STATE", "READ(MB/s)", "WRITE(MB/s)", "TOTAL(MB/s)", "R_IOPS", "W_IOPS", "T_IOPS");
    printf("----------------------------------------------------------------------------------------------------\n");
}

void printDiskDetailRow(const DiskDetailStatus *status) {
    const char *state_str = fetch_state_to_string(status->state);
    const char *state_color = table_color_enabled ? color_for_state(status->state) : "";
    const char *color_reset = table_color_enabled ? "\033[0m" : "";

    if (status->has_metrics) {
        printf("%-20s %s%-11s%s %10.2f %10.2f %10.2f %10.1f %10.1f %10.1f\n",
               status->hostname,
               state_color,
               state_str,
               color_reset,
               status->read_mbps,
               status->write_mbps,
               status->total_mbps,
               status->read_iops,
               status->write_iops,
               status->total_iops);
        return;
    }

    printf("%-20s %s%-11s%s %10s %10s %10s %10s %10s %10s\n",
           status->hostname,
           state_color,
           state_str,
           color_reset,
           "N/A", "N/A", "N/A", "N/A", "N/A", "N/A");
}

int parseJsonDiskDetail(const char *json, DiskDetailStatus *status) {
    if (!json || !status) {
        return -1;
    }

    const char *ptr;
    
    if ((ptr = strstr(json, "\"diskReadMBps\":")) != NULL) {
        sscanf(ptr, "\"diskReadMBps\":%f", &status->read_mbps);
    }
    if ((ptr = strstr(json, "\"diskWriteMBps\":")) != NULL) {
        sscanf(ptr, "\"diskWriteMBps\":%f", &status->write_mbps);
    }
    if ((ptr = strstr(json, "\"diskTotalMBps\":")) != NULL) {
        sscanf(ptr, "\"diskTotalMBps\":%f", &status->total_mbps);
    }
    if ((ptr = strstr(json, "\"diskReadIOPS\":")) != NULL) {
        sscanf(ptr, "\"diskReadIOPS\":%f", &status->read_iops);
    }
    if ((ptr = strstr(json, "\"diskWriteIOPS\":")) != NULL) {
        sscanf(ptr, "\"diskWriteIOPS\":%f", &status->write_iops);
    }
    if ((ptr = strstr(json, "\"diskTotalIOPS\":")) != NULL) {
        sscanf(ptr, "\"diskTotalIOPS\":%f", &status->total_iops);
    }

    return 0;
}

void printNetDetailHeader() {
    printf("%-20s %-11s %12s %12s %12s\n",
           "HOST", "STATE", "RX(MB/s)", "TX(MB/s)", "TOTAL(MB/s)");
    printf("--------------------------------------------------------------------\n");
}

void printNetDetailRow(const NetDetailStatus *status) {
    const char *state_str = fetch_state_to_string(status->state);
    const char *state_color = table_color_enabled ? color_for_state(status->state) : "";
    const char *color_reset = table_color_enabled ? "\033[0m" : "";

    if (status->has_metrics) {
        printf("%-20s %s%-11s%s %12.2f %12.2f %12.2f\n",
               status->hostname,
               state_color,
               state_str,
               color_reset,
               status->rx_mbps,
               status->tx_mbps,
               status->total_mbps);
        return;
    }

    printf("%-20s %s%-11s%s %12s %12s %12s\n",
           status->hostname,
           state_color,
           state_str,
           color_reset,
           "N/A", "N/A", "N/A");
}

int parseJsonNetDetail(const char *json, NetDetailStatus *status) {
    if (!json || !status) {
        return -1;
    }

    const char *ptr;
    
    if ((ptr = strstr(json, "\"netRxMBps\":")) != NULL) {
        sscanf(ptr, "\"netRxMBps\":%f", &status->rx_mbps);
    }
    if ((ptr = strstr(json, "\"netTxMBps\":")) != NULL) {
        sscanf(ptr, "\"netTxMBps\":%f", &status->tx_mbps);
    }
    if ((ptr = strstr(json, "\"netTotalMBps\":")) != NULL) {
        sscanf(ptr, "\"netTotalMBps\":%f", &status->total_mbps);
    }

    return 0;
}
