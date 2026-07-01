CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lncursesw
PREFIX = /usr/local

matrix: src/main.c
	$(CC) $(CFLAGS) -o $@ src/main.c $(LDFLAGS)

install: matrix
	install -m 755 matrix $(PREFIX)/bin/matrix

uninstall:
	rm -f $(PREFIX)/bin/matrix

clean:
	rm -f matrix

.PHONY: install uninstall clean
