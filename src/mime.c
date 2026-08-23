#include <string.h>
#include "mime.h"

static const struct {
    const char *ext;
    const char *type;
} MIME_TABLE[] = {
    { ".html",  "text/html; charset=utf-8" },
    { ".htm",   "text/html; charset=utf-8" },
    { ".css",   "text/css; charset=utf-8" },
    { ".js",    "text/javascript; charset=utf-8" },
    { ".mjs",   "text/javascript; charset=utf-8" },
    { ".json",  "application/json" },
    { ".map",   "application/json" },
    { ".txt",   "text/plain; charset=utf-8" },
    { ".md",    "text/markdown; charset=utf-8" },
    { ".xml",   "application/xml" },
    { ".png",   "image/png" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".gif",   "image/gif" },
    { ".svg",   "image/svg+xml" },
    { ".ico",   "image/x-icon" },
    { ".webp",  "image/webp" },
    { ".woff",  "font/woff" },
    { ".woff2", "font/woff2" },
    { ".ttf",   "font/ttf" },
    { ".mp4",   "video/mp4" },
    { ".webm",  "video/webm" },
    { ".pdf",   "application/pdf" },
    { ".wasm",  "application/wasm" },
};

const char *mime_from_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot)
        return "application/octet-stream";

    for (size_t i = 0; i < sizeof(MIME_TABLE) / sizeof(MIME_TABLE[0]); i++)
        if (_stricmp(dot, MIME_TABLE[i].ext) == 0)
            return MIME_TABLE[i].type;

    return "application/octet-stream";
}
