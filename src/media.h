//media.h
#ifndef MEDIA_H
#define MEDIA_H

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
void free_media_files(media_file *mfs, int count);

#endif //MEDIA_H
