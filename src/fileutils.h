//fileutils.h
#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <ctype.h>

#define MAX_FILES 2048

struct MemoryStruct {
  char *memory;
  size_t size;
};

int is_allowed_filetype(const char *filename);
int compare_files(const void *a, const void *b);
int is_directory(const char *path);
int scan_directory(const char *input, char *files[]);
char *expand_path(const char *path);
int is_web_url(const char *path);
int scan_web_directory(const char *url, char *files[], const char *username,
                       const char *password);
char *extract_auth_from_url(const char *url, char **clean_url, char **username,
                            char **password);

#endif //FILEUTILS_H
