#ifndef SERVER_H
#define SERVER_H


#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>

#define BUFFER_SIZE 1024

void handleArguements(int argc, char *argv[], int *portNumber, char *dataFilePath);

void *handleClient(void *arg);

bool createPthreadForUsers(int clientSocket);

void createAndRunServer(const int portNumber);

void handle_shutdown(int sig);

#endif