#include <stdio.h>
#include <stdlib.h>
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
