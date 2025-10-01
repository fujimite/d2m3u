UNAME_S := $(shell uname 2>/dev/null || echo Windows)

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =

ifeq ($(UNAME_S),Darwin)
  CC = clang
  CFLAGS += -I/opt/homebrew/include
  LDFLAGS += -lavformat -lavutil -lcurl -L/opt/homebrew/lib
  TARGET = d2m3u

else ifeq ($(UNAME_S),Linux)
  CC = gcc
  CFLAGS += -I/usr/include
  LDFLAGS += -lavformat -lavutil -lcurl -L/usr/lib
  TARGET = d2m3u

else ifeq ($(UNAME_S),Windows)
  ifdef MSYSTEM
    ifeq ($(MSYSTEM),MINGW64)
      MSYS2_PREFIX = /mingw64
    else ifeq ($(MSYSTEM),MINGW32)
      MSYS2_PREFIX = /mingw32
    else ifeq ($(MSYSTEM),UCRT64)
      MSYS2_PREFIX = /ucrt64
    else
      MSYS2_PREFIX = /mingw64
    endif
  else
    MSYS2_PREFIX = C:/msys64/mingw64
  endif
  
  CC = gcc
  CFLAGS += -I$(MSYS2_PREFIX)/include
  LDFLAGS += -L$(MSYS2_PREFIX)/lib -lavformat -lavutil -lcurl -lws2_32
  TARGET = d2m3u.exe

else ifneq (,$(findstring MINGW,$(UNAME_S)))
  MSYS2_PREFIX = /mingw64
  CC = gcc
  CFLAGS += -I$(MSYS2_PREFIX)/include
  LDFLAGS += -L$(MSYS2_PREFIX)/lib -lavformat -lavutil -lcurl -lws2_32
  TARGET = d2m3u.exe

else
  $(error $(UNAME_S) is unsupported by this Makefile.)
endif

TARGET = d2m3u
SRC_DIR = src
SOURCES = $(shell find $(SRC_DIR) -type f -name "*.c")
OBJECTS = $(SOURCES:.c=.o)

all: info $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

ifeq ($(UNAME_S),Linux)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)
else ifeq ($(UNAME_S),Darwin)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)
endif

ifeq ($(TARGET),d2m3u.exe)
install: $(TARGET)
	@echo "On Windows, please copy $(TARGET) to a directory in your PATH"
	@echo "For example: copy $(TARGET) to C:\Windows\System32 or C:\Users\%USERNAME%\bin"

uninstall:
	@echo "On Windows, please manually delete $(TARGET) from where you installed it"
endif

debug: CFLAGS += -g -DDEBUG
debug: clean info $(TARGET)

verbose: CFLAGS += -v
verbose: clean info $(TARGET)

info:
	@echo "OS:       $(UNAME_S)"
	@echo "CC:       $(CC)"
	@echo "CFLAGS:   $(CFLAGS)"
	@echo "LDFLAGS:  $(LDFLAGS)"
	@echo "SOURCES:  $(SOURCES)"
	@echo "OBJECTS:  $(OBJECTS)"
	@echo "TARGET:   $(TARGET)"

.PHONY: all clean install uninstall debug verbose info
