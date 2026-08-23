#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include "conn.h"
#include "log.h"
#include "request.h"

#define RECV_BUF_SIZE 16384
#define KEEPALIVE_MAX_REQUESTS 100

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

static int send_simple(SOCKET sock, const char *status_line)
{
    const char *resp = "Content-Length: 0\r\nConnection: close\r\n\r\n";
    return send_all(sock, status_line, (int)strlen(status_line)) &&
           send_all(sock, resp, (int)strlen(resp));
}

void handle_connection(SOCKET sock)
{
    char buf[RECV_BUF_SIZE];
    size_t have = 0;
    int served = 0;

    for (;;) {
        http_request_t req;
        size_t consumed = 0;

        req_status_t st = request_parse(buf, have, &req, &consumed);
        while (st == REQ_NEED_MORE) {
            int n = recv(sock, buf + have, (int)(RECV_BUF_SIZE - have), 0);
            if (n <= 0)
                return;
            have += (size_t)n;
            st = request_parse(buf, have, &req, &consumed);
        }

        if (st == REQ_TOO_LARGE) {
            send_simple(sock, "HTTP/1.1 431 Request Header Fields Too Large\r\n");
            return;
        }
        if (st != REQ_OK) {
            send_simple(sock, "HTTP/1.1 400 Bad Request\r\n");
            return;
        }

        log_info("%s %s %s", req.method, req.target, req.version);

        const char *body = "hello from c-http-server\n";
        char hdr[512];
        int hl = snprintf(hdr, sizeof(hdr),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain; charset=utf-8\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: %s\r\n"
                          "\r\n",
                          strlen(body),
                          req.keep_alive ? "keep-alive" : "close");
        if (!send_all(sock, hdr, hl))
            return;
        if (!req.head_only && !send_all(sock, body, (int)strlen(body)))
            return;

        served++;
        if (!req.keep_alive || served >= KEEPALIVE_MAX_REQUESTS)
            return;

        size_t leftover = have - consumed;
        memmove(buf, buf + consumed, leftover);
        have = leftover;
    }
}
