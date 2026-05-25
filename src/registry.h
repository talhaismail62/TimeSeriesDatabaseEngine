#ifndef REGISTRY_H
#define REGISTRY_H

#include "uthash.h"
#include "head.h"

typedef struct {
        long start_ts;
        long end_ts;
        char filename[256];
} ChunkMetadata;

typedef struct {
        char key[64];
        HeadBlock *head;
        UT_hash_handle hh;

        ChunkMetadata *chunks;
        int64_t chunkCount;
        int16_t chunkCapacity;

} metric_registry;

extern metric_registry *registry;
extern pthread_mutex_t  registry_lock;

/* Call once at startup with the data directory path.
   Required before Head_AGG can dispatch to coarse chunks. */
void registry_init(const char *dataDir);


HeadBlock *getMetricFromHashTable(char *key, bool flag);

bool Head_PUT(char *metricName, long timestamp, double value, char* dataDir);

char *Head_GET(char *metricName, long startTimestamp, long endTimestamp, int *size);
char* Head_AGG(char *metricName,long startTimestamp,long endTimestamp,int bucketSeconds,const char *func);
void deleteMetric(char *key);

void print_metric(char *metric);

void cleanupRegistry(char *dataDir);

char *Head_STATS(char *metricName);

bool headflush(char* metricname, char* dataDir);

char *decompressChunk(const char *filepath, long start, long end, int* chunkPoints);

#endif
