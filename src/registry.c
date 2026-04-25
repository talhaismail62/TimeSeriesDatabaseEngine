#include "registry.h"
#include <stdlib.h>
#include "uthash.h"
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include "head.h"

metric_registry *registry = NULL;
pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

HeadBlock* getMetricFromHashTable(char *key) {
        metric_registry *tempEntry;

        
        HASH_FIND_STR(registry, key, tempEntry);

        if(tempEntry == NULL) {
                tempEntry = (metric_registry*)malloc(sizeof(metric_registry));
                strncpy(tempEntry->key, key, sizeof(tempEntry->key) - 1);
                tempEntry->key[sizeof(tempEntry->key) - 1] = '\0';

                tempEntry->head = getNewHeadBlock();

                HASH_ADD_STR(registry, key, tempEntry);
                printf("New Metric was created as %s\n", tempEntry->key);
        }
        return tempEntry->head;
}

bool Head_PUT(char *metricName, long timestamp, double value)
{
        pthread_mutex_lock(&registry_lock);
        HeadBlock *head = getMetricFromHashTable(metricName);
        pthread_mutex_unlock(&registry_lock);

        if(!head) {
                return false;
        }
        pthread_mutex_lock(&head->lock);
        bool res =  PUT_value(head, timestamp, value);
        pthread_mutex_unlock(&head->lock);
        return res;
}

char* Head_GET(char *metricName, long startTimestamp, long endTimestamp, int* size) {
        pthread_mutex_lock(&registry_lock);
        HeadBlock *head = getMetricFromHashTable(metricName);
        pthread_mutex_unlock(&registry_lock);

        pthread_mutex_lock(&head->lock);
        char* p =  GET_value(head, startTimestamp, endTimestamp, size);
        pthread_mutex_unlock(&head->lock);

        return p;
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

void cleanupRegistry() {
        metric_registry *entry, *tmp;

        HASH_ITER(hh, registry, entry, tmp) {  
               
                free(entry->head->timestamps);
                free(entry->head->values);
                free(entry->head);

                HASH_DEL(registry, entry); 

                free(entry);
        }
        registry = NULL;
}