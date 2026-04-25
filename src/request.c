#include "parsor.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "request.h"
#include "registry.h"


Request* getRequest(const char* buffer)
{
        parser *p = parseString(buffer);
        Request *request = (Request *)malloc(sizeof(Request));

        if(!request) {
                free_parser(p);
                return NULL;
        }
        printf("size : %d, string: %s\n", p->size, p->subStrings[0]);
        printf("RAW: [%s]\n", p->subStrings[0]);

        if(p->size == 4 && strcmp(p->subStrings[0], "PUT") == 0) {
                request->type = PUT;

                strncpy(request->metric, p->subStrings[1], sizeof(request->metric) - 1);
                request->metric[sizeof(request->metric) - 1] = '\0';

                request->timestamp = strtol(p->subStrings[2], NULL, 10);
                request->value = strtod(p->subStrings[3], NULL);
        }
        else if(p->size == 4 && strcmp(p->subStrings[0], "GET") == 0) {
                request->type = GET;

                strncpy(request->metric, p->subStrings[1], sizeof(request->metric) - 1);
                request->metric[sizeof(request->metric) - 1] = '\0';

                request->startTimeStamp = strtol(p->subStrings[2], NULL, 10);
                request->endTimeStamp = strtol(p->subStrings[3], NULL, 10);
        }
        else if(p->size == 2 && strcmp(p->subStrings[0], "FLUSH") == 0) {
                request->type = FLUSH;

                strncpy(request->metric, p->subStrings[1], sizeof(request->metric) - 1);
                request->metric[sizeof(request->metric) - 1] = '\0';
        }
        else if(p->size == 2 && strcmp(p->subStrings[0], "STATS") == 0) {
                request->type = STATS;

                strncpy(request->metric, p->subStrings[1], sizeof(request->metric) - 1);
                request->metric[sizeof(request->metric) - 1] = '\0';
        }
        else if(p->size == 1 && strcmp(p->subStrings[0], "QUIT") == 0) {
                request->type = QUIT;
        }
        else if(p->size == 6 && strcmp(p->subStrings[0], "AGG") == 0) {
                request->type = AGG;

                strncpy(request->metric, p->subStrings[1], sizeof(request->metric) - 1);
                request->metric[sizeof(request->metric) - 1] = '\0';

                request->startTimeStamp = strtol(p->subStrings[2], NULL, 10);
                request->endTimeStamp = strtol(p->subStrings[3], NULL, 10);
                request->bucketSeconds = atoi(p->subStrings[4]);
                strcpy(request->func, p->subStrings[5]);
        }
        else {
                free(request);
                free_parser(p);
                return NULL;
        }
        free_parser(p);
        return request;
}

bool ProcessRequest(Request *request)
{
        bool flag = true;
        switch (request->type)
        {
        case PUT:
                handlePUT(request);
                break;
        case QUIT:
                handleQuit();
                flag = false;
        default:
                break;
        }
        print_metric(request->metric);
        return flag;
}

bool handlePUT(Request *request)
{
        if(request == NULL)
                return false;
        return Head_PUT(request->metric, request->timestamp, request->value);
}

void handleQuit()
{
        cleanupRegistry();
}