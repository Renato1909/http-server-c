#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include "conn.h"
#include "log.h"
#include "request.h"
#include "response.h"
#include "router.h"

#define RECV_BUF_SIZE 16384
#define KEEPALIVE_MAX_REQUESTS 100
#define IO_TIMEOUT_MS 15000

void handle_connection(SOCKET sock)
{
    DWORD timeout_ms = IO_TIMEOUT_MS;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout_ms, sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout_ms, sizeof(timeout_ms));

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
            response_send(sock, 431, "text/plain; charset=utf-8", NULL, 0,
                          0, 0, NULL);
            return;
        }
        if (st != REQ_OK) {
            response_send(sock, 400, "text/plain; charset=utf-8", NULL, 0,
                          0, 0, NULL);
            return;
        }

        int keep_alive = req.keep_alive;
        ULONGLONG t0 = GetTickCount64();
        int status = router_dispatch(sock, &req, &keep_alive);
        ULONGLONG elapsed = GetTickCount64() - t0;

        char ip[48] = "-";
        SOCKADDR_IN peer;
        int peer_len = sizeof(peer);
        if (getpeername(sock, (SOCKADDR *)&peer, &peer_len) == 0) {
            DWORD iplen = sizeof(ip);
            if (WSAAddressToStringA((SOCKADDR *)&peer, sizeof(peer), NULL,
                                    ip, &iplen) != 0)
                strcpy(ip, "-");
        }
        log_info("%s %s -> %d [%s] %lu ms", req.method, req.target, status,
                 ip, (unsigned long)elapsed);

        served++;
        if (!keep_alive || served >= KEEPALIVE_MAX_REQUESTS)
            return;

        size_t leftover = have - consumed;
        memmove(buf, buf + consumed, leftover);
        have = leftover;
    }
}
