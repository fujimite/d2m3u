//url.h
#ifndef URL_H
#define URL_H

int is_web_url(const char *path);
char *extract_auth_from_url(const char *url, char **clean_url, char **username,
                            char **password);

#endif //URL_H
