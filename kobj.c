#include "kobj.h"
#include "tasks.h"
#include "windowmgr.h"
#include "dma.h"
#include "shared.h"

static uint32_t kobj_uid_counter = 1;
static uint32_t kobj_ref_uid_counter = 1;

const kobj_t kobjs[KOBJ_NONE+1] = {
   [KOBJ_DMA] = {
      .type = KOBJ_DMA,
      .free_func = &dma_cleanup,
      .close_func = &dma_close,
      .stop_func = NULL,
      .connect_func = NULL,
      .default_rights = KOBJ_RIGHTS_ALL
   },
   [KOBJ_PCI] = {
      .type = KOBJ_PCI,
      .free_func = &pci_free_inst,
      .close_func = &pci_close,
      .stop_func = &pci_stop,
      .connect_func = NULL,
      .default_rights = KOBJ_RIGHTS_ALL
   },
   [KOBJ_SHARED] = {
      .type = KOBJ_SHARED,
      .free_func = &shared_free_inst,
      .close_func = NULL,
      .stop_func = NULL,
      .connect_func = &shared_connect,
      .default_rights = KOBJ_RIGHTS_ALL
   }
};

kobj_inst_t *create_kobj_inst(int type, void *data) {
   if(type < 0 || type >= KOBJ_NONE)
      return NULL;
   kobj_inst_t *kobj = malloc(sizeof(kobj_inst_t));
   if(!kobj) return NULL;
   
   kobj->uid = kobj_uid_counter++;
   kobj->data = data;
   kobj->mutex = NULL;
   kobj->obj = &kobjs[type];
   kobj->ref_count = 0;
   kobj->stopped = false;
   return kobj;
}

void free_kobj_inst(kobj_inst_t *inst) {
   if(!inst->stopped && inst->obj->stop_func) {
      inst->obj->stop_func(inst);
      inst->stopped = true;
   }
   if(inst->obj->free_func)
      inst->obj->free_func(inst);
   free((uint32_t)inst, sizeof(kobj_inst_t));
}

kobj_ref_t *create_kobj_ref(kobj_inst_t *inst) {
   kobj_ref_t *ref = malloc(sizeof(kobj_ref_t));
   if(!ref) return NULL;
   kobj_add_ref(inst);
   ref->uid = kobj_ref_uid_counter++;
   if(!KOBJ_HANDLE_CHECK(ref->uid)) // skip uid that causes blank checkbits (i.e. bare index handles)
      ref->uid = kobj_ref_uid_counter++;
   ref->inst = inst;
   ref->rights = inst->obj->default_rights;
   ref->data = NULL;
   return ref;
}

void kobj_add_ref(kobj_inst_t *inst) {
   inst->ref_count++;
}

void kobj_release_ref(kobj_inst_t *inst) {
   inst->ref_count--;

   if(inst->ref_count == 0)
      free_kobj_inst(inst);
}

bool kobj_release(kobj_ref_t *ref, void *process, bool ending) {
   if(!ref) return false;

   kobj_inst_t *inst = ref->inst;
   if(!inst) return false;

   if(inst->obj->close_func)
      inst->obj->close_func(ref, process, ending);

   kobj_release_ref(inst);
   
   free((uint32_t)ref, sizeof(kobj_ref_t));

   return true;
}
