#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
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
    // Also- This gives us more "logical memory used" instead of "physical memory used" which is more useful for monitoring purposes.
    metrics.memUsed = (metrics.MemTotal >= metrics.MemAvailable) ? (metrics.MemTotal - metrics.MemAvailable) : 0;
    metrics.memUsedPercent = (metrics.MemTotal > 0) ? ((float)metrics.memUsed / metrics.MemTotal) * 100 : 0;
    
    fclose(fp);
    return metrics;
}

static int read_cpu_counters(
    unsigned long long *user,
    unsigned long long *nice,
    unsigned long long *system,
    unsigned long long *idle,
    unsigned long long *iowait
) {
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return -1;
    }

    int matched = fscanf(fp, "cpu %llu %llu %llu %llu %llu", user, nice, system, idle, iowait);
    fclose(fp);
    return (matched == 5) ? 0 : -1;
}

CpuMetrics getCpuMetrics() {
    CpuMetrics metrics = {0, 0, 0, 0, 0, 0, 0.0f};

    unsigned long long user0 = 0, nice0 = 0, system0 = 0, idle0 = 0, iowait0 = 0;
    unsigned long long user1 = 0, nice1 = 0, system1 = 0, idle1 = 0, iowait1 = 0;

    if (read_cpu_counters(&user0, &nice0, &system0, &idle0, &iowait0) < 0) {
        perror("fopen");
        return metrics;
    }

    usleep(100000); // 100ms sample window for CPU delta

    if (read_cpu_counters(&user1, &nice1, &system1, &idle1, &iowait1) < 0) {
        perror("fopen");
        return metrics;
    }

    metrics.user = user1;
    metrics.nice = nice1;
    metrics.system = system1;
    metrics.idle = idle1;
    metrics.iowait = iowait1;

    unsigned long long total0 = user0 + nice0 + system0 + idle0 + iowait0;
    unsigned long long total1 = user1 + nice1 + system1 + idle1 + iowait1;
    unsigned long long busy0 = total0 - idle0 - iowait0;
    unsigned long long busy1 = total1 - idle1 - iowait1;

    unsigned long long delta_total = (total1 >= total0) ? (total1 - total0) : 0;
    unsigned long long delta_busy = (busy1 >= busy0) ? (busy1 - busy0) : 0;

    metrics.busy = delta_busy;
    metrics.busyPercent = (delta_total > 0) ? ((float)delta_busy / (float)delta_total) * 100.0f : 0.0f;

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

// Reads /proc/diskstats and sums the total sectors read and written for supported disk devices (excluding partitions).
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
    DiskMetrics metrics = {0, 0, 0.0f, 0.0f, 0.0f}; // initializing with zeros to clean garbage data.

    unsigned long long read_before = 0;
    unsigned long long write_before = 0;
    unsigned long long read_after = 0;
    unsigned long long write_after = 0;

    struct timespec ts_before = {0, 0};
    struct timespec ts_after = {0, 0};

    if (read_diskstats_totals(&read_before, &write_before) < 0) {
        perror("fopen");
        return metrics;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &ts_before);

    usleep(200000); // 200ms sample window for quick throughput glance

    if (read_diskstats_totals(&read_after, &write_after) < 0) {
        perror("fopen");
        return metrics;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &ts_after); // CLOCK_MONOTONIC is unaffected by system time changes, ideal for measuring elapsed time

    unsigned long long read_delta = (read_after >= read_before) ? (read_after - read_before) : 0; // handle potential counter reset or overflow by treating negative deltas as zero
    unsigned long long write_delta = (write_after >= write_before) ? (write_after - write_before) : 0; // same but write_delta instead

    metrics.readSectors = read_delta;
    metrics.writeSectors = write_delta;

    double elapsed_seconds = (double)(ts_after.tv_sec - ts_before.tv_sec) // convert seconds to double
        + (double)(ts_after.tv_nsec - ts_before.tv_nsec) / 1000000000.0; // convert nanoseconds to seconds and add to total elapsed time
    if (elapsed_seconds <= 0.0) {
        return metrics;
    }

    // 1 sector = 512 bytes. This will not always be the case. For more accuracy, we read from /sys/block/<device>/queue/logical_block_size.
    float read_mb = ((float)read_delta * 512.0f) / (1024.0f * 1024.0f); // convert bytes to megabytes 
    float write_mb = ((float)write_delta * 512.0f) / (1024.0f * 1024.0f); // convert bytes to megabytes

    metrics.readMBps = read_mb / (float)elapsed_seconds;
    metrics.writeMBps = write_mb / (float)elapsed_seconds;
    metrics.totalMBps = metrics.readMBps + metrics.writeMBps;

    return metrics;
}
