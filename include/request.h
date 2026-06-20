#ifndef REQUEST_H
#define REQUEST_H

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "include/registry.h"

typedef enum
{
        PUT,
        GET,
        FLUSH,
        QUIT,
        STATS,
        AGG
} CommandType;

typedef struct
{
        CommandType type;
        char metric[64];

        long timestamp;
        double value;

        long startTimeStamp;
        long endTimeStamp;
        int resultCount;
        int bucketSeconds;
        char func[16];
} Request;

typedef struct
{
        bool runFurther;
        char *result;
} Response;

Request *
getRequest(const char *buffer);

Response ProcessRequest(Request *request, char *dataDir);

bool handlePUT(Request *request, char *dataDir);

char *handleGET(Request *request);
char *handleAGG(Request *request);
void handleQuit();

char *handleSTATS(Request *request);

bool handleflush(Request *request, char *dataDir);

#endif
