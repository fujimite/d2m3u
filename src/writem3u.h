//writem3u.h
#ifndef WRITEM3U_H
#define WRITEM3U_H

typedef struct {
  char *path;
  char *filename;
  double duration;
  char *title;
} media_file;

media_file *collect_media_info(char *files[], int n, int *out_count);
int write_m3u(media_file mfs[], int count, const char *filename);
void free_media_files(media_file *mfs, int count);

#endif //WRITEM3U_H
