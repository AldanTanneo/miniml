#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"
#include "utils.h"

typedef struct keyval {
  u64 hash;
  str_t key;
  struct keyval *next;
} keyval_t;

struct hashmap {
  u8 cap_log;
  usize cap_mask;
  usize len;
  usize entry_size;
  keyval_t *buf;
};

struct hashset {
  struct hashmap map;
};

#define CAST_SET(set) &(set)->map

static const u8 INIT_CAPACITY_LOG = 2;
static const u8 CAPACITY_LOG_MAX = 32;

static const u64 FNV_BASIS = 0xcbf29ce484222325;
static const u64 FNV_PRIME = 0x100000001b3;

// fnv hash
static inline u64 hasher(str_t key) {
  u64 res = FNV_BASIS;
  for (usize i = 0; i < key.len; ++i) {
    res ^= (u64)(key.data[i]);
    res *= FNV_PRIME;
  }

  // bias the hash so that no valid key has hash 0
  // this way, hash == 0 can be used to detect empty nodes
  if (res == 0) {
    return 1ul << 63;
  } else {
    return res;
  }
}

static inline usize hashmap_getcap(hashmap_t map) { return map->cap_mask + 1; }

static inline usize buf_size(usize entry_size, usize cap) {
  return cap * entry_size;
}

static inline keyval_t *buf_byte_get(keyval_t *buf, usize index) {
  u8 *b = (u8 *)buf;
  return (keyval_t *)(&b[index]);
}

static inline keyval_t *buf_alloc(usize entry_size, usize cap) {
  usize bsize = buf_size(entry_size, cap);
  keyval_t *buf = gcalloc(bsize);
  for (usize i = 0; i < bsize; i += entry_size) {
    keyval_t *slot = buf_byte_get(buf, i);
    slot->hash = 0;
    slot->next = NULL;
  }
  return buf;
}

static inline void *keyval_elt_get(keyval_t *kv) { return (void *)(kv + 1); }

static bool keyval_set(keyval_t *kv, u64 hash, str_t key, void *value,
                       usize elt_size) {
  bool res = true;

  if (kv->hash != 0) {
    if (kv->hash == hash && str_comp(key, kv->key)) {
      // overwrite (keep same kv)
      res = false;
    } else {
      // put new node in second position
      keyval_t *new_kv = gcalloc(sizeof(keyval_t) + elt_size);
      new_kv->next = kv->next;
      kv->next = new_kv;
      new_kv->hash = hash;
      kv = new_kv;
    }
  }

  kv->hash = hash;
  kv->key = key;
  if (elt_size != 0 && value != NULL) {
    memmove(keyval_elt_get(kv), value, elt_size);
  }
  return res;
}

static void hashmap_resize(hashmap_t map) {
  if (map->cap_log >= CAPACITY_LOG_MAX) {
    panic("maximum capacity reached");
  }

  usize entry_size = map->entry_size;
  usize elt_size = entry_size - sizeof(keyval_t);
  usize cap = hashmap_getcap(map);
  usize bcap = buf_size(entry_size, cap);

  usize new_cap = cap * 2;
  usize new_cap_mask = new_cap - 1;
  keyval_t *new_buf = buf_alloc(map->entry_size, new_cap);

  for (usize i = 0; i < bcap; i += entry_size) {
    keyval_t *kv = buf_byte_get(map->buf, i);
    if (kv->hash == 0) {
      continue;
    }
    for (; kv != NULL; kv = kv->next) {
      usize kv_index = (kv->hash & new_cap_mask) * entry_size;
      keyval_t *new_kv = buf_byte_get(new_buf, kv_index);
      keyval_set(new_kv, kv->hash, kv->key, keyval_elt_get(kv), elt_size);
    }
  }

  map->cap_log += 1;
  map->cap_mask = new_cap_mask;
  map->buf = new_buf;
}

static inline keyval_t *hashmap_keyval_get(hashmap_t map, str_t key) {
  u64 hash = hasher(key);
  usize index = hash & map->cap_mask;
  keyval_t *kv = buf_byte_get(map->buf, index * map->entry_size);

  loop {
    if (kv->hash == hash && str_comp(key, kv->key)) {
      return kv;
    }
    if (kv->next == NULL) {
      return NULL;
    }
    kv = kv->next;
  }
}

hashmap_t hashmap_new(usize elt_size) {
  usize entry_size = sizeof(keyval_t) + ((elt_size + 7) / 8) * 8;
  usize cap = 1ul << INIT_CAPACITY_LOG;

  keyval_t *buf = buf_alloc(entry_size, cap);
  hashmap_t res = gcalloc(sizeof(struct hashmap));

  res->cap_log = INIT_CAPACITY_LOG;
  res->cap_mask = cap - 1;
  res->len = 0;
  res->entry_size = sizeof(keyval_t) + ((elt_size + 7) / 8) * 8;
  res->buf = buf;
  return res;
}

