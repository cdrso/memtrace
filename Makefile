# Define common flags
CFLAGS = -g -I incl/
SRCDIR = src
BUILDDIR = build
LDLFLAGS = -ldl

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
