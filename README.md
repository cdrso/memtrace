# Memtrace - A Simple Memory Profiler for Linux x86-64

## Overview

**Memtrace** is a lightweight memory profiling tool designed for Linux x86-64 platforms. It monitors and traces memory allocations and deallocations in real-time by employing runtime interposition techniques to override the standard C library's dynamic memory functions (`malloc`, `calloc`, `realloc`, `free`). This interception allows **Memtrace** to provide insights into memory usage patterns and potential leaks, primarily focusing on single-threaded applications due to its current implementation limitations.

### Key Features

- **Real-Time Monitoring**: Offers immediate feedback on memory allocations and deallocations as they occur.
- **Minimal Overhead**: Introduces minimal performance overhead, making it suitable for development and debugging environments.
- **Interception of Standard C Library Functions**: Specifically targets the standard C library's memory management functions, providing accurate tracking of memory operations.

## Limitations

- **Single-Threaded Focus**: Mainly supports single-threaded applications. Testing has shown that pthread may use custom memory management functions, potentially leading to false leaks when attempting to profile multi-threaded applications.
- **Scope Limited to Standard C Library**: Only monitors the standard C library's memory management functions. Custom memory allocators or extensions (e.g., jemalloc, tcmalloc) will not be tracked.
- **Excludes Statically Linked Executables**: Does not affect statically linked executables or functions provided by the application itself, as these bypass the dynamic linker/loader.
- **Platform Specificity**: Designed for Linux x86 platforms.

## Getting Started

To use **Memtrace**, follow these steps:

1. Compile your application normally.
2. Run the shell command **memtrace** ./<executable>


## License

**Memtrace** is open-source software licensed under the MIT License. See the LICENSE file for details.