void *hashmap_get(hashmap_t map, str_t key) {
  keyval_t *kv = hashmap_keyval_get(map, key);
  if (kv == NULL) {
    return NULL;
  } else {
    return keyval_elt_get(kv);
  }
}

bool hashmap_contains(hashmap_t map, str_t key) {
  return hashmap_keyval_get(map, key) != NULL;
}

bool hashmap_insert(hashmap_t map, str_t key, void *value) {
  if (4 * map->len >= 3 * (map->cap_mask + 1)) {
    hashmap_resize(map);
  }

  u64 hash = hasher(key);
  usize index = (hash & map->cap_mask);

  keyval_t *kv = buf_byte_get(map->buf, index * map->entry_size);
  usize elt_size = map->entry_size - sizeof(keyval_t);
  bool res = keyval_set(kv, hash, key, value, elt_size);

  map->len += 1;
  return res;
}

bool hashmap_remove(hashmap_t map, str_t key) {
  u64 hash = hasher(key);
  usize index = (hash & map->cap_mask);
  keyval_t *kv = buf_byte_get(map->buf, index * map->entry_size);
  keyval_t *prev = NULL;

  loop {
    if (kv->hash == hash && str_comp(key, kv->key)) {
      if (prev == NULL) {
        if (kv->next != NULL) {
          keyval_t *next = kv->next;
          memmove(kv, next, map->entry_size);
        } else {
          kv->hash = 0;
          kv->next = NULL;
        }
      } else {
        prev->next = kv->next;
      }
      return true;
    }
    if (kv->next == NULL) {
      return false;
    }
    prev = kv;
    kv = kv->next;
  }
}

hashset_t hashset_new() { return (hashset_t)hashmap_new(0); }

bool hashset_contains(hashset_t set, str_t key) {
  return hashmap_contains(CAST_SET(set), key);
}

str_t hashset_get(hashset_t set, str_t key) {
  keyval_t *kv = hashmap_keyval_get(CAST_SET(set), key);
  if (kv == NULL) {
    panic("no such key in hashset");
  } else {
    return kv->key;
  }
}

bool hashset_insert(hashset_t set, str_t key) {
  return hashmap_insert(CAST_SET(set), key, NULL);
}

bool hashset_remove(hashset_t set, str_t key) {
  return hashmap_remove(CAST_SET(set), key);
}

usize hashmap_sizeof() { return sizeof(struct hashmap); }
usize hashset_sizeof() { return sizeof(struct hashset); }
usize hashmap_len(hashmap_t map) { return map->len; }
usize hashset_len(hashset_t set) { return hashmap_len(CAST_SET(set)); }

void hashmap_iter(hashmap_t map, void(lambda)(usize, str_t, void *)) {
  if (map->len == 0) {
    return;
  }

  usize entry_size = map->entry_size;
  usize cap = hashmap_getcap(map);
  usize bcap = buf_size(entry_size, cap);

  usize remaining = map->len - 1;

  for (usize i = 0; i < bcap; i += entry_size) {
    for (keyval_t *kv = buf_byte_get(map->buf, i); kv != NULL; kv = kv->next) {
      if (kv->hash != 0) {
        lambda(remaining, kv->key, keyval_elt_get(kv));
        remaining -= 1;
      }
    }
  }
}

void hashset_iter(hashset_t set, void (*lambda)(usize, str_t)) {
  hashmap_t map = CAST_SET(set);
  if (map->len == 0) {
    return;
  }

  usize entry_size = map->entry_size;
  usize cap = hashmap_getcap(map);
  usize bcap = buf_size(entry_size, cap);

  usize remaining = map->len - 1;

  for (usize i = 0; i < bcap; i += entry_size) {
    for (keyval_t *kv = buf_byte_get(map->buf, i); kv != NULL; kv = kv->next) {
      if (kv->hash != 0) {
        lambda(remaining, kv->key);
        remaining -= 1;
      }
    }
  }
}

void hashmap_debug(hashmap_t map) {
  usize entry_size = map->entry_size;
  usize cap = hashmap_getcap(map);
  usize bcap = buf_size(entry_size, cap);

  for (usize i = 0; i < bcap; i += entry_size) {
    eprintln("------ BUCKET %lu -------", i / entry_size);
    for (keyval_t *kv = buf_byte_get(map->buf, i); kv != NULL; kv = kv->next) {
      eprint(": HASH = %016lx, key = ", kv->hash);
      fdebug_str(stderr, kv->key.data, kv->key.len);
      eprintln("");
    }
  }
}
