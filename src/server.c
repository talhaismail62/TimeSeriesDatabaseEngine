#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define BUFFER_SIZE 1024

void handleArguements(int argc, char* argv[], int *portNumber, char *dataFilePath) {

        for (int i = 0; i < argc; ++i) {
                if(strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
                        *portNumber = atoi(argv[i + 1]);
                        ++i;
                }
                else if(strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
                        strcpy(dataFilePath, argv[i + 1]);
                        ++i;
                }
        }
}

int main(int argc, char *argv[])
{
        int portNumber = 8088;
        char dataFilePath[256] = "./data";

        handleArguements(argc, argv, &portNumber, dataFilePath);

        printf("tsdb started on port %d, data directory %s\n", portNumber, dataFilePath);
        

}