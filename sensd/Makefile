CFLAGS?=-Wall
LDFLAGS?=-static
sensd:	sensd.o devtag-allinone.o
install:	sensd
	strip sensd
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p sensd $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o sensd
