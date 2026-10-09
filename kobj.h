#ifndef KOBJ_H
#define KOBJ_H

#include <stdbool.h>
#include <stdint.h>

#include "ksync.h"

// resources/objects that the kernel knows how to handle

typedef int kobj_handle_t; // index into process->kobj[]

typedef enum {
    KOBJ_DMA,
    KOBJ_PCI, // pci device
    KOBJ_SHARED,
    KOBJ_NONE // must be at end
} kobj_type_t;

typedef struct kobj_inst_t kobj_inst_t;
typedef struct kobj_ref_t kobj_ref_t;
typedef struct process_t process_t;

typedef struct kobj_t {
    kobj_type_t type;
    
    bool (*connect_func)(kobj_ref_t *ref, process_t *process); // called when duplicating handle, NULL if only one ref possible
    void (*stop_func)(kobj_inst_t *inst); // disable device (before resources are free'd)
    void (*close_func)(kobj_ref_t *ref, process_t *process, bool ending); // called when a ref is closed (ending = closing as process is ending)
    void (*free_func)(kobj_inst_t *inst); // called when last ref is closed
} kobj_t;

typedef struct kobj_inst_t {
    uint32_t uid;
    const kobj_t *obj;
    void *data;
    int ref_count;
    kmutex_t *mutex;
    bool stopped; // stop_func sent
} kobj_inst_t;

typedef struct kobj_ref_t {
    uint32_t uid;
    kobj_inst_t *inst;
    void *data; // per user data
} kobj_ref_t;

kobj_inst_t *create_kobj_inst(int type, void *data);
kobj_ref_t *create_kobj_ref(kobj_inst_t *inst); // reference to an instance
void free_kobj_inst(kobj_inst_t *inst);

bool kobj_release(kobj_ref_t *ref, void *process, bool ending);

#endif