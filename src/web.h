//web.h
#ifndef WEB_H
#define WEB_H

#include <stddef.h>

#define MAX_WEB_DEPTH 4

struct MemoryStruct {
  char *memory;
  size_t size;
};

int scan_web_directory(const char *url, char *files[], const char *username,
                       const char *password);

#endif //WEB_H
