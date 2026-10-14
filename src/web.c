//web.c
#include "traverse.h"
#include "url.h"
#include "web.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
