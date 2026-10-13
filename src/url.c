//url.c
#include "url.h"
#include <stdlib.h>
#include <string.h>

int is_web_url(const char *path) {
  return strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0;
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
