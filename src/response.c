#include <stdio.h>
#include <string.h>
#include "response.h"

int send_all(SOCKET sock, const char *data, int len)
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

const char *status_reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Content Too Large";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 505: return "HTTP Version Not Supported";
    default:  return "Unknown";
    }
}

int response_send(SOCKET sock, int status, const char *content_type,
                  const void *body, size_t body_len,
                  int head_only, int keep_alive, const char *extra_headers)
{
    char hdr[1024];
    int hl = snprintf(hdr, sizeof(hdr),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %zu\r\n"
                      "%s"
                      "Connection: %s\r\n"
                      "\r\n",
                      status, status_reason(status),
                      content_type ? content_type : "application/octet-stream",
                      body_len,
                      extra_headers ? extra_headers : "",
                      keep_alive ? "keep-alive" : "close");
    if (hl <= 0)
        return 0;
    if (!send_all(sock, hdr, hl))
        return 0;

    if (head_only || !body || body_len == 0)
        return 1;
    return send_all(sock, (const char *)body, (int)body_len);
}
