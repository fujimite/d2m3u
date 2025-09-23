// writem3u.c
#include "writem3u.h"
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

media_file *collect_media_info(char *files[], int n, int *out_count) {
  media_file *mfs = malloc(n * sizeof(media_file));
  if (!mfs) {
    perror("malloc");
    *out_count = 0;
    return NULL;
  }

  int actual_count = 0;

  for (int i = 0; i < n; i++) {
    AVFormatContext *context = NULL;

    if (avformat_open_input(&context, files[i], NULL, NULL) != 0) {
      fprintf(stderr, "Could not open file %s\n", files[i]);
      continue;
    }

    if (avformat_find_stream_info(context, NULL) < 0) {
      fprintf(stderr, "Could not find stream information for file %s\n",
              files[i]);
      avformat_close_input(&context);
      continue;
    }

    media_file *mf = &mfs[actual_count];
    mf->path = strdup(files[i]);

    char *path_copy = strdup(files[i]);
    mf->filename = strdup(basename(path_copy));
    free(path_copy);

    mf->duration = (double)context->duration / AV_TIME_BASE;

    AVDictionaryEntry *title_entry =
        av_dict_get(context->metadata, "title", NULL, 0);
    mf->title = title_entry ? strdup(title_entry->value) : NULL;

    avformat_close_input(&context);
    actual_count++;
  }

  *out_count = actual_count;
  return mfs;
}

int write_m3u(media_file mfs[], int count, const char *filename) {
  char filepath[PATH_MAX];

  const char *output_file =
      (filename && strlen(filename) > 0) ? filename : "playlist.m3u";

  struct stat path_stat;
  if (stat(output_file, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
    snprintf(filepath, sizeof(filepath), "%s/playlist.m3u", output_file);
  }
  else if (output_file[0] == '/') {
    strncpy(filepath, output_file, sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0';
  }
  else {
    if (!getcwd(filepath, sizeof(filepath))) {
      perror("getcwd");
      return -1;
    }
    strncat(filepath, "/", sizeof(filepath) - strlen(filepath) - 1);
    strncat(filepath, output_file, sizeof(filepath) - strlen(filepath) - 1);
  }

  FILE *fp = fopen(filepath, "w");
  if (!fp) {
    perror("fopen");
    return -1;
  }

  fprintf(fp, "#EXTM3U\n");

  for (int i = 0; i < count; i++) {
    media_file *mf = &mfs[i];

    fprintf(fp, "#EXTINF:%.0f,", mf->duration);
    if (mf->title)
      fprintf(fp, "%s", mf->title);
    else
      fprintf(fp, "%s", mf->filename);
    fprintf(fp, "\n");

    fprintf(fp, "%s\n", mf->path);
  }

  fclose(fp);

  //printf("playlist written to: %s\n", filepath);
  return 0;
}

void free_media_files(media_file *mfs, int count) {
  for (int i = 0; i < count; i++) {
    free(mfs[i].path);
    free(mfs[i].filename);
    if (mfs[i].title)
      free(mfs[i].title);
  }
  free(mfs);
}
