//writem3u.c
#include "writem3u.h"
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define PATH_SEP '\\'
  #define PATH_SEP_STR "\\"
  #define getcwd _getcwd
  #define stat _stat
  #define basename win_basename
  #define strcasecmp _stricmp

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
  #include <unistd.h>
  #define PATH_SEP '/'
  #define PATH_SEP_STR "/"
#endif

static int is_web_url(const char *path) {
  return strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0;
}

//percent decoder
static char *url_decode(const char *src) {
  size_t len = strlen(src);
  char *out = malloc(len + 1);
  if (!out)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (src[i] == '%' && i + 2 < len &&
        isxdigit((unsigned char)src[i + 1]) &&
        isxdigit((unsigned char)src[i + 2])) {
      char hex[3] = { src[i + 1], src[i + 2], '\0' };
      out[j++] = (char)strtol(hex, NULL, 16);
      i += 2;
    }
    else if (src[i] == '+') {
      out[j++] = ' ';
    }
    else {
      out[j++] = src[i];
    }
  }
  out[j] = '\0';
  return out;
}

static void strip_extension(char *name) {
  char *dot = strrchr(name, '.');
  if (dot && dot != name)
    *dot = '\0';
}

static char *dir_of(const char *path) {
  const char *last = NULL;
  const char *p = path;
  while (*p) {
    if (*p == '/'
#ifdef _WIN32
        || *p == '\\'
#endif
    ) {
      last = p;
    }
    p++;
  }
  if (!last)
    return strdup(".");
  size_t len = last - path;
  char *dir = malloc(len + 1);
  strncpy(dir, path, len);
  dir[len] = '\0';
  return dir;
}


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
        //decode escapes for title display; mf->path keeps raw
        char *decoded = url_decode(filename);
        free(filename);
        mf->filename = decoded ? decoded : strdup(last_slash + 1);
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
    strip_extension(mf->filename);

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

