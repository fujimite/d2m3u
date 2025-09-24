//writem3u.h
#ifndef WRITEM3U_H
#define WRITEM3U_H

#define ANALYSIS_DURATION 5000000 //5s
#define PROBE_SIZE 1000000        //1mb

typedef struct {
  char *path;
  char *filename;
  double duration;
  char *title;
} media_file;

media_file *collect_media_info(char *files[], int n, int *out_count,
                               const char *username, const char *password);
int write_m3u(media_file mfs[], int count, const char *filename, int embed_auth,
              const char *username, const char *password);
void free_media_files(media_file *mfs, int count);

#endif //WRITEM3U_H