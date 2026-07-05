#!/usr/bin/env bash

build() {
  if [ ! -d ./build ]; then
    mkdir -p build;
  fi
  rm -rf build/*;
  
  clang -o build/apq src/main.c;
  echo "Build successfully!";
}
build;
