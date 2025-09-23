//fileutils.h
#ifndef FILEUTILS_H
#define FILEUTILS_H

int is_allowed_filetype(const char *filename);
int compare_files(const void *a, const void *b);
int is_directory(const char *path);
int scan_directory(const char *input, char *files[]);
char *expand_path(const char *path);

#endif //FILEUTILS_H
