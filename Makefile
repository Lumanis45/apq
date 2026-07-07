
CC=clang
CFLAGS=-O2 -s

.PHONY: install uninstall reinstall

NAME=apq
TARGET=build/$(NAME)
SRC=src/main.c
PREFIX ?= /usr/local
BINDIR=$(PREFIX)/bin

all:
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
	@echo "Build successful!"

install: all
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(NAME)

uninstall:
	@rm -rf $(DESTDIR)$(BINDIR)/$(NAME) 

reinstall: uninstall install
