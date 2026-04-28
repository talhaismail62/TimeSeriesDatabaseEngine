`#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

bool establishConnection(int* sock, int* portNumber, char message[], char buffer[],const char *ipAddress) {
        struct sockaddr_in server_addr;
        *sock = socket(AF_INET, SOCK_STREAM, 0);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(*portNumber);

        inet_pton(AF_INET, ipAddress, &server_addr.sin_addr);

        if (connect(*sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                perror("Connection failed");
                return false;
        }

        printf("connected to tsdb at %s :%d\n", ipAddress, *portNumber);
        return true;
}

void handleArguments(int argc, char * argv[], int *portNumber, char* ipaddress)
{
        if (argc >= 3) {
                *portNumber = atoi(argv[2]);
                strcpy(ipaddress, argv[1]);
        }
}

int main(int argc, char* argv[]) {
        int sock;
        int portNumber = 8088;
        char ipAddress[256] = "127.0.0.1";
        char *buffer = NULL;
        char message[BUFFER_SIZE];

        handleArguments(argc, argv, &portNumber, ipAddress);
        
        struct sockaddr_in server_addr;
        
        sock = socket(AF_INET, SOCK_STREAM, 0);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(portNumber);

        inet_pton(AF_INET, ipAddress, &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                perror("Connection failed");
                return 1;
        }

        printf("Connected to server!\n");

        while (1) {
                printf("> ");
                buffer = (char *)malloc(BUFFER_SIZE);
                fgets(message, BUFFER_SIZE, stdin);

                send(sock, message, strlen(message), 0);
                int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);

                if (bytes <= 0) {
                        printf("Server disconnected\n");
                        break;
                }
                // printf("[%s]\n", buffer);
                if (strncmp(buffer, "quit", 4) == 0)
                {
                        printf("bye:)\n");
                        break;
                }
                buffer[bytes] = '\0';
                printf("%s\n", buffer);
                free(buffer);
        }

        close(sock);
        return 0;
}

// $ ./ tsdb - cli localhost 5555
// connected to tsdb at localhost :5555