#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define PORT 5555
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    int res = connect(sock, (struct sockaddr*)&server, sizeof(server));
    assert(res == 0);

    send(sock, "PUT cpu.usage 100 42.5", 25, 0);
    int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
    buffer[bytes] = '\0';

    printf("Response: %s\n", buffer);
    assert(strcmp(buffer, "ok") == 0);

    send(sock, "GET cpu.usage 100 100", 26, 0);
    bytes = recv(sock, buffer, BUFFER_SIZE, 0);
    buffer[bytes] = '\0';

    printf("Response: %s\n", buffer);
    assert(strstr(buffer, "42.5") != NULL);

    close(sock);
    printf("Test passed!\n");

    return 0;
}