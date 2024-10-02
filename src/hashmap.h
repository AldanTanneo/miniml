#pragma once

#include <stdbool.h>

#include "utils.h"

struct hashmap;
// polymorphic hashmap keyed by strings
typedef struct hashmap *hashmap_t;

// create a new empty hashmap
hashmap_t hashmap_new(usize elt_size);
// get a pointer to a value in a hashmap, or NULL if not present
void *hashmap_get(hashmap_t map, str_t key);
// check if a hashmap contains a key
bool hashmap_contains(hashmap_t map, str_t key);
// insert a new key and value into a hashmap
//
// return false if the key was already
// present, and overwrite the previous value
bool hashmap_insert(hashmap_t map, str_t key, void *value);
// remove a key from a hashmap
bool hashmap_remove(hashmap_t map, str_t key);
// call a function with each key-value pair in the map, with the first parameter
// indicating the number of keys yet to be visited
void hashmap_iter(hashmap_t map, void (*lambda)(usize, str_t, void *));

usize hashmap_len(hashmap_t map);
usize hashmap_sizeof();

struct hashset;
// string hashset
typedef struct hashset *hashset_t;

// create a new empty hashset
hashset_t hashset_new();
// check if a hashset contains a key
bool hashset_contains(hashset_t set, str_t key);
// get a value from the hashset. This value must NEVER be modified.
str_t hashset_get(hashset_t set, str_t key);
// insert a new key into a hashset, return false if the key was already present
bool hashset_insert(hashset_t set, str_t key);
// remove a key from a hashset, return false if the key was not in the set
bool hashset_remove(hashset_t set, str_t key);
// call a function with each value in the set, with the first parameter
// indicating the number of keys yet to be visited
void hashset_iter(hashset_t set, void (*lambda)(usize, str_t));

usize hashset_sizeof();
usize hashset_len(hashset_t set);

void hashmap_debug(hashmap_t map);
