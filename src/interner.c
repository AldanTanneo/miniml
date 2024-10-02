#include <gc/gc.h>
#include <pthread.h>
#include <string.h>
#include <threads.h>

#include "hashmap.h"
#include "interner.h"
#include "utils.h"

static once_flag PRIVATE_INTERNER_INIT = ONCE_FLAG_INIT;
static mtx_t PRIVATE_INTERNER_LOCK = {0};
static hashset_t PRIVATE_INTERNER = NULL;

static void register_interner() {
  hashset_t interner = hashset_new();
  PRIVATE_INTERNER = GC_malloc_uncollectable(hashset_sizeof());
  if (PRIVATE_INTERNER != NULL) {
    memcpy(PRIVATE_INTERNER, interner, hashset_sizeof());
    mtx_init(&PRIVATE_INTERNER_LOCK, mtx_plain);
  } else {
    panic("interner allocation failed");
  }
}

static inline hashset_t lock() {
  call_once(&PRIVATE_INTERNER_INIT, register_interner);
  mtx_lock(&PRIVATE_INTERNER_LOCK);
  return PRIVATE_INTERNER;
}

static inline void unlock(hashset_t _) { mtx_unlock(&PRIVATE_INTERNER_LOCK); }

str_t intern(str_t str) {
  hashset_t interner = lock();

  str_t res;
  if (hashset_contains(interner, str)) {
    res = hashset_get(interner, str);
  } else {
    hashset_insert(interner, str);
    res = str;
  }

  unlock(interner);
  return res;
}

usize interned_strings() {
  hashset_t interner = lock();
  usize res = hashset_len(interner);
  unlock(interner);
  return res;
}

static void print_str(usize remaining, str_t val) {
  fdebug_str(stderr, val.data, val.len);
  if (remaining > 0) {
    eprint(", ");
  }
}

void interner_debug() {
  eprint("{");
  hashset_t interner = lock();
  hashset_iter(interner, print_str);
  unlock(interner);
  eprint("}");
}
