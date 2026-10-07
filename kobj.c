#include "kobj.h"
#include "tasks.h"
#include "windowmgr.h"
#include "dma.h"

static uint32_t kobj_uid_counter = 1;
static uint32_t kobj_ref_uid_counter = 1;

const kobj_t kobjs[KOBJ_NONE+1] = {
   [KOBJ_DMA] = {.type = KOBJ_DMA, .free_func = &dma_cleanup, .close_func = &dma_close}
};

kobj_inst_t *create_kobj_inst(int type, void *data) {
   kobj_inst_t *kobj = malloc(sizeof(kobj_inst_t));
   if(!kobj) return NULL;
   
   kobj->uid = kobj_uid_counter++;
   kobj->data = data;
   kobj->mutex = NULL;
   kobj->obj = &kobjs[type];
   kobj->ref_count = 0;
   return kobj;
}

void free_kobj_inst(kobj_inst_t *inst) {
   debug_printf("Freeing kobj inst %u type %i free 0x%h\n", inst->uid, inst->obj->type, inst->obj->free_func);
   if(inst->obj && inst->obj->free_func)
      inst->obj->free_func(inst);
   free((uint32_t)inst, sizeof(kobj_inst_t));
}

kobj_ref_t *create_kobj_ref(kobj_inst_t *inst) {
   kobj_ref_t *ref = malloc(sizeof(kobj_ref_t));
   if(!ref) return NULL;
   inst->ref_count++;
   ref->uid = kobj_ref_uid_counter++;
   ref->inst = inst;
   ref->data = NULL;
   return ref;
}

bool kobj_release(kobj_ref_t *ref, void *process, bool ending) {
   if(!ref) return false;

   kobj_inst_t *inst = ref->inst;
   if(!inst) return false;
   inst->ref_count--;

   if(inst->obj->close_func)
      inst->obj->close_func(ref, process, ending);

   free((uint32_t)ref, sizeof(kobj_ref_t));

   if(inst->ref_count) return true;

   // free obj instance
   free_kobj_inst(inst);
   return true;
}
