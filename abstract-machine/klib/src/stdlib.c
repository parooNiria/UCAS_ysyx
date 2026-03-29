#include <am.h>
#include <klib.h>
#include <klib-macros.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
static unsigned long int next = 1;

int rand(void) {
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed) {
  next = seed;
}

int abs(int x) {
  return (x < 0 ? -x : x);
}

int atoi(const char* nptr) {
  int x = 0;
  while (*nptr == ' ') { nptr ++; }
  while (*nptr >= '0' && *nptr <= '9') {
    x = x * 10 + *nptr - '0';
    nptr ++;
  }
  return x;
}

#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
static void *hbrk = NULL;
#endif

void *malloc(size_t size) {

  // On native, malloc() will be called during initializaion of C runtime.
  // Therefore do not call panic() here, else it will yield a dead recursion:
  //   panic() -> putchar() -> (glibc) -> malloc() -> panic()
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
  if (hbrk == NULL) hbrk = (void *)ROUNDUP(heap.start, 8);
  size = (size_t)ROUNDUP(size, 8);
  void *old = hbrk;
  hbrk = (void *)((uint8_t *)hbrk + size);
  if (hbrk > heap.end) {
    printf("OOM: size=%u, hbrk=%p, heap_end=%p\n", (unsigned int)size, hbrk, heap.end);
    panic("OOM");
  }
  return old;
#else
  return NULL;
#endif
}

void free(void *ptr) {
}

#endif
