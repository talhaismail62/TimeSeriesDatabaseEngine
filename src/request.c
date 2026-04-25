#include "parsor.h"
#include <string.h>
#include <stdlib.h>

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

Request* getRequest(const char* buffer)
{
        parser *p = parseString(buffer);
        Request *request = (Request *)malloc(sizeof(Request));

        if(!request) {
                free_parser(p);
                return NULL;
        }

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
        else if(strcmp(p->subStrings[0], "QUIT") == 0) {
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