#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep (needed for panic macro)

// utilities

#define loop while (1)
#define println(lit, args...) printf(lit "\n", ##args)
#define print(lit, args...) printf(lit, ##args)
#define eprintln(lit, args...) fprintf(stderr, lit "\n", ##args)
#define eprint(lit, args...) fprintf(stderr, lit, ##args)
#define panic(lit, args...)                                                    \
  {                                                                            \
    eprintln("\x1b[31;1m[%s:%d] panic:\x1b[0m " lit, __FILE__, __LINE__,       \
             ##args);                                                          \
    abort();                                                                   \
    loop {}                                                                    \
  }

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
// typedef __int128_t i128;
typedef intptr_t isize;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
// typedef __uint128_t u128;
typedef uintptr_t usize;

typedef float f32;
typedef double f64;
typedef long double f128;

// growable bytes array
typedef struct bytes {
  u8 *data;
  usize len;
  usize cap;
} bytes_t;

// creates a new GC-allocated growable bytes array
bytes_t bytes_new();
// push a single byte
void bytes_push(bytes_t *b, u8 value);
// reserve additional capacity in the vector
void bytes_reserve(bytes_t *b, usize additional_capacity);
// shrink the capacity to fit the length
void bytes_shrink(bytes_t *b);
// discard all the contents without changing the capacity
void bytes_clear(bytes_t *b);
// discard the contents in the range (0..len), moving the end of the vector
// to its start
void bytes_clear_start(bytes_t *b, usize len);
// read the contents of a file into a vector in the available capacity
usize bytes_fread(bytes_t *b, FILE *f);

// explicit length string
typedef struct {
  usize len;
  u8 *data;
} str_t;

// define a new str from a string literal
#define STR(lit) ((str_t){.data = (u8 *)lit, .len = sizeof(lit) - 1})

str_t str_empty();
// make a new string by taking ownership of a GC allocated buffer
str_t str_make(u8 *data, usize len);
// copy a buffer into a new GC allocated string
str_t str_from(const u8 *data, usize len);
// create a new GC-allocated str from a C-style string
str_t str_new(const char *s);
// print a debug view of a string
void fdebug_str(FILE *f, const u8 *data, usize len);
void fdebug_str_color(FILE *f, const u8 *data, usize len);
// compare two strings for equality
bool str_comp(str_t a, str_t b);

void print_memory_use();

void *gcalloc(usize size);
void *gcalloc_atomic(usize size);
void *gcrealloc(void *old, usize size);
void *gcrealloc_atomic(void *old, usize size);
