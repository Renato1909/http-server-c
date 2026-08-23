#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "response.h"
#include "mime.h"
#include "log.h"

#define MAX_FILE_BYTES (50ULL * 1024 * 1024)

static char g_root[MAX_PATH];
static size_t g_root_len;

void router_init(void)
{
    DWORD n = GetFullPathNameA("public", MAX_PATH, g_root, NULL);
    if (n == 0 || n >= MAX_PATH) {
        log_error("nao foi possivel resolver o diretorio public/");
        g_root_len = (size_t)snprintf(g_root, sizeof(g_root), "public");
        return;
    }
    g_root_len = strlen(g_root);
    while (g_root_len > 3 &&
           (g_root[g_root_len - 1] == '\\' || g_root[g_root_len - 1] == '/'))
        g_root[--g_root_len] = '\0';
    log_info("raiz de arquivos estaticos: %s", g_root);
}

static int has_parent_segment(const char *path)
{
    const char *p = path;
    while (*p) {
        while (*p == '/')
            p++;
        const char *seg = p;
        while (*p && *p != '/')
            p++;
        size_t n = (size_t)(p - seg);
        if (n == 2 && seg[0] == '.' && seg[1] == '.')
            return 1;
    }
    return 0;
}

static int send_error(SOCKET sock, int status, int head_only, int keep_alive,
                      const char *extra_headers)
{
    char body[160];
    int bl = snprintf(body, sizeof(body),
                      "<html><head><title>%d %s</title></head>"
                      "<body><h1>%d %s</h1></body></html>",
                      status, status_reason(status),
                      status, status_reason(status));
    return response_send(sock, status, "text/html; charset=utf-8",
                         body, bl > 0 ? (size_t)bl : 0,
                         head_only, keep_alive, extra_headers);
}

static int serve_file(SOCKET sock, const char *fs_path,
                      int head_only, int keep_alive)
{
    struct _stat64 st;
    if (_stat64(fs_path, &st) != 0)
        return 0;

    if (st.st_mode & S_IFDIR)
        return -1;

    if (st.st_size < 0 || (unsigned long long)st.st_size > MAX_FILE_BYTES)
        return -2;

    FILE *f = fopen(fs_path, "rb");
    if (!f)
        return -2;

    char *data = (char *)malloc((size_t)st.st_size);
    if (!data) {
        fclose(f);
        return -2;
    }

    size_t got = fread(data, 1, (size_t)st.st_size, f);
    fclose(f);

    int rc = response_send(sock, 200, mime_from_path(fs_path),
                           data, got, head_only, keep_alive, NULL);
    free(data);
    return rc ? 1 : -2;
}

static void build_fs_path(char *dst, size_t cap, const char *rel_target)
{
    size_t used = 0;
    used += (size_t)snprintf(dst + used, cap - used, "%.*s",
                             (int)g_root_len, g_root);
    for (const char *p = rel_target; *p && used + 1 < cap; p++)
        dst[used++] = (*p == '/') ? '\\' : *p;
    dst[used] = '\0';
}

int router_dispatch(SOCKET sock, http_request_t *req, int *keep_alive)
{
    int is_get = strcmp(req->method, "GET") == 0;
    int is_head = req->head_only;

    if (strcmp(req->version, "HTTP/1.0") != 0 &&
        strcmp(req->version, "HTTP/1.1") != 0) {
        *keep_alive = 0;
        send_error(sock, 505, 0, *keep_alive, NULL);
        return 505;
    }

    if (!is_get && !is_head) {
        *keep_alive = 0;
        send_error(sock, 405, 0, *keep_alive, "Allow: GET, HEAD\r\n");
        return 405;
    }

    if (strcmp(req->target, "/health") == 0) {
        response_send(sock, 200, "text/plain; charset=utf-8",
                      "ok", 2, is_head, *keep_alive, NULL);
        return 200;
    }

    if (has_parent_segment(req->target)) {
        *keep_alive = 0;
        send_error(sock, 404, is_head, *keep_alive, NULL);
        return 404;
    }

    char target[REQ_MAX_TARGET];
    const char *q = strchr(req->target, '?');
    size_t n = q ? (size_t)(q - req->target) : strlen(req->target);
    if (n >= sizeof(target))
        n = sizeof(target) - 1;
    memcpy(target, req->target, n);
    target[n] = '\0';

    if (strcmp(target, "/") == 0)
        strcpy(target, "/index.html");

    size_t tlen = strlen(target);
    if (tlen > 0 && target[tlen - 1] == '/' &&
        tlen + strlen("index.html") < sizeof(target))
        strcat(target, "index.html");

    char fs_path[MAX_PATH];
    build_fs_path(fs_path, sizeof(fs_path), target);

    char canon[MAX_PATH];
    if (!GetFullPathNameA(fs_path, MAX_PATH, canon, NULL) ||
        _strnicmp(canon, g_root, g_root_len) != 0 ||
        (canon[g_root_len] != '\0' && canon[g_root_len] != '\\')) {
        *keep_alive = 0;
        send_error(sock, 404, is_head, *keep_alive, NULL);
        return 404;
    }

    int rc = serve_file(sock, canon, is_head, *keep_alive);
    if (rc == -1) {
        size_t flen = strlen(canon);
        if (flen + 12 < MAX_PATH)
            strcat(canon, "\\index.html");
        rc = serve_file(sock, canon, is_head, *keep_alive);
    }

    if (rc == 1)
        return 200;
    if (rc == 0) {
        send_error(sock, 404, is_head, *keep_alive, NULL);
        return 404;
    }
    send_error(sock, 500, is_head, *keep_alive, NULL);
    return 500;
}
