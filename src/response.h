#ifndef RESPONSE_H
#define RESPONSE_H

#include <winsock2.h>
#include <stddef.h>

int send_all(SOCKET sock, const char *data, int len);

int response_send(SOCKET sock, int status, const char *content_type,
                  const void *body, size_t body_len,
                  int head_only, int keep_alive, const char *extra_headers);

const char *status_reason(int status);

#endif
