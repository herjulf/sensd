CFLAGS?=-Wall

# It is safe to link static binaries on most platforms. Recommended.
# But a problem with recent Ubuntu 14.X has been spotted. So for now
# -static commented out.

#LDFLAGS?=-static
sensd:	sensd.o devtag-allinone.o
install:	sensd
	strip sensd
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p sensd $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o sensd
