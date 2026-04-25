#include "head.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


HeadBlock* getNewHeadBlock() {
        HeadBlock *head = (HeadBlock *)malloc(sizeof(HeadBlock));

        if(!head) {
                return NULL;
        }
        head->size = 0;
        head->timestamps = (long *)malloc(sizeof(long) * HEAD_CAPACITY);
        head->values = (double *)malloc(sizeof(double) * HEAD_CAPACITY);
        return head;
}

bool PUT_value(HeadBlock* head, long timestamp, double value)
{
        if(!head) {
                return false;
        }
        if(head->size >= HEAD_CAPACITY) {
                // this portion will be for the compression 
                // and flushing the head block later on 
                return false;
        }

        head->timestamps[head->size] = timestamp;
        head->lastTimestamp = timestamp;
        head->values[head->size] = value;
        ++head->size;
        return true;
}

char* GET_value(HeadBlock *head, long startTimestamp, long endTimestamp, int *size)
{
        if (head == NULL) 
                return NULL;

        *size = 0;

        int capacity = 128;
        char *range = malloc(capacity);
        if (!range) 
                return NULL;

        range[0] = '\0';
        int offset = 0;

        for (int i = 0; i < head->size; i++) {

                if (head->timestamps[i] >= startTimestamp && head->timestamps[i] <= endTimestamp) {

                        char *tmp = longToString(head->timestamps[i]);
                        char *tmp1 = doubleToString(head->values[i]);
                        if (!tmp || !tmp1)
                        {
                                free(range);
                                return NULL;
                        }
                        tmp = strcat(tmp, "\t");
                        tmp = strcat(tmp, tmp1);

                        int needed = strlen(tmp) + 2;

                        if (offset + needed >= capacity) {
                                capacity *= 2;
                                char *new_range = realloc(range, capacity);
                                if (!new_range) {
                                        free(tmp);
                                        free(range);
                                        return NULL;
                                }
                                range = new_range;
                        }

                        offset += sprintf(range + offset, "%s\n\0", tmp);
                        free(tmp);

                        (*size)++;
                }
        }
        range = strcat(range, "(");
        char *tmp = intToString(*size);
        if (!tmp) {
                free(range);
                return NULL;
        }
        range = strcat(range, tmp);
        range = strcat(range, " points)\n");
        return range;
}

char* longToString(long x)
{
        char *buf = malloc(32);
        if (!buf) 
                return NULL;

        snprintf(buf, 32, "%ld", x);
        return buf;
}

char* doubleToString(double x)
{
        char *buf = malloc(sizeof(double));
        if (!buf) 
                return NULL;

        snprintf(buf, sizeof(double), "%.2f", x);
        return buf;
}

char* intToString(int x)
{
        char *buf = malloc(sizeof(double));
        if (!buf) 
                return NULL;

        snprintf(buf, sizeof(double), "%d", x);
        return buf;
}