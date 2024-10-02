#include <ctype.h>
#include <gc/gc.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

str_t str_empty() { return STR(""); }

str_t str_make(u8 *data, usize len) {
  return (str_t){.data = data, .len = len};
}

str_t str_from(const u8 *data, usize len) {
  u8 *buf = gcalloc_atomic(len);
  memcpy(buf, data, len);
  return str_make(buf, len);
}

str_t str_new(const char *s) {
  usize n = strlen(s);
  return str_from((const u8 *)s, n);
}

bytes_t bytes_new() { return (bytes_t){.data = NULL, .len = 0, .cap = 0}; }

void bytes_push(bytes_t *b, u8 value) {
  if (b->len >= b->cap) {
    bytes_reserve(b, (b->cap != 0) ? (b->cap) : 4);
  }
  b->data[b->len] = value;
  b->len += 1;
}

void bytes_reserve(bytes_t *b, usize additional_capacity) {
  if (b->len > UINTPTR_MAX - additional_capacity) {
    panic("capacity overflow");
  }
  usize new_cap = b->len + additional_capacity;
  if (new_cap <= b->cap) {
    return;
  }
  u8 *new_alloc = gcalloc_atomic(new_cap);
  if (b->len != 0) {
    memcpy(new_alloc, b->data, b->len);
  }
  b->data = new_alloc;
  b->cap = new_cap;
}

void bytes_shrink(bytes_t *b) {
  if (b->len == 0) {
    *b = bytes_new();
    return;
  }
  u8 *shrinked = gcrealloc_atomic(b->data, b->len);
  b->data = shrinked;
  b->cap = b->len;
}

void bytes_clear(bytes_t *b) { b->len = 0; }

void bytes_clear_start(bytes_t *b, usize len) {
  if (len == 0)
    return;
  if (b->len > len) {
    b->len -= len;
    memmove(b->data, b->data + len, b->len);
  } else {
    bytes_clear(b);
  }
}

usize bytes_fread(bytes_t *b, FILE *f) {
  usize res = fread(b->data + b->len, 1, b->cap - b->len, f);
  if (ferror(f)) {
    panic("error while reading file");
  }
  b->len += res;
  return res;
}

void fdebug_str(FILE *f, const u8 *data, usize len) {
  fputc('"', f);
  for (usize i = 0; i < len; ++i) {
    switch (data[i]) {
    case '\\':
      fputs("\\\\", f);
      break;
    case '"':
      fputs("\\\"", f);
      break;
    case '\0':
      fputs("\\0", f);
      break;
    case '\n':
      fputs("\\n", f);
      break;
    case '\t':
      fputs("\\t", f);
      break;
    case '\r':
      fputs("\\r", f);
      break;
    default:
      if (isprint(data[i])) {
        fputc(data[i], f);
      } else {
        fprintf(f, "\\x%02x", data[i]);
      }
    }
  }
  fputc('"', f);
}

void fdebug_str_color(FILE *f, const u8 *data, usize len) {
  fputs("\x1b[32m\"", f);
  for (usize i = 0; i < len; ++i) {
    switch (data[i]) {
    case '\\':
      fputs("\x1b[34m\\\\\x1b[32m", f);
      break;
    case '"':
      fputs("\x1b[34m\\\"\x1b[32m", f);
      break;
    case '\0':
      fputs("\x1b[34m\\0\x1b[32m", f);
      break;
    case '\n':
      fputs("\x1b[34m\\n\x1b[32m", f);
      break;
    case '\t':
      fputs("\x1b[34m\\t\x1b[32m", f);
      break;
    case '\r':
      fputs("\x1b[34m\\r\x1b[32m", f);
      break;
    default:
      if (data[i] < 128 && isprint(data[i])) {
        fputc(data[i], f);
      } else {
        fprintf(f, "\x1b[34m\\x%02x\x1b[32m", data[i]);
      }
    }
  }
  fputs("\"\x1b[39m", f);
}

bool str_comp(str_t a, str_t b) {
  if (a.len == 0 && b.len == 0) {
    return true;
  }
  if (a.len != b.len) {
    return false;
  }
  if (memcmp(a.data, b.data, a.len) == 0) {
    return true;
  }
  return false;
}

void print_memory_use() {
  usize used = GC_get_memory_use();

  eprint("memory usage: ");
  if (used < 1024) {
    eprintln("%luB", used);
  } else if (used < (1024 * 1024)) {
    eprintln("%lukiB", used / 1024);
  } else if (used < (1024 * 1024 * 1024)) {
    eprintln("%luMiB", used / (1024 * 1024));
  } else {
    eprintln("%luGiB", used / (1024 * 1024 * 1024));
  }
}

void *gcalloc(usize size) {
  if (size == 0) {
    return NULL;
  }
  void *res = GC_malloc(size);
  if (res == NULL) {
    panic("allocation of size %lu failed", size);
  }
  return res;
}

void *gcalloc_atomic(usize size) {
  if (size == 0) {
    return NULL;
  }
  void *res = GC_malloc_atomic(size);
  if (res == NULL) {
    panic("atomic allocation of size %lu failed", size);
  }
  return res;
}

void *gcrealloc(void *old, usize size) {
  if (size == 0) {
    return NULL;
  }
  void *res = GC_realloc(old, size);
  if (res == NULL) {
    panic("reallocation of size %lu failed", size);
  }
  return res;
}

void *gcrealloc_atomic(void *old, usize size) {
  if (size == 0) {
    return NULL;
  }
  void *res;
  if (old == NULL) {
    res = GC_malloc_atomic(size);
  } else {
    res = GC_realloc(old, size);
  }
  if (res == NULL) {
    panic("atomic reallocation of size %lu failed", size);
  }
  return res;
}
