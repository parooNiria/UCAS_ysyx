#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

typedef struct {
  char *out;
  size_t size;
  size_t pos;
  void (*emit)(char ch, void *ctx);
  void *ctx;
} outbuf_t;

static void emit_to_console(char ch, void *ctx) {
  (void)ctx;
  putch(ch);
}

static void emit_to_buffer(char ch, void *ctx) {
  outbuf_t *b = (outbuf_t *)ctx;
  if (b->size == 0 || b->out == NULL) {
    b->pos++;
    return;
  }
  if (b->pos < b->size - 1) {
    b->out[b->pos] = ch;
  }
  b->pos++;
}

static int kvprintf(void (*emit)(char, void *), void *ctx, const char *fmt, va_list ap) {
  int cnt = 0;
  while (*fmt) {
    if (*fmt != '%') {
      emit(*fmt++, ctx);
      cnt++;
      continue;
    }
    fmt++; // skip '%'
    
    // Check flags
    int pad_with_zero = 0;
    if (*fmt == '0') {
      pad_with_zero = 1;
      fmt++;
    }
    
    // Check width
    int width = 0;
    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt++;
    }
    
    // Check length modifiers
    int is_long = 0;
    if (*fmt == 'l') {
      is_long = 1;
      fmt++;
      if (*fmt == 'l') {
        is_long = 2; // long long, treated same as long for 32-bit usually but let's just record it
        fmt++;
      }
    } else if (*fmt == 'z') {
      is_long = 1; // size_t
      fmt++;
    }

    if (*fmt == '\0') break;
    
    char tmp[64];
    int len = 0;
    
    if (*fmt == 'c') {
      char c = (char)va_arg(ap, int);
      emit(c, ctx);
      cnt++;
    } else if (*fmt == 's') {
      const char *s = va_arg(ap, const char *);
      if (!s) s = "(null)";
      while (*s) {
        emit(*s++, ctx);
        cnt++;
      }
    } else if (*fmt == 'd' || *fmt == 'i') {
      long val = is_long ? va_arg(ap, long) : va_arg(ap, int);
      int is_neg = 0;
      unsigned long uval = val;
      if (val < 0) {
        is_neg = 1;
        uval = (unsigned long)(-val);
      }
      do {
        tmp[len++] = '0' + (uval % 10);
        uval /= 10;
      } while (uval > 0);
      if (is_neg) tmp[len++] = '-';
      
      int pads = width - len;
      while (pads-- > 0) {
        emit(pad_with_zero ? '0' : ' ', ctx);
        cnt++;
      }
      while (len > 0) {
        emit(tmp[--len], ctx);
        cnt++;
      }
    } else if (*fmt == 'u') {
      unsigned long uval = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
      do {
        tmp[len++] = '0' + (uval % 10);
        uval /= 10;
      } while (uval > 0);
      int pads = width - len;
      while (pads-- > 0) {
        emit(pad_with_zero ? '0' : ' ', ctx);
        cnt++;
      }
      while (len > 0) {
        emit(tmp[--len], ctx);
        cnt++;
      }
    } else if (*fmt == 'x' || *fmt == 'X' || *fmt == 'p') {
      unsigned long uval;
      if (*fmt == 'p') {
        uval = (unsigned long)va_arg(ap, void *);
      } else {
        uval = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
      }
      char hex_base = (*fmt == 'X') ? 'A' : 'a';
      do {
        int rem = uval % 16;
        tmp[len++] = (rem < 10) ? ('0' + rem) : (hex_base + rem - 10);
        uval /= 16;
      } while (uval > 0);
      if (*fmt == 'p') {
        pad_with_zero = 1;
        // width = sizeof(void*) * 2 if we want to pad ptrs, or just leave it
      }
      int pads = width - len;
      while (pads-- > 0) {
        emit(pad_with_zero ? '0' : ' ', ctx);
        cnt++;
      }
      while (len > 0) {
        emit(tmp[--len], ctx);
        cnt++;
      }
    } else if (*fmt == '%') {
      emit('%', ctx);
      cnt++;
    } else {
      emit('%', ctx);
      emit(*fmt, ctx);
      cnt += 2;
    }
    fmt++;
  }
  return cnt;
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = kvprintf(emit_to_console, NULL, fmt, ap);
  va_end(ap);
  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  outbuf_t b = { .out = out, .size = (size_t)-1, .pos = 0 };
  int ret = kvprintf(emit_to_buffer, &b, fmt, ap);
  out[b.pos] = '\0';
  return ret;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  outbuf_t b = { .out = out, .size = n, .pos = 0 };
  int ret = kvprintf(emit_to_buffer, &b, fmt, ap);
  if (n > 0) {
    size_t end = (b.pos < n - 1) ? b.pos : (n - 1);
    out[end] = '\0';
  }
  return ret;
}

#endif
