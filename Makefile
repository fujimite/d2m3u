UNAME_S := $(shell uname)

ifeq ($(UNAME_S),Darwin)
  ifeq ($(shell command -v clang >/dev/null 2>&1 && echo yes),yes)
    CC = clang
  else ifeq ($(shell command -v gcc >/dev/null 2>&1 && echo yes),yes)
    CC = gcc
  else
    $(error No suitable compiler (clang or gcc) found on macOS)
  endif
  CFLAGS = -Wall -Wextra -O2 -I/opt/homebrew/include
  LDFLAGS = -lavformat -lavutil -lavcodec -lcurl -lm -L/opt/homebrew/lib

else ifeq ($(UNAME_S),Linux)
  ifeq ($(shell command -v gcc >/dev/null 2>&1 && echo yes),yes)
    CC = gcc
  else ifeq ($(shell command -v clang >/dev/null 2>&1 && echo yes),yes)
    CC = clang
  else
    $(error No suitable compiler (gcc or clang) found on Linux)
  endif
  CFLAGS = -Wall -Wextra -O2 -I/usr/include
  LDFLAGS = -lavformat -lavutil -lavcodec -lcurl -lm -L/usr/lib

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

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

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
