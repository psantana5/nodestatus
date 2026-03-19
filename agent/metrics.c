#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "metrics.h"

#define SAMPLER_INTERVAL_MS 1000
#define MIN_DELTA_WINDOW_MS 200
#define CPU_EMA_ALPHA 0.2f

typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
} CpuCounters;

typedef struct {
    int initialized;
    float value;
} EmaState;

typedef struct {
    int initialized;
    unsigned long long readSectors;
    unsigned long long writeSectors;
} DiskCounters;

static pthread_t sampler_thread;
static int sampler_running = 0;
static pthread_mutex_t sampler_lock = PTHREAD_MUTEX_INITIALIZER;
static SystemMetrics latest_metrics = {0};
static int cpu_prev_initialized = 0;
static CpuCounters cpu_prev = {0, 0, 0, 0, 0};
static DiskCounters disk_prev = {0, 0, 0};
static EmaState cpu_ema = {0, 0.0f};
static uint64_t last_sample_ms = 0;

static uint64_t now_monotonic_ms(void) {
    struct timespec ts = {0, 0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static float clamp_percent(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 100.0f) {
        return 100.0f;
    }
    return value;
}

static float ema_update(EmaState *state, float raw) {
    if (!state->initialized) {
        state->initialized = 1;
        state->value = raw;
        return state->value;
    }
    state->value = (CPU_EMA_ALPHA * raw) + ((1.0f - CPU_EMA_ALPHA) * state->value);
    return state->value;
}

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

    metrics.memUsed = (metrics.MemTotal >= metrics.MemAvailable) ? (metrics.MemTotal - metrics.MemAvailable) : 0;
    metrics.memUsedPercent = (metrics.MemTotal > 0) ? ((float)metrics.memUsed / metrics.MemTotal) * 100.0f : 0.0f;
    fclose(fp);
    return metrics;
}

static int read_cpu_counters(CpuCounters *out) {
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return -1;
    }

    int matched = fscanf(fp, "cpu %llu %llu %llu %llu %llu", &out->user, &out->nice, &out->system, &out->idle, &out->iowait);
    fclose(fp);
    return (matched == 5) ? 0 : -1;
}

CpuMetrics getCpuMetrics() {
    CpuMetrics metrics = {0, 0, 0, 0, 0, 0, 0.0f};
    CpuCounters now = {0, 0, 0, 0, 0};
    if (read_cpu_counters(&now) < 0) {
        perror("fopen");
        return metrics;
    }

    metrics.user = now.user;
    metrics.nice = now.nice;
    metrics.system = now.system;
    metrics.idle = now.idle;
    metrics.iowait = now.iowait;

    if (!cpu_prev_initialized) {
        cpu_prev_initialized = 1;
        cpu_prev = now;
        return metrics;
    }

    if (now.user < cpu_prev.user || now.nice < cpu_prev.nice || now.system < cpu_prev.system ||
        now.idle < cpu_prev.idle || now.iowait < cpu_prev.iowait) {
        cpu_prev = now;
        return metrics;
    }

    unsigned long long total_prev = cpu_prev.user + cpu_prev.nice + cpu_prev.system + cpu_prev.idle + cpu_prev.iowait;
    unsigned long long total_now = now.user + now.nice + now.system + now.idle + now.iowait;
    unsigned long long busy_prev = total_prev - cpu_prev.idle - cpu_prev.iowait;
    unsigned long long busy_now = total_now - now.idle - now.iowait;

    unsigned long long delta_total = total_now - total_prev;
    unsigned long long delta_busy = busy_now - busy_prev;

    metrics.busy = delta_busy;
    if (delta_total > 0) {
        float raw = ((float)delta_busy / (float)delta_total) * 100.0f;
        metrics.busyPercent = clamp_percent(ema_update(&cpu_ema, clamp_percent(raw)));
    }

    cpu_prev = now;
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
    unsigned long long read_now = 0;
    unsigned long long write_now = 0;
    if (read_diskstats_totals(&read_now, &write_now) < 0) {
        perror("fopen");
        return metrics;
    }

    if (!disk_prev.initialized) {
        disk_prev.initialized = 1;
        disk_prev.readSectors = read_now;
        disk_prev.writeSectors = write_now;
        return metrics;
    }

    if (read_now < disk_prev.readSectors || write_now < disk_prev.writeSectors) {
        disk_prev.readSectors = read_now;
        disk_prev.writeSectors = write_now;
        return metrics;
    }

    metrics.readSectors = read_now - disk_prev.readSectors;
    metrics.writeSectors = write_now - disk_prev.writeSectors;
    disk_prev.readSectors = read_now;
    disk_prev.writeSectors = write_now;
    return metrics;
}

