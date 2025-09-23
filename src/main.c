#include "fileutils.h"
#include "writem3u.h"
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 2048

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 4) {
    printf("Usage: %s [-v8] <dir|file> [output]\n", argv[0]);
    return -1;
  }

  int flag_verbose = 0;
  int flag_8 = 0; //unused currently. to determine unicode-8 (m3u8) encoding
  const char *input = NULL;
  const char *output_filename = NULL;

  int arg_index = 1;

  while (arg_index < argc && argv[arg_index][0] == '-') {
    const char *flags = argv[arg_index] + 1; // skip '-'
    if (*flags == '\0') {
      fprintf(stderr, "Invalid flag: %s\n", argv[arg_index]);
      return -1;
    }
    for (; *flags != '\0'; flags++) {
      switch (*flags) {
      case 'v':
        flag_verbose = 1;
        break;
      case '8':
        flag_8 = 1;
        break;
      default:
        fprintf(stderr, "Unknown flag: -%c\n", *flags);
        return -1;
      }
    }
    arg_index++;
  }

  if (arg_index >= argc) {
    fprintf(stderr, "ERROR: Missing required <dir|file> argument.\n");
    printf("Usage: %s [-v8] <dir|file> [output]\n", argv[0]);
    return -1;
  }

  input = argv[arg_index++];

  if (arg_index < argc) {
    output_filename = expand_path(argv[arg_index++]);
  }

  if (arg_index < argc) {
    fprintf(stderr, "Unknown extra argument: %s\n", argv[arg_index]);
    return -1;
  }

  if (flag_verbose) {
    av_log_set_level(AV_LOG_VERBOSE);
  }
  else {
    av_log_set_level(AV_LOG_FATAL);
  }

  char *files[MAX_FILES];
  int file_count = 0;

  if (is_directory(input)) {
    file_count = scan_directory(input, files);
  }
  else {
    if (is_allowed_filetype(input)) {
      files[0] = strdup(input);
      file_count = 1;
    }
    else {
      fprintf(stderr, "ERROR: %s is not a media file.\n", input);
      return -1;
    }
  }

  int media_count;
  media_file *mfs = collect_media_info(files, file_count, &media_count);
  if (!mfs || media_count == 0) {
    fprintf(stderr, "Failed to collect media info.\n");
    return -1;
  }

  int result = write_m3u(mfs, media_count, output_filename);

  free_media_files(mfs, media_count);
  for (int i = 0; i < file_count; i++) {
    free(files[i]);
  }
  if (output_filename) {
    free((void *)output_filename);
  }

  return result;
}
