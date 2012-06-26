CFLAGS?=-Wall
LDFLAGS?=-static
all:	seltag
install:	seltag
	strip seltag
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p seltag $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o seltag