static void sample_once(void) {
    SystemMetrics sampled = {0};
    uint64_t now_ms = now_monotonic_ms();

    sampled.load = getLoadAverage();
    sampled.memory = getMemoryMetrics();
    sampled.cpu = getCpuMetrics();
    sampled.disk = getDiskMetrics();

    if (last_sample_ms != 0) {
        uint64_t delta_ms = now_ms - last_sample_ms;
        if (delta_ms >= MIN_DELTA_WINDOW_MS) {
            float seconds = (float)delta_ms / 1000.0f;
            sampled.disk.readMBps = ((float)sampled.disk.readSectors * 512.0f) / (1024.0f * 1024.0f) / seconds;
            sampled.disk.writeMBps = ((float)sampled.disk.writeSectors * 512.0f) / (1024.0f * 1024.0f) / seconds;
            sampled.disk.totalMBps = sampled.disk.readMBps + sampled.disk.writeMBps;
        } else {
            sampled.disk.readMBps = latest_metrics.disk.readMBps;
            sampled.disk.writeMBps = latest_metrics.disk.writeMBps;
            sampled.disk.totalMBps = latest_metrics.disk.totalMBps;
            sampled.cpu.busyPercent = latest_metrics.cpu.busyPercent;
            sampled.cpu.busy = latest_metrics.cpu.busy;
        }
    }

    sampled.sampleTsMs = now_ms;
    sampled.sampleAgeMs = 0;

    pthread_mutex_lock(&sampler_lock);
    latest_metrics = sampled;
    last_sample_ms = now_ms;
    pthread_mutex_unlock(&sampler_lock);
}

static void *sampler_loop(void *arg) {
    (void)arg;
    while (sampler_running) {
        sample_once();
        usleep(SAMPLER_INTERVAL_MS * 1000);
    }
    return NULL;
}

int initMetricsSampler(void) {
    pthread_mutex_lock(&sampler_lock);
    if (sampler_running) {
        pthread_mutex_unlock(&sampler_lock);
        return 0;
    }
    sampler_running = 1;
    pthread_mutex_unlock(&sampler_lock);

    sample_once();
    if (pthread_create(&sampler_thread, NULL, sampler_loop, NULL) != 0) {
        pthread_mutex_lock(&sampler_lock);
        sampler_running = 0;
        pthread_mutex_unlock(&sampler_lock);
        return -1;
    }
    return 0;
}

void stopMetricsSampler(void) {
    pthread_mutex_lock(&sampler_lock);
    int running = sampler_running;
    sampler_running = 0;
    pthread_mutex_unlock(&sampler_lock);

    if (running) {
        (void)pthread_join(sampler_thread, NULL);
    }
}

int getMetricsSnapshot(SystemMetrics *out_metrics) {
    if (out_metrics == NULL) {
        return -1;
    }

    uint64_t now_ms = now_monotonic_ms();
    pthread_mutex_lock(&sampler_lock);
    *out_metrics = latest_metrics;
    if (out_metrics->sampleTsMs > 0 && now_ms >= out_metrics->sampleTsMs) {
        out_metrics->sampleAgeMs = now_ms - out_metrics->sampleTsMs;
    } else {
        out_metrics->sampleAgeMs = 0;
    }
    pthread_mutex_unlock(&sampler_lock);
    return 0;
}
