//traverse.c
#include "traverse.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
  #define strcasecmp _stricmp
  #define stat _stat
#else
  #include <dirent.h>
  #include <fts.h>
  #include <pwd.h>
  #include <unistd.h>
#endif

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

#ifdef _WIN32
static int scan_directory_recurse(const char *dir_path, char *files[], int *file_count, int max_files) {
  WIN32_FIND_DATA find_data;
  HANDLE find_handle;
  char search_path[MAX_PATH];
  char full_path[MAX_PATH];

  if (strlen(dir_path) + 2 >= MAX_PATH) {
    return -1;
  }
  snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

  find_handle = FindFirstFile(search_path, &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  do {
    if (strcmp(find_data.cFileName, ".") == 0 ||
        strcmp(find_data.cFileName, "..") == 0) {
      continue;
    }

    if (snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName) >= MAX_PATH) {
      continue;
    }

    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      scan_directory_recurse(full_path, files, file_count, max_files);
    } else {
      if (*file_count < max_files && is_allowed_filetype(find_data.cFileName)) {
        files[*file_count] = strdup(full_path);
        (*file_count)++;
      }
    }

    if (*file_count >= max_files) {
      break;
    }
  } while (FindNextFile(find_handle, &find_data));

  FindClose(find_handle);
  return 0;
}

int scan_directory(const char *input, char *files[]) {
  int file_count = 0;

  scan_directory_recurse(input, files, &file_count, MAX_FILES);

  if (file_count > 0) {
    qsort(files, file_count, sizeof(char *), compare_files);
  }

  return file_count;
}

#else
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
#endif

char *expand_path(const char *path) {
  if (path[0] == '~') {
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
    if (!home) {
      home = getenv("HOME");
    }
#else
    const char *home = getenv("HOME");
    if (!home) {
      struct passwd *pw = getpwuid(getuid());
      if (pw)
        home = pw->pw_dir;
    }
#endif

    if (!home)
      return strdup(path);

    // home + (path+1) + NUL: path+1 skips '~', so strlen(home) + strlen(path) - 1 + 1
    char *expanded = malloc(strlen(home) + strlen(path) + 1);
    if (!expanded)
      return NULL;

    sprintf(expanded, "%s%s", home, path + 1);
    return expanded;
  }
  else {
    return strdup(path);
  }
}
