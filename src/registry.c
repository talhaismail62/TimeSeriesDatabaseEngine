#include "registry.h"
#include <stdlib.h>
#include "uthash.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "head.h"

metric_registry *registry = NULL;

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
        HeadBlock *head = getMetricFromHashTable(metricName);
        return PUT_value(head, timestamp, value);
}

void print_metric(char *metric) {
        metric_registry *entry;

        HASH_FIND_STR(registry, metric, entry);

        if (!entry) {
                printf("Metric not found\n");
                return;
        }

        HeadBlock *hb = entry->head;

        printf("Metric: %s\n", metric);
        for (int i = 0; i < hb->size; i++) {
                printf("  %ld -> %.2f\n", hb->timestamps[i], hb->values[i]);
        }
}

void deleteMetric(char *key) {
        metric_registry *entry;

        HASH_FIND_STR(registry, key, entry);

        if (!entry) return;

        free(entry->head->timestamps);
        free(entry->head->values);
        free(entry->head);

        HASH_DEL(registry, entry);

        free(entry);
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