//url.h
#ifndef URL_H
#define URL_H

int is_web_url(const char *path);
char *extract_auth_from_url(const char *url, char **clean_url, char **username,
                            char **password);
char *embed_auth_in_url(const char *url, const char *username,
                        const char *password);
char *decode_url_name(const char *name);

#endif //URL_H
