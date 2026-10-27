//url.c
#include "url.h"
#include <stdio.h>
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

static int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

//decodes percent encoding except escape chars. caller needs to free result
char *decode_url_name(const char *name) {
  size_t len = strlen(name);
  char *decoded = malloc(len + 1);
  if (!decoded)
    return NULL;

  size_t out = 0;
  for (size_t i = 0; i < len; i++) {
    if (name[i] == '%' && hex_value(name[i + 1]) >= 0 &&
        hex_value(name[i + 2]) >= 0) {
      int value = hex_value(name[i + 1]) * 16 + hex_value(name[i + 2]);
      if (value >= 0x20 && value != 0x7f) {
        decoded[out++] = (char)value;
        i += 2;
        continue;
      }
    }
    decoded[out++] = name[i];
  }
  decoded[out] = '\0';
  return decoded;
}

//returns null if there is nothing to embed. caller needs to free result
char *embed_auth_in_url(const char *url, const char *username,
                        const char *password) {
  if (!is_web_url(url) || !username || !password)
    return NULL;

  const char *proto_end = strstr(url, "://") + 3;
  if (strchr(proto_end, '@'))
    return NULL;

  size_t proto_len = proto_end - url;
  size_t new_url_len = strlen(url) + strlen(username) + strlen(password) + 10;

  char *new_url = malloc(new_url_len);
  if (!new_url)
    return NULL;

  snprintf(new_url, new_url_len, "%.*s%s:%s@%s", (int)proto_len, url, username,
           password, proto_end);
  return new_url;
}
