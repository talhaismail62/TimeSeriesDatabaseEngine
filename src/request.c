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

Response ProcessRequest(Request *request)
{
        Response response;
        response.runFurther = true;
        response.result = NULL;
        
        switch (request->type)
        {
        case PUT:
                handlePUT(request);
                print_metric(request->metric);
                break;
        case QUIT:
                // handleQuit();
                response.runFurther = false;
                break;
        case GET:
                response.result = handleGET(request);
                break;
        case STATS:
                response.result = handleSTATS(request);
                break;
        default:
                break;
        }
        // print_metric(request->metric);
        return response;
}

bool handlePUT(Request *request)
{
        return Head_PUT(request->metric, request->timestamp, request->value);
}

char* handleGET(Request *request) { // i am using the bucketseconds as the size here, idk why :(
        return Head_GET(request->metric, request->startTimeStamp, request->endTimeStamp, &request->bucketSeconds);
}

char* handleSTATS(Request *request)
{
        return Head_STATS(request->metric);
}
void handleQuit()
{
        // cleanupRegistry();
}