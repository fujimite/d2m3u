#include "traverse.h"
#include "writem3u.h"
#include <getopt.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *name);

int main(int argc, char *argv[]) {
  int flag_verbose = 0;
  int flag_8 = 0; //unused currently. to determine unicode-8 (m3u8) encoding
  int flag_embed_auth = 0;
  const char *input = NULL;
  const char *output_filename = NULL;
  char *username = NULL;
  char *password = NULL;

  static struct option long_options[] = {
      {"verbose", no_argument, 0, 'v'},
      {"utf8", no_argument, 0, '8'},
      {"username", required_argument, 0, 'u'},
      {"password", required_argument, 0, 'p'},
      {"embed-auth", no_argument, 0, 'e'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "v8u:p:eh", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'v':
      flag_verbose = 1;
      break;
    case '8':
      //not implemented yet
      //flag_8 = 1;
      break;
    case 'u':
      username = strdup(optarg);
      break;
    case 'p':
      password = strdup(optarg);
      break;
    case 'e':
      flag_embed_auth = 1;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    case '?':
      fprintf(stderr, "Unknown option. Use -h for help.\n");
      return -1;
    }
  }

  // Check for required arguments
  if (optind >= argc) {
    fprintf(
        stderr,
        "ERROR: Missing required <dir|file|url> argument. Use -h for help.\n");
    return -1;
  }

  input = argv[optind++];

  if (optind < argc) {
    output_filename = expand_path(argv[optind++]);
  }

  if (optind < argc) {
    fprintf(stderr, "Unknown extra argument: %s\n", argv[optind]);
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
  char *final_username = NULL;
  char *final_password = NULL;

  if (is_web_url(input)) {
    if (is_allowed_filetype(input)) { //single web media
      if (flag_verbose) {
        printf("Processing web media file: %s\n", input);
      }
      files[0] = strdup(input);
      file_count = 1;

      if (username) {
        final_username = username;
        final_password = password;
      }
      else {
        char *clean_url = NULL;
        extract_auth_from_url(input, &clean_url, &final_username,
                              &final_password);
        free(clean_url);
      }
    }
    else { //web directory
      if (flag_verbose) {
        printf("Scanning web directory: %s\n", input);
      }
      file_count = scan_web_directory(input, files, username, password);
      if (file_count < 0) {
        fprintf(stderr, "Failed to scan web directory.\n");
        if (username)
          free(username);
        if (password)
          free(password);
        return -1;
      }

      if (username) {
        final_username = username;
        final_password = password;
      }
      else {
        char *clean_url = NULL;
        extract_auth_from_url(input, &clean_url, &final_username,
                              &final_password);
        free(clean_url);
      }
    }
  }
  else if (is_directory(input)) {
    if (flag_verbose) {
      printf("Scanning local directory: %s\n", input);
    }
    file_count = scan_directory(input, files);
  }
  else {
    if (is_allowed_filetype(input)) {
      files[0] = strdup(input);
      file_count = 1;
    }
    else {
      fprintf(stderr, "ERROR: %s is not a media file.\n", input);
      if (username)
        free(username);
      if (password)
        free(password);
      return -1;
    }
  }

  if (file_count == 0) {
    fprintf(stderr, "No media files found.\n");
    if (username)
      free(username);
    if (password)
      free(password);
    return -1;
  }

  if (flag_verbose) {
    printf("Found %d media files.\n", file_count);
  }

  int media_count;
  media_file *mfs = collect_media_info(files, file_count, &media_count,
                                       final_username, final_password);
  if (!mfs || media_count == 0) {
    fprintf(stderr, "Failed to collect media info.\n");
    if (username)
      free(username);
    if (password)
      free(password);
    if (final_username != username && final_username)
      free(final_username);
    if (final_password != password && final_password)
      free(final_password);
    return -1;
  }

  int result = write_m3u(mfs, media_count, output_filename, flag_embed_auth,
                         final_username, final_password);

  if (result == 0 && flag_verbose) {
    printf("Playlist created successfully.\n");
  }

  free_media_files(mfs, media_count);
  for (int i = 0; i < file_count; i++) {
    free(files[i]);
  }
  if (output_filename) {
    free((void *)output_filename);
  }
  if (username)
    free(username);
  if (password)
    free(password);

  if (final_username != username && final_username)
    free(final_username);
  if (final_password != password && final_password)
    free(final_password);

  return result;
}

void print_usage(const char *name) {
  printf("Usage: %s [OPTIONS] <dir|file|url> [output]\n", name);
  printf("Opts:\n");
  printf("  -v, --verbose          Enable verbose output\n");
  //printf("  -8, --utf8             Use UTF-8 encoding (m3u8)\n"); //not implemented
  printf("  -u, --username USER    Username for HTTP authentication\n");
  printf("  -p, --password PASS    Password for HTTP authentication\n");
  printf("  -e, --embed-auth       Embed username/password in playlist URLs\n");
  printf("  -h, --help             Show this help message\n");
}