#ifndef ROUTER_H
#define ROUTER_H

#include <winsock2.h>
#include "request.h"

void router_init(void);

int router_dispatch(SOCKET sock, http_request_t *req, int *keep_alive);

#endif
