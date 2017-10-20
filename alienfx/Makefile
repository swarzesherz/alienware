CC=gcc
CFLAGS=-Wall
LIBS=-lusb-1.0
PREFIX=/usr/local

%o: %c
	$(CC) -c -o $@ $< $(CFLAGS)

alienfx: alienfx.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f alienfx alienfx.o

install: alienfx
	install -m 0755 alienfx $(PREFIX)/bin
