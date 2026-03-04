//fileutils.c
#include "traverse.h"
#include <ctype.h>
#include <curl/curl.h>
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

int is_web_url(const char *path) {
  return strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0;
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

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
    fprintf(stderr, "realloc ran out of memory\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

char *extract_auth_from_url(const char *url, char **clean_url, char **username,
                            char **password) {
  *username = NULL;
  *password = NULL;
  *clean_url = NULL;

  const char *proto_end = strstr(url, "://");
  if (!proto_end) {
    *clean_url = strdup(url);
    return *clean_url;
  }
  proto_end += 3;

  //detect inline auth
  const char *at_sign = strchr(proto_end, '@');
  if (!at_sign) {
    *clean_url = strdup(url);
    return *clean_url;
  }

  size_t auth_len = at_sign - proto_end;
  char *auth = malloc(auth_len + 1);
  strncpy(auth, proto_end, auth_len);
  auth[auth_len] = '\0';

  char *colon = strchr(auth, ':');
  if (colon) {
    *colon = '\0';
    *username = strdup(auth);
    *password = strdup(colon + 1);
  }
  else {
    *username = strdup(auth);
  }

  size_t proto_len = proto_end - url;
  size_t clean_len = proto_len + strlen(at_sign + 1) + 1;
  *clean_url = malloc(clean_len);
  strncpy(*clean_url, url, proto_len);
  strcpy(*clean_url + proto_len, at_sign + 1);

  free(auth);
  return *clean_url;
}

//rewritten to avoid seperate apache/nginx/json logic
static int parse_hrefs(const char *html, const char *base_url,
                       char *files[], int *file_count, int max_files,
                       char *subdirs[], int *subdir_count, int max_subdirs) {
  *subdir_count = 0;
  int count = 0;

  //handles <pre> tags
  const char *ptr = html;
  const char *pre_start = strstr(html, "<pre>");
  if (pre_start)
    ptr = pre_start;

  while ((ptr = strstr(ptr, "<a href=\"")) != NULL) {
    ptr += 9; // skip <a href="
    const char *end = strchr(ptr, '"');
    if (!end)
      break;

    size_t len = end - ptr;

    // Skip empty hrefs to avoid href[-1] access below
    if (len == 0) {
      ptr = end;
      continue;
    }

    char *href = malloc(len + 1);
    strncpy(href, ptr, len);
    href[len] = '\0';

    //skips parents and queries
    if (strcmp(href, "../") == 0 || strcmp(href, "./") == 0 ||
        href[0] == '?' || href[0] == '#' ||
        strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0) {
      free(href);
      ptr = end;
      continue;
    }

    char *full_url = NULL;
    if (href[0] == '/') {
      const char *origin_end = strstr(base_url, "://");
      if (origin_end) {
        origin_end = strchr(origin_end + 3, '/');
      }
      size_t origin_len = origin_end ? (size_t)(origin_end - base_url) : strlen(base_url);
      size_t url_len = origin_len + len + 1;
      full_url = malloc(url_len);
      snprintf(full_url, url_len, "%.*s%s", (int)origin_len, base_url, href);
    } else {
      size_t base_len = strlen(base_url);
      int needs_slash = (base_len > 0 && base_url[base_len - 1] != '/');
      size_t url_len = base_len + (needs_slash ? 1 : 0) + len + 1;
      full_url = malloc(url_len);
      if (needs_slash)
        snprintf(full_url, url_len, "%s/%s", base_url, href);
      else
        snprintf(full_url, url_len, "%s%s", base_url, href);
    }

    if (strncmp(full_url, base_url, strlen(base_url)) != 0) {
      free(full_url);
      free(href);
      ptr = end;
      continue;
    }

    if (href[len - 1] == '/') {
      if (*subdir_count < max_subdirs) {
        subdirs[(*subdir_count)++] = full_url;
      } else {
        free(full_url);
      }
    } else if (count < max_files && is_allowed_filetype(href)) {
      files[count++] = full_url;
    } else {
      free(full_url);
    }

    free(href);
    ptr = end;
  }

  *file_count = count;
  return count;
}

//caller needs to free chunk
static int fetch_url(CURL *curl, const char *url, struct MemoryStruct *chunk) {
  chunk->memory = malloc(1);
  chunk->size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed for %s: %s\n",
            url, curl_easy_strerror(res));
    free(chunk->memory);
    chunk->memory = NULL;
    return -1;
  }

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code == 401) {
    fprintf(stderr, "Authentication failed (401 Unauthorized) for %s\n", url);
    free(chunk->memory);
    chunk->memory = NULL;
    return -1;
  }
  if (response_code != 200) {
    fprintf(stderr, "HTTP error %ld for %s\n", response_code, url);
    free(chunk->memory);
    chunk->memory = NULL;
    return -1;
  }

  return 0;
}

static void scan_web_directory_recursive(CURL *curl, const char *url,
                                         char *files[], int *file_count,
                                         int max_files, int depth) {
  if (*file_count >= max_files)
    return;

  if (depth > MAX_WEB_DEPTH) {
    fprintf(stderr, "Warning: max recursion depth (%d) reached at %s\n",
            MAX_WEB_DEPTH, url);
    return;
  }

  struct MemoryStruct chunk;
  if (fetch_url(curl, url, &chunk) != 0)
    return;

  char *page_files[MAX_FILES];
  char *subdirs[MAX_FILES];
  int page_file_count = 0;
  int subdir_count = 0;

  parse_hrefs(chunk.memory, url,
              page_files, &page_file_count, max_files - *file_count,
              subdirs, &subdir_count, MAX_FILES);

  free(chunk.memory);

  int appended = 0;
  for (int i = 0; i < page_file_count && *file_count < max_files; i++) {
    files[(*file_count)++] = page_files[i];
    appended++;
  }
  for (int i = appended; i < page_file_count; i++) {
    free(page_files[i]);
  }

  for (int i = 0; i < subdir_count; i++) {
    scan_web_directory_recursive(curl, subdirs[i], files, file_count,
                                 max_files, depth + 1);
    free(subdirs[i]);
  }
}

int scan_web_directory(const char *url, char *files[], const char *username,
                       const char *password) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();

  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    curl_global_cleanup();
    return -1;
  }

  char *clean_url = NULL;
  char *url_user = NULL;
  char *url_pass = NULL;
  extract_auth_from_url(url, &clean_url, &url_user, &url_pass);

  const char *final_user = username ? username : url_user;
  const char *final_pass = password ? password : url_pass;

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
#ifdef _WIN32
  curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

  if (final_user) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, final_user);
    if (final_pass)
      curl_easy_setopt(curl, CURLOPT_PASSWORD, final_pass);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  }

  int file_count = 0;
  scan_web_directory_recursive(curl, clean_url, files, &file_count,
                               MAX_FILES, 0);

  if (file_count > 0) {
    qsort(files, file_count, sizeof(char *), compare_files);
  } else {
    fprintf(stderr, "No media files found in directory listing\n");
    file_count = -1;
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  free(clean_url);
  if (url_user) free(url_user);
  if (url_pass) free(url_pass);

  return file_count;
}