CFLAGS=-g -Wall
LDFLAGS=-lcurses
PREFIX?=/usr/local

bike: bike.c

.PHONY: clean install
clean:
	rm -f bike
install:
	install -m 755 -d $(PREFIX)/bin
	install -m 755 bike $(PREFIX)/bin
