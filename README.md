# APQ - AUR package query

#### Synopsis:
- It's a lightweight [aur](https://aur.archlinux.org/) helper.
- Writen in C, and based on KISS(Keep It Simple Stupid) philosophy.
- Codebase main.c - 79 lines of code.

#### Usage:
- `apq --ask <package-name>`

#### Build:
- With optimizations:
  - `./make -O[1|2|3] -march=native`
- Without:
  - `./make`
