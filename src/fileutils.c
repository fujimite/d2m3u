//fileutils.c
#include "fileutils.h"
#include <dirent.h>
#include <fts.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES 2048

int is_allowed_filetype(const char *filename) {
  const char *ext_list[] = {"mp3",  "wav",  "aac",  "flac", "ogg",  "wma",
                            "m4a",  "aiff", "alac", "mp4",  "avi",  "mov",
                            "mkv",  "webm", "flv",  "wmv",  "mpeg", "3gp",
                            "rmvb", "m4v",  NULL};

  if (filename[0] == '.' && strchr(filename + 1, '.') == NULL) {
    return 0;
  }

  const char *ext = strrchr(filename, '.');
  if (ext == NULL)
    return 0;

  for (int i = 0; ext_list[i] != NULL; i++) {
    if (strcasecmp(ext + 1, ext_list[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int compare_files(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

int is_directory(const char *path) {
  struct stat statbuf;
  return stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
}

int scan_directory(const char *input, char *files[]) {
  int file_count = 0;

  char *const paths[] = {(char *)input, NULL};
  FTS *fts = fts_open(paths, FTS_NOCHDIR | FTS_PHYSICAL, NULL);
  if (fts == NULL) {
    perror("fts_open");
    return -1;
  }

  FTSENT *entry;
  while ((entry = fts_read(fts)) != NULL) {
    if (file_count == MAX_FILES)
      break;

    if (entry->fts_info == FTS_F && is_allowed_filetype(entry->fts_name)) {
      files[file_count] = strdup(entry->fts_path);
      file_count++;
    }
  }

  fts_close(fts);
  qsort(files, file_count, sizeof(char *), compare_files);

  return file_count;
}

char *expand_path(const char *path) {
  if (path[0] == '~') {
    const char *home = getenv("HOME");
    if (!home) {
      struct passwd *pw = getpwuid(getuid());
      if (pw)
        home = pw->pw_dir;
    }

    if (!home)
      return strdup(path);

    char *expanded = malloc(strlen(home) + strlen(path));
    if (!expanded)
      return NULL;

    sprintf(expanded, "%s%s", home, path + 1);
    return expanded;
  }
  else {
    return strdup(path);
  }
}