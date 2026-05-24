#include "registry.h"
#include <stdlib.h>
#include "uthash.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include "head.h"
#include "flush.h"
#include "timestamp.h"
#include "bit_io.h"
#include "chunk.h"
#include "value.h"


metric_registry *registry = NULL;
pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

HeadBlock* getMetricFromHashTable(char *key, bool flag) {
        metric_registry *tempEntry;

        printf("[%s]\n", key);
        HASH_FIND_STR(registry, key, tempEntry);
        // if(tempEntry == NULL)
        //         printf("getMetricFromHashTable:  NULL\n");
        // else 
        //         printf("getMetricFromHashTable:  NOT NULL\n");

        if(tempEntry == NULL && flag == true) {
                tempEntry = (metric_registry*)malloc(sizeof(metric_registry));
                strncpy(tempEntry->key, key, sizeof(tempEntry->key) - 1);
                tempEntry->key[sizeof(tempEntry->key) - 1] = '\0';

                tempEntry->head = getNewHeadBlock();
                tempEntry->chunkCount = 0;
                tempEntry->chunkCapacity = 4;
                tempEntry->chunks = (ChunkMetadata *)malloc(sizeof(ChunkMetadata) * tempEntry->chunkCapacity);

                HASH_ADD_STR(registry, key, tempEntry);
                printf("New Metric was created as %s\n", tempEntry->key);
        }
        return tempEntry->head;
}

bool Head_PUT(char *metricName, long timestamp, double value, char* dataDir)
{
        // printf("reached Head_STATS, %s\n", metricName);
        pthread_mutex_lock(&registry_lock);
        HeadBlock *head = getMetricFromHashTable(metricName, true);
        pthread_mutex_unlock(&registry_lock);

        pthread_mutex_lock(&head->lock);


        if (!head)
        {
                return false;
        }

        bool res = PUT_value(head, timestamp, value);
    
        if (!res && head->size >= HEAD_CAPACITY) {
                pthread_mutex_unlock(&head->lock); 
                headflush(metricName, dataDir); 
                pthread_mutex_lock(&head->lock);
                res =  PUT_value(head, timestamp, value);
        }
        
        pthread_mutex_unlock(&head->lock);
        return res;
}

char* Head_GET(char *metricName, long startTimestamp, long endTimestamp, int* size) {
        pthread_mutex_lock(&registry_lock);
        metric_registry *entry;
        HASH_FIND_STR(registry, metricName, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry) 
                return NULL;

        *size = 0;
        int capacity = 2048;
        char *result = malloc(capacity);
        result[0] = '\0';
        int current_len = 0;

        for (int i = 0; i < entry->chunkCount; i++) {

                if (entry->chunks[i].start_ts <= endTimestamp && entry->chunks[i].end_ts >= startTimestamp) {

                        int pointsInChunk = 0;
                        char* diskData = decompressChunk(entry->chunks[i].filename, startTimestamp, endTimestamp, &pointsInChunk);
                        if (diskData) {

                                *size += pointsInChunk;
                                int diskLen = strlen(diskData);

                                if (current_len + diskLen + 1 > capacity) {
                                        capacity = current_len + diskLen + 2048;
                                        result = realloc(result, capacity);
                                }
                                strcat(result, diskData);
                                current_len += diskLen;
                                free(diskData);
                        }
                }
        }

        int ramPoints = 0;
        pthread_mutex_lock(&entry->head->lock);
        char *ramData = GET_value(entry->head, startTimestamp, endTimestamp, &ramPoints);
        pthread_mutex_unlock(&entry->head->lock);

        if (ramData) {
                *size += ramPoints;
                int ramLen = strlen(ramData);
                if (current_len + ramLen + 1 > capacity) {
                        capacity = current_len + ramLen + 1;
                        result = realloc(result, capacity);
                }
                strcat(result, ramData);
                free(ramData);
        }

        char summary[64];
        sprintf(summary, "(%d points)\n", *size);
        
        if (current_len + strlen(summary) + 1 > capacity) {
                result = realloc(result, current_len + strlen(summary) + 1);
        }
    
        strcpy(result + current_len, summary);

        return result;
}

