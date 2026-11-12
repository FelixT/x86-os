#include "kobj.h"
#include "tasks.h"
#include "windowmgr.h"
#include "dma.h"
#include "shared.h"
#include "fs.h"
#include "msg.h"

static uint32_t kobj_uid_counter = 1;
static uint32_t kobj_ref_uid_counter = 1;

const kobj_t kobjs[KOBJ_NONE+1] = {
   [KOBJ_DMA] = {
      .type = KOBJ_DMA,
      .free_func = &dma_cleanup,
      .close_func = NULL,
      .stop_func = NULL,
      .connect_func = NULL,
      .map_func = &dma_map,
      .default_rights = KOBJ_RIGHT_READ|KOBJ_RIGHT_WRITE|KOBJ_RIGHT_DUPLICATE,
      .control_func = NULL
   },
   [KOBJ_PCI] = {
      .type = KOBJ_PCI,
      .free_func = &pci_free_inst,
      .close_func = NULL,
      .stop_func = &pci_stop,
      .connect_func = NULL,
      .map_func = &pci_map,
      .default_rights = KOBJ_RIGHT_READ|KOBJ_RIGHT_WRITE|KOBJ_RIGHT_CONTROL,
      .control_func = &pci_control
   },
   [KOBJ_SHARED] = {
      .type = KOBJ_SHARED,
      .free_func = &shared_free_inst,
      .close_func = NULL,
      .stop_func = NULL,
      .connect_func = NULL,
      .map_func = &shared_map,
      .default_rights = KOBJ_RIGHT_READ|KOBJ_RIGHT_WRITE|KOBJ_RIGHT_TRANSFER|KOBJ_RIGHT_DUPLICATE,
      .control_func = NULL
   },
   [KOBJ_FILE] = {
      .type = KOBJ_FILE,
      .free_func = &fs_free,
      .close_func = NULL,
      .stop_func = NULL,
      .connect_func = NULL,
      .map_func = NULL,
      .default_rights = KOBJ_RIGHT_READ|KOBJ_RIGHT_WRITE|KOBJ_RIGHT_TRANSFER|KOBJ_RIGHT_DUPLICATE,
      .control_func = NULL
   },
   [KOBJ_PORT] = {
      .type = KOBJ_PORT,
      .free_func = &port_free,
      .close_func = NULL,
      .stop_func = &port_stop,
      .connect_func = NULL,
      .map_func = NULL,
      .default_rights = KOBJ_RIGHT_READ//|KOBJ_RIGHT_TRANSFER
   },
   [KOBJ_CHANNEL_CONN] = {
      .type = KOBJ_CHANNEL_CONN,
      .free_func = &port_free_connection,
      .close_func = NULL,
      .stop_func = &port_stop_connection,
      .connect_func = NULL,
      .map_func = NULL,
      .default_rights = KOBJ_RIGHT_READ|KOBJ_RIGHT_WRITE//|KOBJ_RIGHT_DUPLICATE|KOBJ_RIGHT_TRANSFER
   }
};

kobj_inst_t *create_kobj_inst(int type, void *data) {
   if(type < 0 || type >= KOBJ_NONE)
      return NULL;
   kobj_inst_t *kobj = kmalloc(sizeof(kobj_inst_t));
   if(!kobj) return NULL;
   
   kobj->uid = kobj_uid_counter++;
   kobj->data = data;
   kobj->obj = &kobjs[type];
   kobj->ref_count = 0;
   kobj->handle_count = 0;
   kobj->stopped = false;
   return kobj;
}

static void kobj_stop(kobj_inst_t *inst) {
   if(inst->stopped || !inst->obj->stop_func) return;
   inst->obj->stop_func(inst);
   inst->stopped = true;
}

void free_kobj_inst(kobj_inst_t *inst) {
   kobj_stop(inst);
   if(inst->obj->free_func)
      inst->obj->free_func(inst);
   kfree(inst);
}

kobj_ref_t *create_kobj_ref(kobj_inst_t *inst, uint32_t rights) {
   kobj_ref_t *ref = kmalloc(sizeof(kobj_ref_t));
   if(!ref) return NULL;
   kobj_add_ref(inst);
   inst->handle_count++;
   ref->uid = kobj_ref_uid_counter++;
   if(!KOBJ_HANDLE_CHECK(ref->uid)) // skip uid that causes blank checkbits (i.e. bare index handles)
      ref->uid = kobj_ref_uid_counter++;
   ref->inst = inst;
   ref->rights = inst->obj->default_rights & rights;
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

   inst->handle_count--;
   if(inst->handle_count == 0)
      kobj_stop(inst);

   kobj_release_ref(inst);

   kfree(ref);

   return true;
}
