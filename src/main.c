#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main(int argc, char **argv)
{
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port < 1 || port > 65535)
            port = 8080;
    }
    return server_run(port);
}
