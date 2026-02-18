// basic hashmap

#include "map.h"
#include "stdlib.h"
#include "../prog.h"
#include "../../lib/string.h"

void map_init(map_t *map, uint32_t size) {
   map->table = malloc(size * sizeof(map_entry_t));
   memset(map->table, 0, size * sizeof(map_entry_t));
   map->size = size;
   map->count = 0;
}

uint32_t hash_fnv1a(const char *key) {
   uint32_t hash = 2166136261u;
   while (*key) {
      hash ^= (uint8_t)*key++;
      hash *= 16777619u;
   }
   return hash;
}

void map_insert(map_t *map, const char *key, void *value) {
   uint32_t index = hash_fnv1a(key) & (map->size - 1);
   uint32_t start = index;
   
   while(map->table[index].occupied) {
      if(strequ(map->table[index].key, (char*)key)) {
         // update existing
         map->table[index].value = value;
         return;
      }
      index = (index + 1) & (map->size - 1);
      if(index == start) return;  // Table full
   }
   
   // insert new
   strncpy(map->table[index].key, key, KEY_MAX_LEN);
   map->table[index].key[KEY_MAX_LEN] = '\0';
   map->table[index].value = value;
   map->table[index].occupied = true;
   map->count++;
}

void *map_lookup(map_t *map, const char *key) {
   uint32_t index = hash_fnv1a(key) & (map->size - 1);
   uint32_t start = index;
    
   while(map->table[index].occupied) {
      if(strequ(map->table[index].key, (char*)key)) {
         return map->table[index].value;
      }
      index = (index + 1) & (map->size - 1);
      if(index == start) break;
   }
   
   return NULL;
}

bool map_remove(map_t *map, const char *key) {
   uint32_t index = hash_fnv1a(key) & (map->size - 1);
   uint32_t start = index;
   
   while(map->table[index].occupied) {
      if(strequ(map->table[index].key, (char*)key)) {
         map->table[index].occupied = false;
         map->count--;
         return true;
      }
      index = (index + 1) & (map->size - 1);
      if(index == start) break;
   }
   
   return false;
}
