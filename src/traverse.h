//traverse.h
#ifndef TRAVERSE_H
#define TRAVERSE_H

#define MAX_FILES 2048

int is_allowed_filetype(const char *filename);
int compare_files(const void *a, const void *b);
int is_directory(const char *path);
int scan_directory(const char *input, char *files[]);
char *expand_path(const char *path);

#endif //TRAVERSE_H
