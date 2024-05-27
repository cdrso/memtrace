# Define common flags
CFLAGS = -g
LDLFLAGS = -ldl

all: main memtrace myalloc.so

main: main.c
	gcc $(CFLAGS) -o $@ $^

memtrace: shmwrap.c hashmap.c memtrace.c
	gcc $(CFLAGS) -o $@ $^ $(LDLFLAGS)

myalloc.so: shmwrap.c hashmap.c memtrace.c myalloc.c
	gcc -DRUNTIME -shared -fpic -o $@ $^ $(LDLFLAGS) $(CFLAGS)

.PHONY: clean

clean:
	rm -f main memtrace myalloc.so
