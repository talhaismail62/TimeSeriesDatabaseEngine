#ifndef REQUEST_H
#define REQUEST_H


#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "registry.h"

typedef enum {
        PUT, GET, FLUSH, QUIT, STATS, AGG
} CommandType;

typedef struct {
        CommandType type;
        char metric[64];

        long timestamp;
        double value;

        long startTimeStamp;
        long endTimeStamp;

        int bucketSeconds;
        char func[16];
} Request;

Request *getRequest(const char *buffer);

bool ProcessRequest(Request *request);

bool handlePUT(Request *request);

void handleQuit();

#endif

