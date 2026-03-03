# d2m3u.exe
Windows instructions:

### Development
- Install and set up MSYS2.
- Open the MinGW64 terminal (NOT MSYS2) and install the following:
- `mingw-w64-x86_64-gcc` `mingw-w64-x86_64-curl` `mingw-w64-x86_64-ffmpeg`
- Build inside the MinGW64 terminal with `make`

### Usage
- Run `make install` to install.
- Alternativerly, run `make installer` to just build the installer exe.