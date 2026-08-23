#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "conn.h"
#include "log.h"

static volatile SOCKET g_listener = INVALID_SOCKET;

static BOOL WINAPI ctrl_handler(DWORD type)
{
    (void)type;
    if (g_listener != INVALID_SOCKET) {
        log_info("sinal recebido, encerrando listener");
        closesocket(g_listener);
    }
    return TRUE;
}

typedef struct {
    SOCKET sock;
} client_ctx_t;

static DWORD WINAPI client_thread(LPVOID param)
{
    client_ctx_t *ctx = (client_ctx_t *)param;
    handle_connection(ctx->sock);
    closesocket(ctx->sock);
    free(ctx);
    return 0;
}

int server_run(int port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        log_error("WSAStartup falhou");
        return 1;
    }

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        log_error("socket() falhou: %d", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    g_listener = listener;

    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)port);

    if (bind(listener, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log_error("bind() na porta %d falhou: %d", port, WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        log_error("listen() falhou: %d", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    log_info("servidor ouvindo em http://localhost:%d", port);

    for (;;) {
        SOCKET conn = accept(listener, NULL, NULL);
        if (conn == INVALID_SOCKET) {
            log_info("accept() encerrou o loop");
            break;
        }

        client_ctx_t *ctx = (client_ctx_t *)malloc(sizeof(*ctx));
        if (!ctx) {
            closesocket(conn);
            continue;
        }
        ctx->sock = conn;

        HANDLE t = CreateThread(NULL, 0, client_thread, ctx, 0, NULL);
        if (t) {
            CloseHandle(t);
        } else {
            log_error("CreateThread falhou: %lu", GetLastError());
            closesocket(conn);
            free(ctx);
        }
    }

    closesocket(listener);
    g_listener = INVALID_SOCKET;
    WSACleanup();
    return 0;
}
