#ifndef HEAD_H
#define HEAD_H

#include<stdbool.h>

#define HEAD_CAPACITY 1024

typedef struct {
        long *timestamps;
        double *values;

        int size;
        long lastTimestamp;
} HeadBlock;

HeadBlock *getNewHeadBlock();

bool PUT_value(HeadBlock *head, long timestamp, double value);

#endif