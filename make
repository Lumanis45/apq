#!/usr/bin/env bash

mkdir -p build
rm -rf build/*

CFLAGS="-O2 -s"

if [ $# -gt 0 ]; then
  CFLAGS="$*"
fi

clang $CFLAGS src/main.c -o build/apq
echo "Build successful with flags: $CFLAGS"
