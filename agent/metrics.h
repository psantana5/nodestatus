#ifndef METRICS_H
#define METRICS_H

typedef struct {
    float load1;
    float load5;
    float load15;
} LoadMetrics;

LoadMetrics getLoadAverage();

#endif
