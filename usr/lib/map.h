#ifndef MAP_H
#define MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEY_MAX_LEN 31

typedef struct {
   char key[KEY_MAX_LEN+1];
   void *value;
   bool occupied;
} map_entry_t;

typedef struct {
   map_entry_t *table;
   uint32_t size;
   uint32_t count;
} map_t;

void map_init(map_t *map, uint32_t size);
uint32_t hash_fnv1a(const char *key);
void map_insert(map_t *map, const char *key, void *value);
void *map_lookup(map_t *map, const char *key);
bool map_remove(map_t *map, const char *key);

#endif