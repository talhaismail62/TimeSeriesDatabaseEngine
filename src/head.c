#include "head.h"
#include <stdlib.h>
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