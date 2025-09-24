# d2m3u
d2m3u is a command line tool to generate m3u playlists from local or web directories.

Requirements:
- `ffmpeg` and `curl`
Usage:
- Clone source and run `make install` or place prebuilt binary in your path
- Run `d2m3u -h`

Development:
- Install the following:
  - Ubuntu/Debian: `libavformat-dev` `libavcodec-dev` `libavutil-dev` `libcurl4-openssl-dev`
  - Fedora: `ffmpeg-devel` `libcurl-devel`
  - macOS: `ffmpeg` `curl`
- Build with `make` or `build.sh`
