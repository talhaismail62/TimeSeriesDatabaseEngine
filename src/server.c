#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include "server.h"
#include "request.h"

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

void* handleClient(void* arg) {
        int clientSocket = *(int*)arg;

        free(arg);
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

                Response response = ProcessRequest(request);
                if (response.runFurther == false) {
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
        }
        close(clientSocket);
        return NULL;
}

bool createPthreadForUsers(int clientSocket)
{
        pthread_t tid;

        int *pclient = malloc(sizeof(int));
        if (!pclient) {
                perror("malloc failed!");
                close(clientSocket);
                return false;
        }
        *pclient = clientSocket;

        if (pthread_create(&tid, NULL, handleClient, pclient) != 0) {
                perror("Thread creation failed");
                free(pclient);
                close(clientSocket);
                return false;
        }

        pthread_detach(tid);
        return true;
}

void createAndRunServer(const int portNumber) {
        int serverSocket, clientSocket;
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


        socklen_t addressSize;
        while (1)
        {
                addressSize = sizeof(clientAddress);
                clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &addressSize);
                // printf("cs : %d", clientSocket);
                if (clientSocket < 0) {
                        printf("HHHH");
                        perror("Accept failed");
                        continue;
                }
                printf("New client connected!\n");

                createPthreadForUsers(clientSocket);
                
        }
        close(serverSocket);

}


