CFLAGS=-g -Wall -DBUILD_ID=\"`git rev-parse HEAD`\"
LDFLAGS=-lcurses
PREFIX?=/usr/local

bike: bike.c
	cc ${CFLAGS} $< -o $@ ${LDFLAGS}

.PHONY: clean install
clean:
	rm -f bike
install:
	install -m 755 -d $(PREFIX)/bin
	install -m 755 bike $(PREFIX)/bin
