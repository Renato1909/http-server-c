#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include "conn.h"
#include "log.h"

#define RECV_BUF_SIZE 16384

static int send_all(SOCKET sock, const char *data, int len)
{
    int sent = 0;
    while (sent < len) {
        int n = send(sock, data + sent, len - sent, 0);
        if (n <= 0)
            return 0;
        sent += n;
    }
    return 1;
}

void handle_connection(SOCKET sock)
{
    char buf[RECV_BUF_SIZE];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
        return;
    buf[n] = '\0';

    char method[8], target[2048], version[16];
    if (sscanf(buf, "%7s %2047s %15s", method, target, version) != 3) {
        const char *resp =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
        send_all(sock, resp, (int)strlen(resp));
        return;
    }

    log_info("%s %s %s", method, target, version);

    const char *body = "hello from c-http-server\n";
    char resp[512];
    int len = snprintf(resp, sizeof(resp),
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain; charset=utf-8\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       strlen(body), body);
    send_all(sock, resp, len);
}
