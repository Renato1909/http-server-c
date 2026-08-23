#include <string.h>
#include <ctype.h>
#include "request.h"

#define MAX_HEAD_BYTES 16384

static const char *find_header_end(const char *buf, size_t len, size_t *head_len)
{
    if (len < 4)
        return NULL;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            *head_len = i;
            return buf + i;
        }
    }
    return NULL;
}

static void copy_token(char *dst, size_t cap, const char *src, size_t n)
{
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int value_has(const char *v, size_t n, const char *needle)
{
    size_t m = strlen(needle);
    if (n < m)
        return 0;
    for (size_t i = 0; i + m <= n; i++)
        if (_strnicmp(v + i, needle, m) == 0)
            return 1;
    return 0;
}

static void parse_header(const char *name, size_t name_len,
                         const char *val, size_t val_len,
                         http_request_t *out)
{
    if (name_len == 10 && _strnicmp(name, "connection", 10) == 0) {
        if (value_has(val, val_len, "close"))
            out->keep_alive = 0;
        else if (value_has(val, val_len, "keep-alive"))
            out->keep_alive = 1;
    } else if (name_len == 14 && _strnicmp(name, "content-length", 14) == 0) {
        unsigned long long cl = 0;
        for (size_t i = 0; i < val_len && isdigit((unsigned char)val[i]); i++) {
            cl = cl * 10 + (unsigned long long)(val[i] - '0');
            if (cl > (1ULL << 30)) {
                cl = (1ULL << 30) + 1;
                break;
            }
        }
        out->content_length = (size_t)cl;
    } else if (name_len == 4 && _strnicmp(name, "host", 4) == 0) {
        copy_token(out->host, sizeof(out->host), val, val_len);
    }
}

req_status_t request_parse(const char *buf, size_t len,
                           http_request_t *out, size_t *consumed)
{
    memset(out, 0, sizeof(*out));

    size_t head_len = 0;
    if (!find_header_end(buf, len, &head_len))
        return len >= MAX_HEAD_BYTES ? REQ_TOO_LARGE : REQ_NEED_MORE;

    const char *end = buf + head_len;

    const char *eol1 = memchr(buf, '\r', head_len);
    if (!eol1 || eol1 == buf || eol1 + 1 >= end || eol1[1] != '\n')
        return REQ_BAD;

    const char *sp1 = memchr(buf, ' ', (size_t)(eol1 - buf));
    if (!sp1 || sp1 == buf)
        return REQ_BAD;
    copy_token(out->method, sizeof(out->method), buf, (size_t)(sp1 - buf));

    const char *t0 = sp1 + 1;
    const char *sp2 = memchr(t0, ' ', (size_t)(eol1 - t0));
    if (!sp2 || sp2 == t0)
        return REQ_BAD;
    copy_token(out->target, sizeof(out->target), t0, (size_t)(sp2 - t0));
    if (out->target[0] != '/')
        return REQ_BAD;

    copy_token(out->version, sizeof(out->version), sp2 + 1,
               (size_t)(eol1 - (sp2 + 1)));
    if (strncmp(out->version, "HTTP/", 5) != 0)
        return REQ_BAD;

    int http11 = strcmp(out->version, "HTTP/1.1") == 0;
    int http10 = strcmp(out->version, "HTTP/1.0") == 0;
    if (!http11 && !http10)
        return REQ_BAD;

    out->keep_alive = http11;
    out->head_only = strcmp(out->method, "HEAD") == 0;

    const char *line = eol1 + 2;
    while (line < end) {
        const char *eol = memchr(line, '\r', (size_t)(end - line));
        if (!eol || eol == line)
            break;
        if (eol + 1 >= end || eol[1] != '\n')
            break;

        const char *colon = memchr(line, ':', (size_t)(eol - line));
        if (colon) {
            const char *val = colon + 1;
            while (val < eol && (*val == ' ' || *val == '\t'))
                val++;
            parse_header(line, (size_t)(colon - line), val,
                         (size_t)(eol - val), out);
        }
        line = eol + 2;
    }

    *consumed = head_len + 4;
    return REQ_OK;
}
