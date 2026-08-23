#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

#define REQ_MAX_TARGET 2048

typedef enum {
    REQ_OK = 0,
    REQ_NEED_MORE,
    REQ_BAD,
    REQ_TOO_LARGE
} req_status_t;

typedef struct {
    char method[16];
    char target[REQ_MAX_TARGET];
    char version[16];
    char host[256];
    size_t content_length;
    int keep_alive;
    int head_only;
} http_request_t;

req_status_t request_parse(const char *buf, size_t len,
                           http_request_t *out, size_t *consumed);

#endif
