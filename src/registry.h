#ifndef REGISTRY_H
#define REGISTRY_H

#include "uthash.h"
#include "head.h"

typedef struct {
        char key[64];
        HeadBlock *head;
        UT_hash_handle hh;
} metric_registry;

extern metric_registry *registry;

HeadBlock *getMetricFromHashTable(char *key, bool flag);

bool Head_PUT(char *metricName, long timestamp, double value);

char *Head_GET(char *metricName, long startTimestamp, long endTimestamp, int *size);

void deleteMetric(char *key);

void print_metric(char *metric);

void cleanupRegistry();

char *Head_STATS(char *metricName);

#endif