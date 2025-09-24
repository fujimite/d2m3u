# Allow overriding the OS detection
TARGET_OS ?= $(shell uname)

ifeq ($(TARGET_OS),Darwin)
  ifeq ($(shell command -v clang >/dev/null 2>&1 && echo yes),yes)
    CC = clang
  else ifeq ($(shell command -v gcc >/dev/null 2>&1 && echo yes),yes)
    CC = gcc
  else
    $(error No suitable compiler (clang or gcc) found on macOS)
  endif
  CFLAGS = -Wall -Wextra -O2 -I/opt/homebrew/include
  LDFLAGS = -lavformat -lavutil -lavcodec -lcurl -lm -L/opt/homebrew/lib

else ifeq ($(TARGET_OS),Linux)
  ifeq ($(shell command -v gcc >/dev/null 2>&1 && echo yes),yes)
    CC = gcc
  else ifeq ($(shell command -v clang >/dev/null 2>&1 && echo yes),yes)
    CC = clang
  else
    $(error No suitable compiler (gcc or clang) found on Linux)
  endif
  CFLAGS = -Wall -Wextra -O2 -I/usr/include
  LDFLAGS = -lavformat -lavutil -lavcodec -lcurl -lm -L/usr/lib

else ifeq ($(TARGET_OS),Windows)
  ifeq ($(shell command -v gcc >/dev/null 2>&1 && echo yes),yes)
    CC = x86_64-w64-mingw32-gcc
  else ifeq ($(shell command -v clang >/dev/null 2>&1 && echo yes),yes)
    CC = clang
  else
    $(error No suitable compiler (gcc/MinGW or clang) found on Windows)
  endif
  CFLAGS = -Wall -Wextra -O2 -I/mingw64/include
  LDFLAGS = -lavformat -lavutil -lavcodec -lcurl -lm -L/mingw64/lib

else
  $(error $(TARGET_OS) is unsupported by this Makefile.)
endif
