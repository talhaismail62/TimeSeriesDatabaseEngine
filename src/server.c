#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>
#include <signal.h>
#include "server.h"
#include "request.h"
#include <errno.h>
#include "chunk.h"
#include "wal.h"
#include "retention.h"
#include "downsample.h"

#define BUFFER_SIZE 1024
volatile bool server_running = true;
int serverSocket;
pthread_mutex_t registry_lock02 = PTHREAD_MUTEX_INITIALIZER;

static RetentionConfig g_retention_cfg = {.count = 0};

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
                else if(strcmp(argv[i], "--retention") == 0 && i + 1 < argc) {
                        if (retention_add_rule(&g_retention_cfg, argv[i + 1]) != 0)
                                fprintf(stderr, "warn: bad --retention spec '%s' (want metric=seconds)\n",
                                        argv[i + 1]);
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

        char inbuf[8192];
        int inlen = 0;
        inbuf[0] = '\0';

        while (1) {
                char tmp[BUFFER_SIZE];
                int bytes = recv(clientSocket, tmp, BUFFER_SIZE, 0);

                if (bytes <= 0) {
                        printf("Client disconnected\n");
                        break;
                }

                if (inlen + bytes >= (int)sizeof(inbuf) - 1) {
                        inlen = 0;
                        inbuf[0] = '\0';
                        send(clientSocket, "ERR line too long\n", 18, 0);
                        continue;
                }

                memcpy(inbuf + inlen, tmp, bytes);
                inlen += bytes;
                inbuf[inlen] = '\0';

                char *line_start = inbuf;

                while (1) {
                        char *nl = memchr(line_start, '\n', inbuf + inlen - line_start);
                        if (!nl) break;

                        int linelen = (int)(nl - line_start) + 1;

                        char line[1024];
                        int copylen = linelen < (int)sizeof(line) - 1 ? linelen : (int)sizeof(line) - 1;
                        memcpy(line, line_start, copylen);
                        line[copylen] = '\0';

                        line_start = nl + 1;

                        Request *request = getRequest(line);
                        if (request == NULL) {
                                send(clientSocket, "Invalid Command!\n", 17, 0);
                                continue;
                        }

                        Response response = ProcessRequest(request, dataDir);

                        if (response.runFurther == false)
                        {
                                send(clientSocket,"quit\n" , 5, 0);
                                free(request);
                                if (response.result) free(response.result);
                                close(clientSocket);
                                return NULL;
                        }

                        if(response.result == NULL) {
                                if (send(clientSocket, "ok", 2, 0) < 0) {
                                        perror("send failed");
                                        free(request);
                                        break;
                                }
                        }
                        else {
                                if (send(clientSocket, response.result, strlen(response.result), 0) < 0) {
                                        perror("send failed");
                                        free(response.result);
                                        free(request);
                                        break;
                                }
                                free(response.result);
                        }

                        free(request);
                }

                int remaining = (int)(inbuf + inlen - line_start);
                if (remaining > 0) memmove(inbuf, line_start, remaining);
                inlen = remaining;
                inbuf[inlen] = '\0';
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

        registry_init(dataFilePath);
        loadRegistry(dataFilePath);
        retention_start(&g_retention_cfg, dataFilePath);
        downsample_start(dataFilePath);
        while (server_running)
        {
                addressSize = sizeof(clientAddress);
                clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &addressSize);
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
        retention_stop();
        downsample_stop();
        cleanupRegistry(dataFilePath);
}

void handle_shutdown(int sig)
{
    (void)sig;
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

                        /* Replay any WAL left from a previous run that crashed
                           before the head block was flushed. */
                        HeadBlock *hb = getMetricFromHashTable(entry->d_name, false);
                        if (hb)
                                wal_replay(dataFilePath, entry->d_name, hb);

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