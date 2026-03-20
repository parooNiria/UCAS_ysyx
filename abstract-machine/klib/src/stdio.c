#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

typedef struct {
  char *out;
  size_t size;
  size_t pos;
} outbuf_t;

static int append_dec(char *out, int value) {
  char tmp[16];
  int n = 0;
  unsigned int u = (unsigned int)value;

  if (value < 0) {
    u = (unsigned int)(-u);
    *out++ = '-';
    n++;
  }

  int k = 0;
  do {
    tmp[k++] = (char)('0' + (u % 10));
    u /= 10;
  } while (u != 0);

  while (k > 0) {
    *out++ = tmp[--k];
    n++;
  }
  return n;
}

static void emit_to_console(char ch, void *ctx) {
  (void)ctx;
  putch(ch);
}

static void emit_to_buffer(char ch, void *ctx) {
  outbuf_t *b = (outbuf_t *)ctx;
  if (b->size == 0) {
    b->pos++;
    return;
  }

  if (b->pos + 1 < b->size) {
    b->out[b->pos] = ch;
  }
  b->pos++;
}

static int kvprintf(void (*emit)(char, void *), void *ctx, const char *fmt, va_list ap) {
  int cnt = 0;

  while (*fmt != '\0') {
    if (*fmt != '%') {
      emit(*fmt++, ctx);
      cnt++;
      continue;
    }

    fmt++; // skip '%'
    if (*fmt == '\0') break;

    if (*fmt == 's') {
      const char *s = va_arg(ap, const char *);
      if (s == NULL) s = "(null)";
      while (*s) {
        emit(*s++, ctx);
        cnt++;
      }
    } else if (*fmt == 'd') {
      char tmp[16];
      int v = va_arg(ap, int);
      int n = append_dec(tmp, v);
      for (int i = 0; i < n; i++) {
        emit(tmp[i], ctx);
      }
      cnt += n;
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
  int n = kvprintf(emit_to_console, NULL, fmt, ap);
  va_end(ap);
  return n;
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
  int n = vsprintf(out, fmt, ap);
  va_end(ap);
  return n;
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
