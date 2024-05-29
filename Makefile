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

all: $(BUILDDIR)/myalloc.so $(BUILDDIR)/memtrace $(BUILDDIR)/main $(BUILDDIR)/ht_test

$(BUILDDIR)/myalloc.so: $(SRCDIR)/shmwrap.c $(SRCDIR)/hashtable.c $(SRCDIR)/memtrace.c $(SRCDIR)/myalloc.c
	gcc -DRUNTIME -shared -fpic -o $@ $^ $(LDLFLAGS) $(CFLAGS)

$(BUILDDIR)/memtrace: $(SRCDIR)/shmwrap.c $(SRCDIR)/hashtable.c $(SRCDIR)/memtrace.c
	gcc $(CFLAGS) -pg -o $@ $^ $(LDLFLAGS)

$(BUILDDIR)/main: $(SRCDIR)/main.c
	gcc $(CFLAGS) -o $@ $^

$(BUILDDIR)/ht_test: $(SRCDIR)/shmwrap.c $(SRCDIR)/ht_test.c $(SRCDIR)/hashtable.c
	gcc $(CFLAGS) -D HT_TEST -o $@ $^

.PHONY: clean

clean:
	rm -r $(BUILDDIR)/main $(BUILDDIR)/memtrace $(BUILDDIR)/myalloc.so $(BUILDDIR)/ht_test

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILDDIR)/myalloc.so $(DESTDIR)$(LIBDIR)
	install -m 755 $(BUILDDIR)/memtrace $(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/myalloc.so
	rm -f $(DESTDIR)$(BINDIR)/memtrace
