#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "metrics.h"

LoadMetrics getLoadAverage() {
    LoadMetrics metrics = {0, 0, 0};
    FILE *fp = fopen("/proc/loadavg", "r");
    
    if (fp == NULL) {
        perror("fopen");
        return metrics;
    }
    
    int result = fscanf(fp, "%f %f %f", &metrics.load1, &metrics.load5, &metrics.load15);
    
    if (result != 3) {
        perror("fscanf");
    }
    
    fclose(fp);
    return metrics;
}

MemoryMetrics getMemoryMetrics() {
    MemoryMetrics metrics = {0, 0, 0, 0, 0}; 
    FILE *fp = fopen("/proc/meminfo", "r");
    
    if (fp == NULL) {
        perror("fopen");
        return metrics;
    }
    

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &metrics.MemTotal) == 1) {
            continue;
        }
        if (sscanf(line, "MemFree: %lu kB", &metrics.MemFree) == 1) {
            continue;
        }
        if (sscanf(line, "MemAvailable: %lu kB", &metrics.MemAvailable) == 1) {
            continue;
        }
    }

    // On Linux, MemAvailable is a better indicator for reclaimable memory than MemFree.
    metrics.memUsed = (metrics.MemTotal >= metrics.MemAvailable) ? (metrics.MemTotal - metrics.MemAvailable) : 0;
    metrics.memUsedPercent = (metrics.MemTotal > 0) ? ((float)metrics.memUsed / metrics.MemTotal) * 100 : 0;
    
    fclose(fp);
    return metrics;
}

CpuMetrics getCpuMetrics() {
    CpuMetrics metrics = {0, 0, 0, 0, 0, 0, 0.0f};
    FILE *fp = fopen("/proc/stat", "r");
    
    if (fp == NULL) {
        perror("fopen");
        return metrics;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "cpu %llu %llu %llu %llu %llu", &metrics.user, &metrics.nice, &metrics.system, &metrics.idle, &metrics.iowait) == 5) {
            break;
        }
    }

    unsigned long long total = metrics.user + metrics.nice + metrics.system + metrics.idle + metrics.iowait;
    metrics.busy = (total > 0) ? (total - metrics.idle - metrics.iowait) : 0;
    metrics.busyPercent = (total > 0) ? ((float)metrics.busy / (float)total) * 100.0f : 0.0f;
    
    fclose(fp);
    return metrics;
}

static int is_partition_device(const char *name) {
    size_t len = strlen(name);
    if (len == 0) {
        return 0;
    }

    if (strncmp(name, "nvme", 4) == 0 || strncmp(name, "mmcblk", 6) == 0) {
        for (size_t i = 0; i < len; i++) {
            if (name[i] == 'p' && i + 1 < len && isdigit((unsigned char)name[i + 1])) {
                return 1;
            }
        }
        return 0;
    }

    return isdigit((unsigned char)name[len - 1]) ? 1 : 0;
}

static int is_supported_disk_device(const char *name) {
    if (is_partition_device(name)) {
        return 0;
    }
    if (strncmp(name, "sd", 2) == 0 ||
        strncmp(name, "vd", 2) == 0 ||
        strncmp(name, "xvd", 3) == 0 ||
        strncmp(name, "hd", 2) == 0 ||
        strncmp(name, "nvme", 4) == 0 ||
        strncmp(name, "mmcblk", 6) == 0) {
        return 1;
    }
    return 0;
}

static int read_diskstats_totals(unsigned long long *read_sectors, unsigned long long *write_sectors) {
    FILE *fp = fopen("/proc/diskstats", "r");
    if (fp == NULL) {
        return -1;
    }

    unsigned long long read_total = 0;
    unsigned long long write_total = 0;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        int major = 0;
        int minor = 0;
        char name[64] = {0};
        unsigned long long reads_completed = 0;
        unsigned long long reads_merged = 0;
        unsigned long long sectors_read = 0;
        unsigned long long time_reading_ms = 0;
        unsigned long long writes_completed = 0;
        unsigned long long writes_merged = 0;
        unsigned long long sectors_written = 0;
        unsigned long long time_writing_ms = 0;
        unsigned long long io_in_progress = 0;
        unsigned long long time_io_ms = 0;
        unsigned long long weighted_time_io_ms = 0;

        int matched = sscanf(
            line,
            "%d %d %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &major,
            &minor,
            name,
            &reads_completed,
            &reads_merged,
            &sectors_read,
            &time_reading_ms,
            &writes_completed,
            &writes_merged,
            &sectors_written,
            &time_writing_ms,
            &io_in_progress,
            &time_io_ms,
            &weighted_time_io_ms
        );

        if (matched < 10) {
            continue;
        }

        if (!is_supported_disk_device(name)) {
            continue;
        }

        read_total += sectors_read;
        write_total += sectors_written;
    }

    fclose(fp);
    *read_sectors = read_total;
    *write_sectors = write_total;
    return 0;
}

DiskMetrics getDiskMetrics() {
    DiskMetrics metrics = {0, 0, 0.0f, 0.0f, 0.0f};

    unsigned long long read_before = 0;
    unsigned long long write_before = 0;
    unsigned long long read_after = 0;
    unsigned long long write_after = 0;

    if (read_diskstats_totals(&read_before, &write_before) < 0) {
        perror("fopen");
        return metrics;
    }

    usleep(200000); // 200ms sample window for quick throughput glance

    if (read_diskstats_totals(&read_after, &write_after) < 0) {
        perror("fopen");
        return metrics;
    }

    unsigned long long read_delta = (read_after >= read_before) ? (read_after - read_before) : 0;
    unsigned long long write_delta = (write_after >= write_before) ? (write_after - write_before) : 0;

    metrics.readSectors = read_delta;
    metrics.writeSectors = write_delta;

    // 1 sector = 512 bytes, sample window = 0.2s => *5 to get per-second rate
    float read_mb = ((float)read_delta * 512.0f) / (1024.0f * 1024.0f);
    float write_mb = ((float)write_delta * 512.0f) / (1024.0f * 1024.0f);

    metrics.readMBps = read_mb * 5.0f;
    metrics.writeMBps = write_mb * 5.0f;
    metrics.totalMBps = metrics.readMBps + metrics.writeMBps;

    return metrics;
}
