//writem3u.c
#include "url.h"
#include "writem3u.h"
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
  #define strcasecmp _stricmp
#else
  #include <unistd.h>
  #define PATH_SEP '/'
  #define PATH_SEP_STR "/"
#endif

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

    char *auth_url =
        embed_auth ? embed_auth_in_url(mf->path, username, password) : NULL;
    fprintf(fp, "%s\n", auth_url ? auth_url : mf->path);
    if (auth_url)
      free(auth_url);
  }

  fclose(fp);

  //still need to make verbose mode useful kw
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

      char *auth_url =
          embed_auth ? embed_auth_in_url(mf->path, username, password) : NULL;
      fprintf(fp, "%s\n", auth_url ? auth_url : mf->path);
      if (auth_url)
        free(auth_url);
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
