# Define common flags
CFLAGS = -g -I incl/
SRCDIR = src
BUILDDIR = build
LDLFLAGS = -ldl

# Installation directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib

DESTDIR =

all: $(BUILDDIR)/main $(BUILDDIR)/memtrace $(BUILDDIR)/myalloc.so

$(BUILDDIR)/main: $(SRCDIR)/main.c
	gcc $(CFLAGS) -o $@ $^

$(BUILDDIR)/memtrace: $(SRCDIR)/shmwrap.c $(SRCDIR)/hashmap.c $(SRCDIR)/memtrace.c
	gcc $(CFLAGS) -o $@ $^ $(LDLFLAGS)

$(BUILDDIR)/myalloc.so: $(SRCDIR)/shmwrap.c $(SRCDIR)/hashmap.c $(SRCDIR)/memtrace.c $(SRCDIR)/myalloc.c
	gcc -DRUNTIME -shared -fpic -o $@ $^ $(LDLFLAGS) $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(BUILDDIR)/main $(BUILDDIR)/memtrace $(BUILDDIR)/myalloc.so

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(LIBDIR)
	#install -m 755 $(BUILDDIR)/main $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILDDIR)/memtrace $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILDDIR)/myalloc.so $(DESTDIR)$(LIBDIR)

uninstall:
	#rm -f $(DESTDIR)$(BINDIR)/main
	rm -f $(DESTDIR)$(BINDIR)/memtrace
	rm -f $(DESTDIR)$(LIBDIR)/myalloc.so
