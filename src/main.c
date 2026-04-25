#include <stdio.h>
#include "server.h"
#include "registry.h"

int main(int argc, char *argv[])
{
        int portNumber = 8088;
        char dataFilePath[256] = "./data";

        handleArguements(argc, argv, &portNumber, dataFilePath);

        printf("tsdb started on port %d, data directory %s\n", portNumber, dataFilePath);

        createAndRunServer(portNumber);
        return 0;
}