char* decompressChunk(const char* filepath, long start, long end, int* chunkPoints) {
        struct chunkheader header;
        uint8_t *payload = NULL;
        *chunkPoints = 0;
        int count = 0;

        if (chunkread(filepath, &header, &payload) != 0) 
                return NULL;

        struct bytebuffer buf = { .data = payload, .size = header.sizebytes };
        struct bitreader *br = brcreate(&buf);
        
        struct timestampdecoder tsdec;
        struct valuedecoder valdec;
        tsdecoderinit(&tsdec, br);
        valdecoderinit(&valdec, br);

        int capacity = 1024;
        char *chunkResult = malloc(capacity);
        chunkResult[0] = '\0';
        int current_len = 0;

        for (uint32_t i = 0; i < header.point_count; i++) {
                long ts = (long)tsdecoderread(&tsdec);
                double val = valdecoderread(&valdec);

                if (ts >= start && ts < end) {
                        count++;
                        char temp[128];
                        int n = snprintf(temp, sizeof(temp), "%ld\t%.2f\n", ts, val);
                        
                        if (current_len + n + 1 > capacity) {
                                capacity *= 2;
                                chunkResult = realloc(chunkResult, capacity);
                        }
                        strcpy(chunkResult + current_len, temp);
                        current_len += n;
                }
        }
        *chunkPoints = count;
        free(payload);
        free(br); 
        return chunkResult;
}

void print_metric(char *metric) {
        metric_registry *entry;

        pthread_mutex_lock(&registry_lock);
        HASH_FIND_STR(registry, metric, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry) {
                printf("Metric not found\n");
                return;
        }

        HeadBlock *hb = entry->head;
        pthread_mutex_lock(&hb->lock);
        printf("Metric: %s\n", metric);
        for (int i = 0; i < hb->size; i++) {
                printf("  %ld -> %.2f\n", hb->timestamps[i], hb->values[i]);
        }
        pthread_mutex_unlock(&hb->lock);
}

void cleanupRegistry(char *dataDir) {
        metric_registry *entry, *tmp;

        HASH_ITER(hh, registry, entry, tmp) {  
                
                if (entry->head && entry->head->size > 0) {
                        printf("Persisting %d points for[] %s]...\n", entry->head->size, entry->key);
                        headflush(entry->key, dataDir); 
                }

                if (entry->head) {

                        if (entry->head->timestamps) free(entry->head->timestamps);
                        if (entry->head->values) free(entry->head->values);
                        
                        pthread_mutex_destroy(&entry->head->lock);
                        free(entry->head);
                }

                if (entry->chunks) {
                        free(entry->chunks);
                }

                HASH_DEL(registry, entry); 

                free(entry);
        }
        registry = NULL;
}

char* Head_STATS(char *metricName) 
{
        pthread_mutex_lock(&registry_lock);
        HeadBlock *head = getMetricFromHashTable(metricName, false);
        pthread_mutex_unlock(&registry_lock);

        pthread_mutex_lock(&head->lock);
        char *p = STATS_value(head, metricName);
        pthread_mutex_unlock(&head->lock);

        return p;        
}

bool headflush(char *metricname, char *dataDir) {

        pthread_mutex_lock(&registry_lock);

        HeadBlock *head =
                getMetricFromHashTable(
                metricname,
                false
                );

        pthread_mutex_unlock(&registry_lock);

        if (!head) {
                return false;
        }

        pthread_mutex_lock(&head->lock);

        if (head->size == 0) {

                pthread_mutex_unlock(&head->lock);

                return true;
        }


        char metricDir[256];
        snprintf(metricDir, sizeof(metricDir), "%s/%s", dataDir, metricname);
        mkdir(metricDir, 0777);


        uint32_t chunkid =
                (uint32_t)head->timestamps[0];

        char filepath[256];

        snprintf(
                filepath,
                sizeof(filepath),
                "%s/%s_%u.chunk",
                metricDir,
                metricname,
                chunkid
        );

        int res =
                flushtochunk(
                chunkid,
                filepath,
                (uint64_t *)head->timestamps,
                head->values,
                head->size
                );

        if (res == 0) {

                metric_registry *entry;
                HASH_FIND_STR(registry, metricname, entry);

                if (entry->chunkCount >= entry->chunkCapacity) {
                        entry->chunkCapacity *= 2;
                        entry->chunks = realloc(entry->chunks, sizeof(ChunkMetadata) * entry->chunkCapacity);
                }

                entry->chunks[entry->chunkCount].start_ts = head->timestamps[0];
                entry->chunks[entry->chunkCount].end_ts = head->lastTimestamp;
                strcpy(entry->chunks[entry->chunkCount].filename, filepath);
                entry->chunkCount++;
                        head->size = 0;
                        // just made the last timestamp zero 
                        // so that the new timestamps are also accepted 
                        // by the engine
                        head->lastTimestamp = 0;
                }

        pthread_mutex_unlock(&head->lock);

        return (res == 0);
}
