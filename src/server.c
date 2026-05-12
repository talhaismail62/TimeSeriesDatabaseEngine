#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include<dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include "server.h"
#include "request.h"
#include <errno.h>
#include "chunk.h"

#define BUFFER_SIZE 1024
volatile bool server_running = true;
int serverSocket;
pthread_mutex_t registry_lock02 = PTHREAD_MUTEX_INITIALIZER;


void handleArguements(int argc, char *argv[], int *portNumber, char *dataFilePath)
{

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

void* handleClient(void* arg) {

        ThreadArgs *args = (ThreadArgs*)arg;
        int clientSocket = args->clientSocket;
        char dataDir[256];
        strcpy(dataDir, args->path);

        free(args);
        
        Request *request = NULL;
        char buffer[BUFFER_SIZE];

        while (1) {
                int bytes = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
                // printf("%d", bytes);

                if (bytes <= 0) {
                        printf("Client disconnected\n");
                        break;
                }

                buffer[bytes] = '\0';
                request = getRequest(buffer);
                if (request == NULL) {
                        send(clientSocket, "Invalid Command!", 17, 0);
                        continue;
                }
                
                printf("Client says: %s\n", buffer);

                Response response = ProcessRequest(request, dataDir);

                if (response.runFurther == false)
                {
                        send(clientSocket,"quit" , 4, 0);
                        break;
                }
                if(response.result == NULL) {
                        if (send(clientSocket, "ok", 2, 0) < 0) {
                                perror("send failed");
                                break;
                        }
                }
                else {
                        if(request->bucketSeconds <= 0) {
                                if (send(clientSocket, "No data Found!", 15, 0) < 0) {
                                        perror("send failed");
                                        break;
                                }
                        }
                        else if (send(clientSocket, response.result, strlen(response.result), 0) < 0)
                        {
                                perror("send failed");
                                break;
                        }
                        printf("%s\n", response.result);
                        free(response.result);
                }

                free(request);
                //  PUT cpu.usage 1728000000 45.2 
                // GET cpu.usage 1728000000 1728000005
                // send(clientSocket, buffer, bytes, 0);

                // PUT cpu.usage 1728000000 45.2
                // ok
                // > PUT cpu.usage 1728000001 34.5
                // ok
                // > PUT cpu.usage 1728000002 34.3
                // ok
                // > PUT cpu.usage 1728000005 45.0
                // ok
                // > PUT cpu.usage 1728000007 31.0
                // ok
                // > GET cpu.usage 1728000000 1728000005
                // > STATS cpu.usage


                // PUT cpu.usage 1728000008 31.0
                // PUT cpu.usage 1728000009 32.0
                // PUT cpu.usage 1728000010 33.0
                // PUT cpu.usage 1728000011 34.0
                // PUT cpu.usage 1728000012 36.0
        }
        close(clientSocket);
        return NULL;
}

bool createPthreadForUsers(int clientSocket, char* dataFilePath)
{
        pthread_t tid;
        ThreadArgs *args = malloc(sizeof(ThreadArgs));

        if (!args) {
                perror("Failed to allocate thread args");
                close(clientSocket);
                return false;
        }
        args->clientSocket = clientSocket;
        strncpy(args->path, dataFilePath, 256 - 1);
        args->path[256 - 1] = '\0';

        
        if (pthread_create(&tid, NULL, handleClient, (void*)args) != 0) {
                perror("Thread creation failed");
                free(args);
                close(clientSocket);
                return false;
        }

        pthread_detach(tid);
        return true;
}

void createAndRunServer(const int portNumber, char *dataFilePath) {
        int clientSocket;
        struct sockaddr_in serverAddress, clientAddress;

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(portNumber);
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        // bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
        // listen(serverSocket, 10);
        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
                perror("bind failed");
                exit(1);
        }

        if (listen(serverSocket, 128) < 0) {
                perror("listen failed");
                exit(1);
        }

        printf("Server listening on port %d...\n", portNumber);

        signal(SIGINT, handle_shutdown);
        signal(SIGTERM, handle_shutdown);
        socklen_t addressSize;

        loadRegistry(dataFilePath);
        while (server_running)
        {
                addressSize = sizeof(clientAddress);
                clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &addressSize);
                // printf("cs : %d", clientSocket);
                if (clientSocket < 0) {
                        if (!server_running) 
                                break;
                        if (errno == EINTR || errno == EBADF)
                                break;
                        perror("Accept failed");
                        continue;
                }
                printf("New client connected!\n");

                createPthreadForUsers(clientSocket, dataFilePath);
                
        }
        
        printf("Server shutting down...\n");
        cleanupRegistry(dataFilePath);
        
}

void handle_shutdown(int sig)
{
    server_running = false;
    close(serverSocket);
}

void loadRegistry(char* dataFilePath) {
        DIR *dir = opendir(dataFilePath);
        if (!dir) {
                perror("opendir");
                return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                char metric_path[512];
                snprintf(metric_path, sizeof(metric_path), "%s/%s", dataFilePath, entry->d_name);

                struct stat st;
                if (stat(metric_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        getMetricFromHashTable(entry->d_name, true);
                        
                        DIR *mDir = opendir(metric_path);
                        if (!mDir) continue;

                        struct dirent *mEntry;
                        while ((mEntry = readdir(mDir)) != NULL) {
                                if (strstr(mEntry->d_name, ".chunk")) {
                                char chunk_full_path[1024];
                                snprintf(chunk_full_path, sizeof(chunk_full_path), "%s/%s", metric_path, mEntry->d_name);
                                
                                loadChunkMetadata(entry->d_name, chunk_full_path);
                                }
                        }
                        closedir(mDir);
                        printf("Metric directory and chunks loaded for: %s\n", entry->d_name);
                }
        }
        closedir(dir);
}


void loadChunkMetadata(char *metricName, char *chunkPath) {

        pthread_mutex_lock(&registry_lock02);
        metric_registry *entry;
        HASH_FIND_STR(registry, metricName, entry);
        pthread_mutex_unlock(&registry_lock02);

        if (!entry) 
                return;

        struct chunkheader header;
        uint8_t *payload = NULL;

        if (chunkread(chunkPath, &header, &payload) == 0) {

                if (entry->chunkCount >= entry->chunkCapacity) {
                        entry->chunkCapacity = (entry->chunkCapacity == 0) ? 4 : entry->chunkCapacity * 2;
                        entry->chunks = realloc(entry->chunks, sizeof(ChunkMetadata) * entry->chunkCapacity);
                }

                entry->chunks[entry->chunkCount].start_ts = (long)header.startts;
                entry->chunks[entry->chunkCount].end_ts = (long)header.endts;
                strncpy(entry->chunks[entry->chunkCount].filename, chunkPath, 255);
                
                entry->chunkCount++;
                
                if (payload) 
                        free(payload);
        }
}