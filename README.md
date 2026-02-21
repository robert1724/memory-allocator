# memory-allocator
Custom malloc, free, calloc and realloc implementations using sbrk and mmap syscalls, with best-fit allocation, block splitting, block coalescing and heap preallocation

Overview
  - This project implements a memory allocator that manages heap memory through a doubly-linked list of memory blocks. It mimics the behavior of the standard C library allocator with manual control over memory allocation strategies.

Features
  - Dual allocation strategy
     - sbrk for small allocations (< 128 KB)
     - mmap for large allocations (≥ 128 KB)
  - Block splitting: splits free blocks when the remaining space is ≥ 40 bytes, reducing internal fragmentation
  - Block coalescing: merges adjacent free blocks to reduce external fragmentation
  - Best-fit allocation: searches for the smallest free block that satisfies the request
  - Heap preallocation: preallocates 128 KB on the first small allocation to minimize `sbrk` syscalls
  - 8-byte alignment: all allocations are padded to 8-byte boundaries

Function ~ Description
  - void *os_malloc(size_t size) ~ Allocates "size" bytes of memory
  - void os_free(void *ptr) ~ Frees a previously allocated block
  - void *os_calloc(size_t nmemb, size_t size) ~ Allocates and zero-initializes memory for an array
  - void *os_realloc(void *ptr, size_t size) ~ Resizes a previously allocated block
