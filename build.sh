#!/bin/bash
# flags: -v for gcc verbose, -g for debug symbols

SRC_FILES=$(find src -type f -name "*.c")
GCC_OPTIONS="-o d2m3u $SRC_FILES -lavformat -lavutil -lm -lcurl"

while [[ $# -gt 0 ]]; do
  arg="$1"
  case "$arg" in
    -v|--verbose)
      GCC_OPTIONS="-v $GCC_OPTIONS"
      ;;
    -g|--debug)
      GCC_OPTIONS="-g $GCC_OPTIONS"
      ;;
    -[a-zA-Z]*)
      for (( i=1; i<${#arg}; i++ )); do
        ch="${arg:$i:1}"
        case "$ch" in
          v)
            GCC_OPTIONS="-v $GCC_OPTIONS"
            ;;
          g)
            GCC_OPTIONS="-g $GCC_OPTIONS"
            ;;
          *)
            echo "Unknown option: -$ch"
            exit 1
            ;;
        esac
      done
      ;;
    *)
      echo "Unknown argument: $arg"
      exit 1
      ;;
  esac
  shift
done

OS=$(uname)
echo "Building for $OS..."

if [ "$OS" == "Darwin" ]; then
  gcc $GCC_OPTIONS -I/opt/homebrew/include -L/opt/homebrew/lib
elif [ "$OS" == "Linux" ]; then
  gcc $GCC_OPTIONS -I/usr/include -L/usr/lib
else
  echo "$OS is unsupported by this script."
  exit 1
fi

echo "Done."
