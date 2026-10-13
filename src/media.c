//media.c
#include "media.h"
#include "url.h"
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #define basename win_basename

  static char *win_basename(char *path) {
    static char fname[_MAX_FNAME];
    static char ext[_MAX_EXT];
    static char result[_MAX_FNAME + _MAX_EXT];
    _splitpath(path, NULL, NULL, fname, ext);
    strcpy(result, fname);
    strcat(result, ext);
    return result;
  }
#else
  #include <libgen.h>
#endif

media_file *collect_media_info(char *files[], int n, int *out_count,
                               const char *username, const char *password) {
  media_file *mfs = malloc(n * sizeof(media_file));
  if (!mfs) {
    perror("malloc");
    *out_count = 0;
    return NULL;
  }

  int actual_count = 0;

  for (int i = 0; i < n; i++) {
    AVFormatContext *context = NULL;
    AVDictionary *options = NULL;

    char *url_to_open = files[i];
    char *modified_url = NULL;

    if (is_web_url(files[i])) {
      av_dict_set(&options, "timeout", "10000000", 0); // 10s
      av_dict_set(&options, "user_agent", "libavformat", 0);

      if (username && password) {
        //ffmpeg requires embedded credentials
        const char *auth_start = strstr(files[i], "://");
        if (auth_start) {
          auth_start += 3;
          const char *at_sign = strchr(auth_start, '@');

          if (at_sign) {
            url_to_open = files[i];
          }
          else {
            const char *proto_end = strstr(files[i], "://") + 3;
            size_t proto_len = proto_end - files[i];
            size_t new_url_len =
                strlen(files[i]) + strlen(username) + strlen(password) + 10;

            modified_url = malloc(new_url_len);
            snprintf(modified_url, new_url_len, "%.*s%s:%s@%s", (int)proto_len,
                     files[i], username, password, proto_end);
            url_to_open = modified_url;
          }
        }
      }
    }

    if (avformat_open_input(&context, url_to_open, NULL, &options) != 0) {
      fprintf(stderr, "Could not open file %s\n", files[i]);
      av_dict_free(&options);
      if (modified_url)
        free(modified_url);
      continue;
    }

    if (is_web_url(files[i])) {
      context->max_analyze_duration = ANALYSIS_DURATION;
      context->probesize = PROBE_SIZE;
    }

    if (avformat_find_stream_info(context, NULL) < 0) {
      fprintf(stderr, "Could not find stream information for file %s\n",
              files[i]);
      avformat_close_input(&context);
      av_dict_free(&options);
      if (modified_url)
        free(modified_url);
      continue;
    }

    media_file *mf = &mfs[actual_count];
    mf->path = strdup(files[i]);

    if (is_web_url(files[i])) {
      const char *last_slash = strrchr(files[i], '/');
      if (last_slash && *(last_slash + 1)) {
        char *filename = strdup(last_slash + 1);
        //remove queries
        char *query = strchr(filename, '?');
        if (query)
          *query = '\0';
        mf->filename = filename;
      }
      else {
        mf->filename = strdup("webstream");
      }
    }
    else {
      char *path_copy = strdup(files[i]);
      mf->filename = strdup(basename(path_copy));
      free(path_copy);
    }

    mf->duration = (double)context->duration / AV_TIME_BASE;

    //fallback duration to zero
    if (context->duration == AV_NOPTS_VALUE || mf->duration < 0) {
      mf->duration = 0;
    }

    AVDictionaryEntry *title_entry =
        av_dict_get(context->metadata, "title", NULL, 0);
    mf->title = title_entry ? strdup(title_entry->value) : NULL;

    avformat_close_input(&context);
    av_dict_free(&options);
    if (modified_url)
      free(modified_url);
    actual_count++;
  }

  *out_count = actual_count;
  return mfs;
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
