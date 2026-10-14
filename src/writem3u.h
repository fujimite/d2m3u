//writem3u.h
#ifndef WRITEM3U_H
#define WRITEM3U_H

#include "media.h"

int write_m3u(media_file mfs[], int count, const char *filename, int embed_auth,
              const char *username, const char *password);
int write_m3u_split(char *files[], int file_count, const char *output_dir,
                    int embed_auth, const char *username, const char *password,
                    int verbose);

#endif //WRITEM3U_H
