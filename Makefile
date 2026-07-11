UNAME_S := $(shell uname 2>/dev/null || echo Windows)
VERSION ?= 0.0
TARGET = d2m3u
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
    else
      MSYS2_PREFIX = /mingw64
    endif
  else
    MSYS2_PREFIX = C:/msys64/mingw64
  endif
  CC = gcc
  CFLAGS += -I$(MSYS2_PREFIX)/include
  LDFLAGS += -L$(MSYS2_PREFIX)/lib -lavformat -lavutil -lcurl -lws2_32 -Wl,--as-needed
  TARGET = d2m3u.exe
  DIST = dist/
else ifneq (,$(findstring MINGW,$(UNAME_S)))
  MSYS2_PREFIX = /mingw64
  CC = gcc
  CFLAGS += -I$(MSYS2_PREFIX)/include
  LDFLAGS += -L$(MSYS2_PREFIX)/lib -lavformat -lavutil -lcurl -lws2_32 -Wl,--as-needed
  TARGET = d2m3u.exe
  DIST = dist/
else
  $(error $(UNAME_S) is unsupported by this Makefile.)
endif
SRC_DIR = src
SOURCES = $(shell find $(SRC_DIR) -type f -name "*.c")
OBJECTS = $(SOURCES:.c=.o)
all: info $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
ifeq ($(TARGET),d2m3u.exe)
clean:
	rm -rf $(OBJECTS) $(TARGET) $(DIST) d2m3u-setup*.exe
else
clean:
	rm -rf $(OBJECTS) $(TARGET)
endif
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
install: installer
	start d2m3u-setup.exe
uninstall:
	@echo "On Windows, you will need to go to programs & features, then run the uninstaller."
dist: $(TARGET)
	mkdir -p dist/licenses
	cp $(TARGET) dist/
	ldd $(TARGET) | grep '$(MSYS2_PREFIX)' | awk '{print $$3}' | xargs -I{} cp {} dist/
	cp licenses-win/* dist/licenses/
installer: dist
	makensis -DAPP_VERSION=$(VERSION) installer.nsi
	rm -rf dist
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
.PHONY: all clean install uninstall debug verbose info installer
