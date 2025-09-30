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
      //skip parents
      continue;
    }
    
    if (snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName) >= MAX_PATH) {
      continue; //skip if path truncated
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

static int parse_apache_listing(const char *html, const char *base_url,
                                char *files[], int max_files) {
  int count = 0;
  const char *ptr = html;

  while ((ptr = strstr(ptr, "<a href=\"")) != NULL && count < max_files) {
    ptr += 9; //skips <a href="
    const char *end = strchr(ptr, '"');
    if (!end)
      break;

    size_t len = end - ptr;
    char *filename = malloc(len + 1);
    strncpy(filename, ptr, len);
    filename[len] = '\0';

    //skips parents
    if (strcmp(filename, "../") != 0 && strcmp(filename, "./") != 0 &&
        strchr(filename, '?') == NULL && is_allowed_filetype(filename)) {

      size_t url_len = strlen(base_url) + strlen(filename) + 2;
      char *full_url = malloc(url_len);

      if (base_url[strlen(base_url) - 1] == '/') {
        snprintf(full_url, url_len, "%s%s", base_url, filename);
      }
      else {
        snprintf(full_url, url_len, "%s/%s", base_url, filename);
      }

      files[count++] = full_url;
    }

    free(filename);
    ptr = end;
  }

  return count;
}

static int parse_nginx_listing(const char *html, const char *base_url,
                               char *files[], int max_files) {
  int count = 0;
  const char *ptr = html;

  //nginx <pre> tag
  const char *pre_start = strstr(html, "<pre>");
  if (pre_start) {
    ptr = pre_start;
  }

  while ((ptr = strstr(ptr, "<a href=\"")) != NULL && count < max_files) {
    ptr += 9; //skips <a href="
    const char *end = strchr(ptr, '"');
    if (!end)
      break;

    size_t len = end - ptr;
    char *filename = malloc(len + 1);
    strncpy(filename, ptr, len);
    filename[len] = '\0';

    if (strcmp(filename, "../") != 0 && strcmp(filename, "./") != 0 &&
        filename[0] != '?' && is_allowed_filetype(filename)) {

      size_t url_len = strlen(base_url) + strlen(filename) + 2;
      char *full_url = malloc(url_len);

      if (base_url[strlen(base_url) - 1] == '/') {
        snprintf(full_url, url_len, "%s%s", base_url, filename);
      }
      else {
        snprintf(full_url, url_len, "%s/%s", base_url, filename);
      }

      files[count++] = full_url;
    }

    free(filename);
    ptr = end;
  }

  return count;
}

//json (untested rn)
static int parse_json_listing(const char *json, const char *base_url,
                              char *files[], int max_files) {
  int count = 0;
  const char *ptr = json;

  while ((ptr = strstr(ptr, "\"name\"")) != NULL && count < max_files) {
    ptr = strchr(ptr, ':');
    if (!ptr)
      break;
    ptr++;

    while (*ptr && isspace(*ptr))
      ptr++;

    if (*ptr != '"')
      continue;
    ptr++;

    const char *end = strchr(ptr, '"');
    if (!end)
      break;

    size_t len = end - ptr;
    char *filename = malloc(len + 1);
    strncpy(filename, ptr, len);
    filename[len] = '\0';

    if (is_allowed_filetype(filename)) {
      size_t url_len = strlen(base_url) + strlen(filename) + 2;
      char *full_url = malloc(url_len);

      if (base_url[strlen(base_url) - 1] == '/') {
        snprintf(full_url, url_len, "%s%s", base_url, filename);
      }
      else {
        snprintf(full_url, url_len, "%s/%s", base_url, filename);
      }

      files[count++] = full_url;
    }

    free(filename);
    ptr = end;
  }

  return count;
}

int scan_web_directory(const char *url, char *files[], const char *username,
                       const char *password) {
  CURL *curl;
  CURLcode res;
  struct MemoryStruct chunk;
  int file_count = 0;

  chunk.memory = malloc(1);
  chunk.size = 0;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();

  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    free(chunk.memory);
    return -1;
  }

  char *clean_url = NULL;
  char *url_user = NULL;
  char *url_pass = NULL;
  extract_auth_from_url(url, &clean_url, &url_user, &url_pass);

  const char *final_user = username ? username : url_user;
  const char *final_pass = password ? password : url_pass;

  curl_easy_setopt(curl, CURLOPT_URL, clean_url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

  if (final_user) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, final_user);
    if (final_pass) {
      curl_easy_setopt(curl, CURLOPT_PASSWORD, final_pass);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  }

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    file_count = -1;
  }
  else {
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 200) {
      file_count =
          parse_apache_listing(chunk.memory, clean_url, files, MAX_FILES);

      if (file_count == 0) {
        file_count =
            parse_nginx_listing(chunk.memory, clean_url, files, MAX_FILES);
      }

      if (file_count == 0) {
        file_count =
            parse_json_listing(chunk.memory, clean_url, files, MAX_FILES);
      }

      if (file_count > 0) {
        qsort(files, file_count, sizeof(char *), compare_files);
      }
      else {
        fprintf(stderr, "No media files found in directory listing\n");
      }
    }
    else if (response_code == 401) {
      fprintf(stderr, "Authentication failed (401 Unauthorized)\n");
      file_count = -1;
    }
    else {
      fprintf(stderr, "HTTP error: %ld\n", response_code);
      file_count = -1;
    }
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  free(chunk.memory);
  free(clean_url);
  if (url_user)
    free(url_user);
  if (url_pass)
    free(url_pass);

  return file_count;
}