int write_m3u(media_file mfs[], int count, const char *filename, int embed_auth,
              const char *username, const char *password) {
  char filepath[PATH_MAX * 2 + 16];

  const char *output_file =
      (filename && strlen(filename) > 0) ? filename : "playlist.m3u";

  char output_file_buf[PATH_MAX];
  if (strlen(output_file) < 4 ||
      strcasecmp(output_file + strlen(output_file) - 4, ".m3u") != 0) {
    snprintf(output_file_buf, sizeof(output_file_buf), "%s.m3u", output_file);
    output_file = output_file_buf;
  }

  struct stat path_stat;
  if (stat(output_file, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
    snprintf(filepath, sizeof(filepath), "%s%cplaylist.m3u",
             output_file, PATH_SEP);
  }
  else if (output_file[0] == '/'
#ifdef _WIN32
          || (strlen(output_file) > 1 && output_file[1] == ':') //C:\blabla
#endif
         ) {
    strncpy(filepath, output_file, sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0';
  }
  else {
    if (!getcwd(filepath, sizeof(filepath))) {
      perror("getcwd");
      return -1;
    }
    strncat(filepath, PATH_SEP_STR, sizeof(filepath) - strlen(filepath) - 1);
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

    if (mf->duration > 0) {
      fprintf(fp, "#EXTINF:%.0f,", mf->duration);
    }
    else {
      fprintf(fp, "#EXTINF:-1,");
    }

    //fallback to filename if no title
    if (mf->title)
      fprintf(fp, "%s", mf->title);
    else
      fprintf(fp, "%s", mf->filename);
    fprintf(fp, "\n");

    if (embed_auth && is_web_url(mf->path) && username && password) {
      const char *auth_start = strstr(mf->path, "://");
      if (auth_start) {
        auth_start += 3;
        const char *at_sign = strchr(auth_start, '@');

        if (at_sign) {
          fprintf(fp, "%s\n", mf->path);
        }
        else {
          const char *proto_end = strstr(mf->path, "://") + 3;
          size_t proto_len = proto_end - mf->path;
          fprintf(fp, "%.*s%s:%s@%s\n", (int)proto_len, mf->path, username,
                  password, proto_end);
        }
      }
      else {
        fprintf(fp, "%s\n", mf->path);
      }
    }
    else {
      fprintf(fp, "%s\n", mf->path);
    }
  }

  fclose(fp);

  //printf("Playlist written to: %s\n", filepath);
  return 0;
}

int write_m3u_split(char *files[], int file_count, const char *output_dir,
                    int embed_auth, const char *username, const char *password,
                    int verbose) {
  if (file_count == 0)
    return 0;

  char out_dir[PATH_MAX];
  char base_name[PATH_MAX];
  strncpy(base_name, "playlist", sizeof(base_name) - 1);
  base_name[sizeof(base_name) - 1] = '\0';

  if (output_dir && strlen(output_dir) > 0) {
    struct stat od_stat;
    int is_existing_dir = (stat(output_dir, &od_stat) == 0 && S_ISDIR(od_stat.st_mode));
    char last_ch = output_dir[strlen(output_dir) - 1];
    int ends_with_sep = (last_ch == '/'
#ifdef _WIN32
                         || last_ch == '\\'
#endif
                        );

    if (is_existing_dir || ends_with_sep) {
      strncpy(out_dir, output_dir, sizeof(out_dir) - 1);
      out_dir[sizeof(out_dir) - 1] = '\0';
    }
    else {
      char *tmp = strdup(output_dir);
      char *last_sep = strrchr(tmp, '/');
#ifdef _WIN32
      char *last_sep2 = strrchr(tmp, '\\');
      if (!last_sep || (last_sep2 && last_sep2 > last_sep))
        last_sep = last_sep2;
#endif
      if (last_sep) {
        *last_sep = '\0';
        strncpy(out_dir, tmp, sizeof(out_dir) - 1);
        out_dir[sizeof(out_dir) - 1] = '\0';
        //strips .m3u if user wrote it
        char *ext = strrchr(last_sep + 1, '.');
        if (ext && strcasecmp(ext, ".m3u") == 0)
          *ext = '\0';
        strncpy(base_name, last_sep + 1, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
      }
      else {
        if (!getcwd(out_dir, sizeof(out_dir))) {
          perror("getcwd");
          free(tmp);
          return -1;
        }
        char *ext = strrchr(tmp, '.');
        if (ext && strcasecmp(ext, ".m3u") == 0)
          *ext = '\0';
        strncpy(base_name, tmp, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
      }
      free(tmp);
    }
  }
  else {
    if (!getcwd(out_dir, sizeof(out_dir))) {
      perror("getcwd");
      return -1;
    }
  }

  size_t odlen = strlen(out_dir);
  if (odlen > 1 && (out_dir[odlen - 1] == '/'
#ifdef _WIN32
                    || out_dir[odlen - 1] == '\\'
#endif
                    )) {
    out_dir[odlen - 1] = '\0';
  }

  int playlist_index = 0;
  int result = 0;
  int i = 0;

  while (i < file_count) {
    char *cur_dir = dir_of(files[i]);
    int j = i + 1;

    while (j < file_count) {
      char *d = dir_of(files[j]);
      int same = (strcmp(d, cur_dir) == 0);
      free(d);
      if (!same)
        break;
      j++;
    }

    int group_count = j - i;
    playlist_index++;

    //appends 01_, 02_, etc
    char playlist_name[PATH_MAX * 2 + 16];
    snprintf(playlist_name, sizeof(playlist_name), "%s%c%02d_%s.m3u",
             out_dir, PATH_SEP, playlist_index, base_name);
    free(cur_dir);

    if (verbose) {
      printf("Writing playlist %d: %s (%d file%s)\n",
             playlist_index, playlist_name, group_count,
             group_count == 1 ? "" : "s");
    }

    int media_count = 0;
    media_file *mfs = collect_media_info(&files[i], group_count, &media_count,
                                         username, password);
    if (!mfs || media_count == 0) {
      fprintf(stderr, "Warning: no media info for group %d, skipping.\n",
              playlist_index);
      if (mfs)
        free_media_files(mfs, media_count);
      i = j;
      continue;
    }

    FILE *fp = fopen(playlist_name, "w");
    if (!fp) {
      perror("fopen");
      free_media_files(mfs, media_count);
      i = j;
      result = -1;
      continue;
    }

    fprintf(fp, "#EXTM3U\n");
    for (int k = 0; k < media_count; k++) {
      media_file *mf = &mfs[k];
      fprintf(fp, "#EXTINF:%.0f,%s\n",
              mf->duration > 0 ? mf->duration : -1.0,
              mf->title ? mf->title : mf->filename);

      if (embed_auth && is_web_url(mf->path) && username && password) {
        const char *auth_start = strstr(mf->path, "://");
        if (auth_start) {
          auth_start += 3;
          const char *at_sign = strchr(auth_start, '@');
          if (at_sign) {
            fprintf(fp, "%s\n", mf->path);
          }
          else {
            const char *proto_end = strstr(mf->path, "://") + 3;
            size_t proto_len = proto_end - mf->path;
            fprintf(fp, "%.*s%s:%s@%s\n", (int)proto_len, mf->path,
                    username, password, proto_end);
          }
        }
        else {
          fprintf(fp, "%s\n", mf->path);
        }
      }
      else {
        fprintf(fp, "%s\n", mf->path);
      }
    }

    fclose(fp);
    free_media_files(mfs, media_count);
    i = j;
  }

  if (verbose && result == 0) {
    printf("Split into %d playlist%s.\n", playlist_index,
           playlist_index == 1 ? "" : "s");
  }

  return result;
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