// usermode malloc and free

#include "stdlib.h"
#include "../prog.h"

typedef struct block_t {
    size_t size;
    int is_free;
    struct block_t *next;
    uint32_t magic; // used to detect heap corruption
} block_t;

#define BLOCK_MAGIC 0xDEADBEEF
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

static block_t *heap_list_head = NULL; // linked list of all blocks

block_t *find_free_block(size_t size);

void *malloc(size_t size) {
    if(size == 0) return NULL;

    size = ALIGN(size);
    
    block_t *block = find_free_block(size);
    // found block, mark as used
    if(block) {
        block->is_free = 0;
        
        // if block is much larger than needed, split it
        size_t remaining = block->size - size;
        if(remaining > sizeof(block_t) + ALIGNMENT) {
            // create free block from remainder
            block_t *new_block = (block_t*)((char*)(block + 1) + size);
            new_block->size = remaining - sizeof(block_t);
            new_block->is_free = 1;
            new_block->magic = BLOCK_MAGIC;
            new_block->next = block->next;
            
            block->size = size;
            block->next = new_block;
        }
        
        return (void*)(block + 1);
    }
    
    // otherwise, resize heap
    block = sbrk(sizeof(block_t) + size);
    if(block == (void*)-1) {
        debug_write_str("malloc: sbrk failed\n");
        return NULL;
    }
    
    block->size = size;
    block->is_free = 0;
    block->magic = BLOCK_MAGIC;
    block->next = NULL;
    
    if(heap_list_head == NULL) {
        heap_list_head = block;
    } else {
        block_t *current = heap_list_head;
        while(current->next) {
            current = current->next;
        }
        current->next = block;
    }
    
    return (void*)(block + 1);
}

void coalesce_free_blocks(void);

void free(void *ptr) {
    if(!ptr) return;
    
    // get block info
    block_t *block = (block_t*)ptr - 1;
    
    if(block->magic != BLOCK_MAGIC) {
        debug_write_str("free: invalid pointer or corrupted block\n");
        return;
    }
    
    if(block->is_free) {
        debug_write_str("free: already freed\n");
        return;
    }
    
    block->is_free = 1;
    
    coalesce_free_blocks();
}

block_t* find_free_block(size_t size) {
    block_t *current = heap_list_head;
    
    // return first free block
    while(current) {
        if(current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

void coalesce_free_blocks(void) {
    block_t *current = heap_list_head;
    
    while(current && current->next) {
        // coalesce if both blocks are free and physically adjacent
        if(current->is_free && current->next->is_free) {
            void *current_end = (void*)((char*)current + sizeof(block_t) + current->size);
            if(current_end == (void*)current->next) {
                // merge into first block
                block_t *next_block = current->next;
                current->size += sizeof(block_t) + next_block->size;
                
                // remove next block from list
                current->next = next_block->next;
                continue;
            }
        }
        
        current = current->next;
    }
}
