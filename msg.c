#include "msg.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lib/maths.h"
#include "windowmgr.h" // for debug

#include "fs.h"
#include "api.h"

// message passing

// allows passing of small messages between tasks
// sync or async (with queues)

// todo: sync timeout
// todo: large messages use shared memory (cow)

uint32_t call_uid_counter = 1;

msg_port_t *ports = NULL;

msg_port_t *find_port_by_name(char *name) {
   msg_port_t *cur = ports;
   while(cur) {
      if(strequ(cur->name, name))
         return cur;
      cur = cur->next;
   }
   return NULL;
}

int create_port(task_state_t *task, char *name, bool client_reserves, kobj_handle_t *port_h) {
   // ports have path-like namespace
   // process needs to be privliedged to register /sys/*
   process_t *process = task->process;
   if(strstartswith(name, "/sys/") && !process->privileged) {
      debug_printf("create_port: process %u isn't privileged enough for '%s'\n", process->uid, name);
      return MSG_ERR_NO_PRIVILEGE;
   }
   if(fs_exists(name)) {
      debug_printf("create_port: '%s' is an existing file\n", name);
      return MSG_ERR_NAME_TAKEN;
   }
   if(find_port_by_name(name)) {
      debug_printf("create_port: port '%s' already exists\n", name);
      return MSG_ERR_NAME_TAKEN;
   }

   msg_port_t *port = kmalloc(sizeof(msg_port_t));
   if(!port)
      return MSG_ERR_NOMEM;

   strncpy(port->name, name, sizeof(port->name)-1);
   port->owner_uid = process->uid;
   port->owner_taskid = task->task_id;
   port->owner_taskuid = task->task_uid;
   for(int i = 0; i < MSG_MAX_CHANNELS; i++)
      port->channels[i] = NULL;
   port->channel_count = 0;
   for(int i = 0; i < MAX_TASK_THREADS; i++)
      port->waiters[i].active = false;
   port->client_reserves = client_reserves;
   port->signal_flags = 0;
   port->next = NULL;

   // create kobj
   kobj_inst_t *inst = create_kobj_inst(KOBJ_PORT, port);
   if(!inst) {
      kfree(port);
      return MSG_ERR_HANDLE_FAIL;
   }

   kobj_handle_t h = process_acquire_kobj(process, inst, KOBJ_RIGHT_SAME);
   if(h < 0) {
      free_kobj_inst(inst);
      return MSG_ERR_HANDLE_FAIL;
   }

   port->next = ports;
   ports = port;

   *port_h = h;
   return 0;
}

bool port_free_channel_exists(msg_port_t *port) {
   if(port->channel_count < MSG_MAX_CHANNELS) {
      return true;
   }

   for(int i = 0; i < port->channel_count; i++) {
      if(port->channels[i]) continue;
      return true;
   }
   return false;
}

int port_claim_free_channel_index(msg_port_t *port) {
   if(port->channel_count < MSG_MAX_CHANNELS) {
      port->channel_count++;
      return port->channel_count-1;
   }

   for(int i = 0; i < port->channel_count; i++) {
      if(port->channels[i]) continue;
      return i;
   }
   return -1;
}

static task_state_t *msg_get_task(int task_id, uint32_t task_uid) {
   if(task_id < 0 || task_id >= TOTAL_TASKS) return NULL;
   task_state_t *task = &gettasks()[task_id];
   if(!task->enabled || task->task_uid != task_uid) return NULL;
   return task;
}

static int create_channel(msg_port_t *port, task_state_t *client_task, msg_channel_t *channel) {
   task_state_t *server_task = msg_get_task(port->owner_taskid, port->owner_taskuid);
   if(!server_task) {
      kfree(channel);
      return MSG_ERR_PEER_DISCONNECTED;
   }
   process_t *server_process = server_task->process;
   process_t *client_process = client_task->process;

   msg_channel_conn_t *server_conn = &channel->conns[0];
   msg_channel_conn_t *client_conn = &channel->conns[1];

   server_conn->connected = false;
   client_conn->connected = false;
   server_conn->released = true; // released until inst created
   client_conn->released = true;

   channel->port = port;
   channel->server_unanswered = 0;
   // sync
   channel->call_uid = 0;
   channel->client_recv_msg = (msg_recv_t){.buffer = NULL, .handle_count = 0};
   channel->client_send_msg = (msg_send_t){.buffer = NULL, .handle_count = 0};
   channel->client_recv_msg_user = NULL;
   channel->call_pending = false;

   server_conn->channel = channel;
   server_conn->blocked = false;
   server_conn->flags = 0;
   server_conn->notify_pending = false;
   server_conn->server = true;
   server_conn->queue = (msg_queue_t){.count = 0, .reserved = 0, .head = 0, .tail = 0};
   server_conn->taskid = server_task->task_id;
   server_conn->taskuid = server_task->task_uid;

   client_conn->channel = channel;
   client_conn->blocked = false;
   client_conn->flags = 0;
   client_conn->notify_pending = false;
   client_conn->server = false;
   client_conn->queue = (msg_queue_t){.count = 0, .reserved = 0, .head = 0, .tail = 0};
   client_conn->taskid = client_task->task_id;
   client_conn->taskuid = client_task->task_uid;

   // create conn handles
   kobj_inst_t *client_inst = create_kobj_inst(KOBJ_CHANNEL_CONN, client_conn);
   if(!client_inst) {
      kfree(channel);
      return MSG_ERR_NOMEM;
   }
   client_conn->released = false;

   handle_t client_h = process_acquire_kobj(client_process, client_inst, KOBJ_RIGHT_SAME);
   if(client_h < 0) {
      free_kobj_inst(client_inst);
      return MSG_ERR_HANDLE_FAIL;
   }
   client_conn->handle = client_h;
   client_conn->connected = true;

   kobj_inst_t *server_inst = create_kobj_inst(KOBJ_CHANNEL_CONN, server_conn);
   if(!server_inst) {
      process_release_kobj(client_process, client_h);
      return MSG_ERR_NOMEM;
   }
   server_conn->released = false;

   handle_t server_h = process_acquire_kobj(server_process, server_inst, KOBJ_RIGHT_SAME);
   if(server_h < 0) {
      free_kobj_inst(server_inst);
      process_release_kobj(client_process, client_h);
      return MSG_ERR_PEER_HANDLE_FAIL;
   }
   server_conn->handle = server_h;
   server_conn->connected = true;

   return 0;
}

// create channel connection
int port_connect(task_state_t *task, char *name, handle_t *conn_h) {
   process_t *process = task->process;
   msg_port_t *port = find_port_by_name(name);
   if(!port) {
      debug_printf("port_connect: couldn't find port '%s'\n", name);
      return MSG_ERR_PORT_NOT_FOUND;
   }

   if(port->owner_taskuid == task->task_uid) {
      debug_printf("port_connect: task %u of process %u can't connect to its own port\n", task->task_uid, process->uid);
      return MSG_ERR_SELF;
   }

   if(!port_free_channel_exists(port)) {
      debug_printf("port_connect: no free channels\n");
      return MSG_ERR_PORT_FULL;
   }

   msg_channel_t *channel = kmalloc(sizeof(msg_channel_t));
   if(!channel) return MSG_ERR_NOMEM;

   int c = port_claim_free_channel_index(port);
   if(c < 0) {
      debug_printf("port_connect: failed to claim free channel index\n");
      kfree(channel);
      return MSG_ERR_PORT_FULL;
   }

   int r = create_channel(port, task, channel);
   if(r < 0)
      return r;

   port->channels[c] = channel;

   *conn_h = channel->conns[1].handle;
   return 0;
}

typedef enum {
   MSG_NOTIFY_DELIVERED, // msg_func called or queued
   MSG_NOTIFY_NO_CALLBACK, // no msg_func (recipient can poll or snooze instead)
   MSG_NOTIFY_DROPPED // msg_func couldn't be queued - retry
} msg_notify_status_t;

static msg_notify_status_t call_msg_func(registers_t *regs, task_state_t *task, handle_t conn_h, uint32_t flags) {
   if(!task->msg_func) {
      task_wake(task);
      return MSG_NOTIFY_NO_CALLBACK;
   }
   uint32_t args[2] = { conn_h, flags };
   bool delivered;
   if(regs)
      delivered = task_call_subroutine(regs, task, E_MSG, (uint32_t)task->msg_func, args, 2);
   else
      delivered = task_queue_subroutine(task, E_MSG, (uint32_t)task->msg_func, args, 2);

   return delivered ? MSG_NOTIFY_DELIVERED : MSG_NOTIFY_DROPPED;
}

bool msg_notif_is_stale(task_state_t *task, handle_t conn_h) {
   kobj_ref_t *ref = process_get_kobj_type(task->process, conn_h, KOBJ_CHANNEL_CONN);
   if(!ref) return true;
   msg_channel_conn_t *conn = ref->inst->data;
   if(conn->connected) return false;
   return conn->queue.count == 0;
}

static void msg_set_notify_pending(task_state_t *task, msg_channel_conn_t *conn, bool pending) {
   conn->notify_pending = pending;
   if(pending)
      task->msg_notify_pending = true;
}

static int msg_find_waiter(msg_port_t *port);
static int msg_deliver_queued(registers_t *regs, task_state_t *task, msg_channel_conn_t *conn, msg_recv_t *recv_msg, msg_recv_t *recv_msg_user);
static int msg_deliver_flags(registers_t *regs, task_state_t *task, handle_t conn_h, uint32_t flags, msg_recv_t *recv_msg);

// deliver async send to paused waiter
static bool msg_deliver_to_waiter(msg_port_t *port, msg_channel_t *channel) {
   int w = msg_find_waiter(port);
   if(w < 0) return false;
   msg_waiter_t *waiter = &port->waiters[w];
   task_state_t *task = msg_get_task(waiter->task_id, waiter->task_uid);
   if(!task) return false;

   int r;
   msg_channel_conn_t *conn = &channel->conns[0];
   if(conn->queue.count > 0) {
      r = msg_deliver_queued(&task->registers, task, conn, &waiter->recv_msg, waiter->recv_msg_user);
   } else if(conn->flags) {
      r = msg_deliver_flags(&task->registers, task, conn->handle, conn->flags, waiter->recv_msg_user);
      if(r >= 0)
         conn->flags = 0;
   } else {
      return false;
   }

   if(r < 0)
      task->registers.ebx = r;
   waiter->active = false;
   task_resume(task);
   return true;
}

int port_send(registers_t *regs, task_state_t *task, handle_t conn_h, msg_send_t *send_msg, uint32_t flags) {
   int err = msg_validate_send(task, send_msg);
   if(err < 0)
      return err;

   if(send_msg->handle_count > 0)
      return MSG_ERR_INVALID_MSG; // async handle passing is unsupported

   if(send_msg->size == 0) {
      debug_printf("port_send: empty messages are invalid\n");
      return MSG_ERR_EMPTY_MSG;
   }

   process_t *process = task->process;

   kobj_ref_t *ref = process_get_kobj_type(process, conn_h, KOBJ_CHANNEL_CONN);
   if(!ref)
      return MSG_ERR_HANDLE_FAIL;

   msg_channel_conn_t *conn = ref->inst->data;
   msg_channel_t *channel = conn->channel;
   msg_port_t *port = channel->port;

   if(!conn->connected) {
      debug_printf("port_send: sender has disconnected from channel\n");
      return MSG_ERR_DISCONNECTED;
   }

   msg_channel_conn_t *peer_conn = &channel->conns[conn->server];
   if(!peer_conn->connected) {
      debug_printf("port_send: peer has disconnected\n");
      return MSG_ERR_PEER_DISCONNECTED;
   }

   bool reserve_slot = port->client_reserves && !conn->server && (flags & MSG_EXPECT_REPLY);
   // use explicit flag for replies to avoid consuming for unrelated sends
   bool writing_in_reserved = port->client_reserves && conn->server && (flags & MSG_REPLY) && channel->server_unanswered > 0;

   msg_queue_t *queue = &peer_conn->queue;
   if(queue->count + queue->reserved - (writing_in_reserved ? 1 : 0) >= MSG_QUEUE_DEPTH) {
      // full
      debug_printf("port_send: queue is full\n");
      // send fails - notify sending task, and allow them to decide what to do: drop or wait for queue to free up
      return MSG_ERR_QUEUE_FULL;
   }

   task_state_t *recipient_task = msg_get_task(peer_conn->taskid, peer_conn->taskuid);
   if(!recipient_task) {
      debug_printf("port_send: invalid/ended recipient task\n");
      return MSG_ERR_INVALID_TASK;
   }

   if(reserve_slot) {
      msg_queue_t *recv_queue = &conn->queue;
      if(recv_queue->count + recv_queue->reserved >= MSG_QUEUE_DEPTH) {
         // sender (client) receive queue is full, and channel requires a reserved slot
         debug_printf("port_send: receieve queue is full with client_reserves enabled\n");
         return MSG_ERR_RECEIVE_QUEUE_FULL;
      } else {
         recv_queue->reserved++;
      }
   }

   int i;
   if(queue->count == 0) {
      i = 0;
      queue->count = 1;
      queue->head = i;
      queue->tail = i;
   } else {
      i = (queue->tail + 1) % MSG_QUEUE_DEPTH;
      queue->count++;
      queue->tail = i;
   }

   if(writing_in_reserved) {
      queue->reserved--;  // consume reserved slot
      channel->server_unanswered--;
   }

   msg_obj_t *obj = &queue->slots[i];
   memcpy(obj->data, send_msg->buffer, send_msg->size);
   obj->length = send_msg->size;
   obj->flags = flags & (MSG_EXPECT_REPLY | MSG_REPLY);

   // check for waiter (sync) and deliver msg
   if(!conn->server && !recipient_task->msg_func && msg_deliver_to_waiter(port, channel))
      return 0;

   // send notification (async)
   msg_notify_status_t notified = call_msg_func(regs, recipient_task, peer_conn->handle, peer_conn->flags);
   if(notified == MSG_NOTIFY_DROPPED)
      debug_printf("port_send: recipient event queue full, deferring notification\n");
   msg_set_notify_pending(recipient_task, peer_conn, notified == MSG_NOTIFY_DROPPED);
   if(notified == MSG_NOTIFY_DELIVERED)
      peer_conn->flags = 0; // clear

   return 0; // success
}

// set event flags & wake task
int port_signal(msg_port_t *port, uint32_t flag) {
   port->signal_flags |= flag; // stays set even if delivery failure is returned
   int w = msg_find_waiter(port);
   // no waiters
   if(w < 0) {
      task_state_t *task = msg_get_task(port->owner_taskid, port->owner_taskuid);
      if(!task) return MSG_ERR_INVALID_TASK;
      msg_notify_status_t notified = call_msg_func(NULL, task, 0, port->signal_flags); // queue msg
      if(notified == MSG_NOTIFY_DROPPED) { // queue full
         debug_printf("port_signal: dropped\n");
         return MSG_ERR_QUEUE_FULL;
      }
      if(notified == MSG_NOTIFY_DELIVERED)
         port->signal_flags = 0;

      return 0; // success
   }

   msg_waiter_t *waiter = &port->waiters[w];
   task_state_t *waiter_task = msg_get_task(waiter->task_id, waiter->task_uid);
   if(!waiter_task) {
      return MSG_ERR_INVALID_TASK;
   }
   int r = msg_deliver_flags(&waiter_task->registers, waiter_task, 0, port->signal_flags, waiter->recv_msg_user);
   if(r >= 0)
      port->signal_flags = 0;
   waiter_task->registers.ebx = r;
   waiter->active = false;
   task_resume(waiter_task);
   return r;
}

static bool msg_add_waiter(msg_port_t *port, task_state_t *task, msg_recv_t *recv_msg) {
   // add to waiter list
   msg_waiter_t *slot = NULL;
   for(int i = 0; i < MAX_TASK_THREADS; i++) {
      msg_waiter_t *waiter = &port->waiters[i];
      if(waiter->active && waiter->task_uid == task->task_uid) { // reuse
         slot = waiter;
         break;
      }
      if(!slot && (!waiter->active || !msg_get_task(waiter->task_id, waiter->task_uid)))
         slot = waiter;
   }
   if(!slot) return false;

   slot->recv_msg = *recv_msg;
   slot->recv_msg_user = recv_msg;
   slot->task_id = task->task_id;
   slot->task_uid = task->task_uid;
   slot->active = true;
   return true;
}

static int msg_find_waiter(msg_port_t *port) {
   for(int i = 0; i < MAX_TASK_THREADS; i++) {
      msg_waiter_t *waiter = &port->waiters[i];
      if(!waiter->active) continue;
      task_state_t *t = msg_get_task(waiter->task_id, waiter->task_uid);
      if(!t || !t->paused || t->pause_reason != PAUSE_MSG_RECEIVE) { // stale
         waiter->active = false;
         continue;
      }
      return i;
   }
   return -1;
}

void msg_free_handles(process_t *process, handle_t *handles, int count) {
   for(int i = 0; i < count; i++)
      process_release_kobj(process, handles[i]);
}

static int msg_copy_from_sender(task_state_t *send_task, void *dest, void *src, int size) {
   if(send_task == get_current_task_state()) {
      memcpy(dest, src, size);
      return size;
   }
   return copy_from_task(send_task->task_id, dest, src, size);
}

// write kernel fields to receiver's msg_recv_t*
static bool msg_write_receipt(task_state_t *task, msg_recv_t *user_recv_msg, msg_send_t *send_msg, msg_copy_receipt_t *receipt, uint32_t call_uid, uint32_t flags, uint32_t message_flags) {
   msg_recv_t out;
   out.bytes_received = receipt->bytes;
   out.sent_size = send_msg->size;
   out.handles_sent = send_msg->handle_count;
   out.handles_received = receipt->handles;
   out.call_uid = call_uid;
   out.flags = flags;
   out.conn_h = receipt->conn_h;
   out.message_flags = message_flags;
   size_t off = offsetof(msg_recv_t, bytes_received);
   int sz = sizeof(msg_recv_t) - off;
   return copy_to_task(task->task_id, (uint8_t*)user_recv_msg + off, (uint8_t*)&out + off, sz) == sz;
}

static msg_copy_receipt_t msg_do_copy(task_state_t *send_task, task_state_t *receive_task, msg_send_t *send_msg, msg_recv_t *recv_msg, msg_recv_t *recv_msg_user, uint32_t call_uid, handle_t conn_h) {
   // copy from send_msg -> recv_msg, then write the receipt to recv_msg_user
   msg_copy_receipt_t r = {.bytes = 0, .handles = 0, .receiver_status = 0, .sender_status = 0, .conn_h = conn_h};
   msg_handle_t sent[MSG_MAX_HANDLES];
   handle_t handles[MSG_MAX_HANDLES];
   int send_handles = send_msg->handle_count;
   int copy_handles = min(send_handles, recv_msg->handle_count);
   if(send_handles > 0) {
      // copy into handles
      int read_sz = send_handles*sizeof(msg_handle_t);
      if(msg_copy_from_sender(send_task, sent, send_msg->handles, read_sz) != read_sz) {
         r.sender_status = MSG_ERR_INVALID_HANDLES;
         return r;
      }

      // check truncated handles
      for(int i = copy_handles; i < send_handles; i++) {
         if(sent[i].flags & MSG_HANDLE_MOVE) {
            // fail whole send if any truncated handles have move flag
            r.sender_status = MSG_ERR_TOO_MANY_HANDLES;
            return r;
         }
      }
   }

   // copy message (buffer)
   void *send_buffer = send_msg->buffer;
   if(send_buffer) {

      // use tmp buffer (fast for small msg size)
      uint8_t tmp[MSG_MAX_LEN];
      int copy_len = min(recv_msg->buffer_size, send_msg->size); // truncate

      // copy into tmp
      if(msg_copy_from_sender(send_task, tmp, send_buffer, copy_len) != copy_len) {
         r.sender_status = MSG_ERR_INVALID_BUF;
         return r;
      }

      // copy tmp into recv buffer
      if(copy_to_task(receive_task->task_id, recv_msg->buffer, tmp, copy_len) != copy_len) {
         r.sender_status = MSG_ERR_PEER_INVALID_BUF;
         r.receiver_status = MSG_ERR_INVALID_BUF;
         return r;
      }

      // success
      r.bytes = copy_len;

   }
   
   // copy handles
   if(copy_handles > 0) {
      int write_sz = copy_handles*sizeof(handle_t);

      // copy kobjs into recv task and update handles
      for(int i = 0; i < copy_handles; i++) {
         handle_t h = process_copy_kobj(send_task->process, receive_task->process, sent[i].handle, sent[i].rights);
         if(h < 0) {
            msg_free_handles(receive_task->process, handles, i);
            if(h == -2) { // receiver had too many handles
               r.sender_status = MSG_ERR_PEER_INVALID_HANDLES;
               r.receiver_status = MSG_ERR_LIMIT; // too many handles
            } else { // sender's handle is stale or couldn't be connected
               r.sender_status = MSG_ERR_INVALID_HANDLES;
            }
            return r;
         }
         handles[i] = h;
      }

      // copy handles into recv task
      if(copy_to_task(receive_task->task_id, recv_msg->handles, handles, write_sz) != write_sz) {
         msg_free_handles(receive_task->process, handles, copy_handles);
         r.sender_status = MSG_ERR_PEER_INVALID_HANDLES;
         r.receiver_status = MSG_ERR_INVALID_HANDLES;
         return r;
      }

      r.handles = copy_handles;
   }

   if(!msg_write_receipt(receive_task, recv_msg_user, send_msg, &r, call_uid, 0, 0)) {
      // receiver unmapped its msg_recv_t*, undo the handle copy so they don't leak
      msg_free_handles(receive_task->process, handles, r.handles);
      r.sender_status = MSG_ERR_PEER_INVALID_MSG;
      r.receiver_status = MSG_ERR_INVALID_MSG;
      return r;
   }

   // sent successfully - close handles with move flag
   for(int i = 0; i < r.handles; i++) {
      if(sent[i].flags & MSG_HANDLE_MOVE)
         process_release_kobj(send_task->process, sent[i].handle);
   }

   return r;
}

static bool msg_check_handles(process_t *process, bool same_process, msg_handle_t *handles, int size) {
   for(int i = 0; i < size; i++) {
      kobj_ref_t *ref = process_get_kobj(process, handles[i].handle);
      if(!ref || !kobj_ref_can_copy(ref, same_process, handles[i].rights)) return false;
   }
   return true;
}

int msg_call(task_state_t *task, handle_t conn_h, msg_send_t *send_msg, msg_recv_t *recv_msg) {
   int err = msg_validate_send(task, send_msg);
   if(err < 0)
      return err;
   err = msg_validate_recv(task, recv_msg);
   if(err < 0)
      return err;

   if(send_msg->size == 0 && send_msg->handle_count == 0) {
      debug_printf("msg_call: empty messages are invalid\n");
      return MSG_ERR_EMPTY_MSG;
   }

   kobj_ref_t *ref = process_get_kobj_type(task->process, conn_h, KOBJ_CHANNEL_CONN);
   if(!ref)
      return MSG_ERR_HANDLE_FAIL;

   msg_channel_conn_t *conn = ref->inst->data;

   // only the client thread that connected can call (server can't call itself)
   if(conn->server || task->task_uid != conn->taskuid)
      return MSG_ERR_DENIED;

   msg_channel_t *channel = conn->channel;
   msg_port_t *port = channel->port;

   if(!conn->connected) {
      debug_printf("msg_call: disconnected\n");
      return MSG_ERR_DISCONNECTED;
   }

   msg_channel_conn_t *peer_conn = &channel->conns[conn->server];

   if(!peer_conn->connected) {
      debug_printf("msg_call: peer disconnected\n");
      return MSG_ERR_PEER_DISCONNECTED;
   }

   if(!msg_check_handles(task->process, task->process->uid == port->owner_uid, send_msg->handles, send_msg->handle_count)) {
      debug_printf("msg_call: invalid handle\n");
      return MSG_ERR_INVALID_HANDLES;
   }

   channel->client_recv_msg = *recv_msg;
   channel->client_recv_msg_user = recv_msg;

   // find waiting server thread
   int w = msg_find_waiter(port);
   if(w < 0) {
      // server isn't waiting yet, store write until it does
      channel->client_send_msg = *send_msg;
      channel->call_pending = true;
      task_pause(task, PAUSE_MSG_CALL); // task is paused until server replies
      task_interrupt_set(-1, true); // yield once the syscall finishes
      return 0;
   }

   // found waiter, do copy
   msg_waiter_t *waiter = &port->waiters[w];
   task_state_t *server_task = &gettasks()[waiter->task_id];
   msg_recv_t *server_recv_msg = &waiter->recv_msg;
   uint32_t call_uid = call_uid_counter++;
   msg_copy_receipt_t r = msg_do_copy(task, server_task, send_msg, server_recv_msg, waiter->recv_msg_user, call_uid, peer_conn->handle);
   if(r.sender_status < 0) {
      // copy failed
      if(r.receiver_status < 0) {
         // server failure - wake server
         waiter->active = false;
         server_task->registers.ebx = r.receiver_status;
         task_resume(server_task);
      }
      // return error code to api
      return r.sender_status;
   }

   // send success
   waiter->active = false;
   channel->call_uid = call_uid;

   // set server task's regs (matching api_msg_receive)

   server_task->registers.ebx = r.bytes; // bytes read/copied to server
   task_pause(task, PAUSE_MSG_CALL); // task is paused until server replies
   task_resume(server_task);
   task_interrupt_set(server_task->task_id, true); // run server once the syscall finishes

   return 0;
}

// validate usermode msg_send_t*
int msg_validate_send(task_state_t *task, msg_send_t *msg) {
   if(!task_validate_mem(task, msg, sizeof(msg_send_t), 0))
      return MSG_ERR_INVALID_MSG;
   if(msg->size < 0)
      return MSG_ERR_INVALID_MSG;
   if(msg->size > (int)MSG_MAX_LEN) {
      debug_printf("msg: msg length %i too long (max %i)\n", msg->size, MSG_MAX_LEN);
      return MSG_ERR_TOO_LONG; // fail instead of truncate
   }
   if(!task_validate_mem(task, msg->buffer, msg->size, 0))
      return MSG_ERR_INVALID_BUF;
   if(msg->handle_count > MSG_MAX_HANDLES)
      return MSG_ERR_TOO_MANY_HANDLES;
   if(msg->handle_count < 0)
      return MSG_ERR_INVALID_MSG;
   if(!task_validate_mem(task, msg->handles, sizeof(msg_handle_t)*msg->handle_count, 0))
      return MSG_ERR_INVALID_HANDLES;
   return 0;
}

// validate usermode msg_recv_t* (called from msg_call and msg_receive)
int msg_validate_recv(task_state_t *task, msg_recv_t *msg) {
   if(!task_validate_mem(task, msg, sizeof(msg_recv_t), 1))
      return MSG_ERR_INVALID_MSG;
   if(!task_validate_mem(task, msg->buffer, msg->buffer_size, 1))
      return MSG_ERR_INVALID_BUF;
   if(msg->handle_count > MSG_MAX_HANDLES)
      return MSG_ERR_TOO_MANY_HANDLES;
   if(msg->handle_count < 0)
      return MSG_ERR_INVALID_MSG;
   if(!task_validate_mem(task, msg->handles, sizeof(handle_t)*msg->handle_count, 1))
      return MSG_ERR_INVALID_HANDLES;
   return 0;
}

// remove msg from queue after read (conn is reader)
static void msg_dequeue(msg_channel_conn_t *conn) {
   msg_queue_t *queue = &conn->queue;
   msg_obj_t *msg = &queue->slots[queue->head];
   msg_channel_t *channel = conn->channel;
   msg_channel_conn_t *peer_conn = &channel->conns[conn->server];
   msg_port_t *port = channel->port;

   if(conn->server && port && port->client_reserves && (msg->flags & MSG_EXPECT_REPLY)) {
      // slot stays reserved until server replies
      channel->server_unanswered++;
   }

   // remove from queue
   queue->head = (queue->head + 1) % MSG_QUEUE_DEPTH;
   queue->count--;

   // see if we've unblocked the other side by reading
   if(peer_conn->blocked) {
      task_state_t *unpause_task = msg_get_task(peer_conn->taskid, peer_conn->taskuid);
      if(unpause_task)
         task_resume(unpause_task);
      peer_conn->blocked = false;
   }
}

static int msg_deliver_queued(registers_t *regs, task_state_t *task, msg_channel_conn_t *conn, msg_recv_t *recv_msg, msg_recv_t *recv_msg_user) {
   msg_queue_t *queue = &conn->queue;
   msg_obj_t *slot = &queue->slots[queue->head];
   int write_sz = min(slot->length, recv_msg->buffer_size);
   if(copy_to_task(task->task_id, recv_msg->buffer, slot->data, write_sz) != write_sz)
      return MSG_ERR_INVALID_BUF;

   uint32_t message_flags = slot->flags | (queue->count == 1 ? MSG_LAST_MSG : 0);

   msg_copy_receipt_t r = {.bytes = write_sz, .handles = 0, .receiver_status = 0, .sender_status = 0, .conn_h = conn->handle};
   msg_send_t send_msg = {.size = slot->length, .handle_count = 0};
   if(!msg_write_receipt(task, recv_msg_user, &send_msg, &r, 0, 0, message_flags))
      return MSG_ERR_INVALID_MSG; // invalid recv_msg

   // success
   msg_dequeue(conn);

   regs->ebx = r.bytes;
   return 0;
}

static int msg_deliver_flags(registers_t *regs, task_state_t *task, handle_t conn_h, uint32_t flags, msg_recv_t *recv_msg) {
   msg_copy_receipt_t r = {.bytes = 0, .handles = 0, .receiver_status = 0, .sender_status = 0, .conn_h = conn_h};
   msg_send_t send_msg = {.size = 0, .handle_count = 0};
   if(!msg_write_receipt(task, recv_msg, &send_msg, &r, 0, flags, 0))
      return MSG_ERR_INVALID_MSG; // invalid recv_msg
   // success
   regs->ebx = 0;
   return 0;
}

// receive on all channels in port
int msg_receive(registers_t *regs, task_state_t *task, handle_t port_h, msg_recv_t *recv_msg) {
   int err = msg_validate_recv(task, recv_msg);
   if(err < 0)
      return err;

   kobj_ref_t *ref = process_get_kobj_type(task->process, port_h, KOBJ_PORT);
   if(!ref)
      return MSG_ERR_HANDLE_FAIL;

   msg_port_t *port = ref->inst->data;

   // check port signal flags
   if(port->signal_flags) {
      int r = msg_deliver_flags(regs, task, 0, port->signal_flags, recv_msg);
      if(r >= 0)
         port->signal_flags = 0;
      return r;
   }

   // check for stored send
   for(int i = 0; i < port->channel_count; i++) {
      msg_channel_t *channel = port->channels[i];
      if(!channel) continue;

      msg_channel_conn_t *conn = &channel->conns[0];
      msg_channel_conn_t *client_conn = &channel->conns[1];
      if(!conn->connected)
         continue;

      // check for queued
      if(conn->queue.count > 0)
         return msg_deliver_queued(regs, task, conn, recv_msg, recv_msg);

      // check for waiting sync request
      task_state_t *client_task = msg_get_task(client_conn->taskid, client_conn->taskuid);
      if(client_task && channel->call_pending) {
         msg_send_t *client_send_msg = &channel->client_send_msg;
         msg_send_t send_msg = *client_send_msg; // local copy
         // clear stored call
         client_send_msg->buffer = NULL;
         client_send_msg->handle_count = 0;
         channel->call_pending = false;
         uint32_t call_uid = call_uid_counter++;

         msg_copy_receipt_t r = msg_do_copy(client_task, task, &send_msg, recv_msg, recv_msg, call_uid, conn->handle);
         if(r.sender_status < 0) {
            // copy failed - wake client/sender with error
            client_task->registers.ebx = r.sender_status;
            task_resume(client_task);
            if(r.receiver_status < 0)
               return r.receiver_status;
            continue; // check for next send (server stays waiting)
         }

         // success (match api_msg_receive expected regs)
         channel->call_uid = call_uid;
         regs->ebx = r.bytes;

         return 0;
      }

      // check channel flags
      if(conn->flags) {
         int r = msg_deliver_flags(regs, task, conn->handle, conn->flags, recv_msg);
         if(r >= 0)
            conn->flags = 0;
         return r;
      }
   }

   // nothing to receive yet, wait until call
   if(!msg_add_waiter(port, task, recv_msg)) {
      debug_printf("msg_receive: no free waiter slot on port %u\n", port_h);
      return MSG_ERR_LIMIT;
   }
   task_pause(task, PAUSE_MSG_RECEIVE);
   task_interrupt_set(-1, true); // yield once the syscall finishes

   return 0;
}

static msg_err_t msg_get_peer_error(msg_err_t err) {
   if(err == MSG_ERR_INVALID_BUF)
      return MSG_ERR_PEER_INVALID_BUF;
   else if(err == MSG_ERR_INVALID_HANDLES)
      return MSG_ERR_PEER_INVALID_HANDLES;
   else if(err == MSG_ERR_TOO_MANY_HANDLES)
      return MSG_ERR_PEER_TOO_MANY_HANDLES;
   return MSG_ERR_PEER_INVALID_MSG;
}

int msg_reply(registers_t *regs, task_state_t *task, handle_t conn_h, uint32_t call_uid, msg_send_t *send_msg) {
   if(call_uid == 0)
      return MSG_ERR_MSG_NOT_FOUND;
   
   process_t *process = task->process;
   kobj_ref_t *ref = process_get_kobj_type(process, conn_h, KOBJ_CHANNEL_CONN);
   if(!ref)
      return MSG_ERR_HANDLE_FAIL;
   
   msg_channel_conn_t *conn = ref->inst->data;
   if(!conn->server)
      return MSG_ERR_NO_PRIVILEGE;

   msg_channel_t *channel = conn->channel;

   // validate call id
   if(channel->call_uid != call_uid)
      return MSG_ERR_INVALID_CALLID;

   channel->call_uid = 0;

   msg_channel_conn_t *peer_conn = &channel->conns[1];
   task_state_t *client_task = msg_get_task(peer_conn->taskid, peer_conn->taskuid);
   if(!client_task || client_task->pause_reason !=  PAUSE_MSG_CALL)
      return MSG_ERR_PEER_DISCONNECTED;

   // client is (also) resumed with error code for any failure
   registers_t *client_regs = &client_task->registers;

   int err = msg_validate_send(task, send_msg);
   if(err < 0) {
      client_regs->ebx = msg_get_peer_error(err);
      task_resume(client_task);
      return err;
   }

   if(!msg_check_handles(task->process, task->process == client_task->process, send_msg->handles, send_msg->handle_count)) {
      debug_printf("msg_reply: invalid handle\n");
      client_regs->ebx = MSG_ERR_PEER_INVALID_HANDLES;
      task_resume(client_task);
      return MSG_ERR_INVALID_HANDLES;
   }

   msg_recv_t *recv_msg = &channel->client_recv_msg;
   msg_copy_receipt_t r = msg_do_copy(task, client_task, send_msg, recv_msg, channel->client_recv_msg_user, 0, peer_conn->handle);
   if(r.sender_status < 0) {
      if(r.receiver_status < 0)
         client_regs->ebx = r.receiver_status;
      else
         client_regs->ebx = msg_get_peer_error(r.sender_status);
      task_resume(client_task);
      return r.sender_status; // wake server with error
   }

   // success
   client_regs->ebx = r.bytes;
   regs->ebx = r.bytes;
   task_resume(client_task);
   task_interrupt_set(client_task->task_id, true); // run client once the syscall finishes

   return 0;
}

// wait until recipient has read msg, i.e. when there's space to send another msg in queue
int msg_wait_on_receive(task_state_t *task, handle_t conn_h) {
   process_t *process = task->process;
   kobj_ref_t *ref = process_get_kobj_type(process, conn_h, KOBJ_CHANNEL_CONN);
   if(!ref)
      return MSG_ERR_HANDLE_FAIL;
   
   msg_channel_conn_t *conn = ref->inst->data;
   msg_channel_t *channel = conn->channel;
   msg_channel_conn_t *peer_conn = &channel->conns[conn->server];

   if(peer_conn->queue.count == 0) // nothing for peer to receive
      return MSG_ERR_EMPTY_MSG;
   if(!peer_conn->connected)
      return MSG_ERR_PEER_DISCONNECTED;

   conn->blocked = true;

   task_pause(task, PAUSE_MSG_WRITE);
   return true;
}

int port_receive(task_state_t *task, handle_t conn_h, uint8_t *buffer, uint32_t size, uint32_t *channel_flags, uint32_t *msg_flags) {
   process_t *process = task->process;
   kobj_ref_t *ref = process_get_kobj_type(process, conn_h, KOBJ_CHANNEL_CONN);
   if(!ref)
      return MSG_ERR_HANDLE_FAIL;
   
   msg_channel_conn_t *conn = ref->inst->data;
   
   if(!conn->connected) {
      debug_printf("port_receive: host has disconnected from channel\n");
      return MSG_ERR_DISCONNECTED;
   }

   msg_queue_t *queue = &conn->queue;
   
   if(queue->count == 0) {
      // not a failure so still return flags to caller
      *channel_flags = conn->flags;
      conn->flags = 0; // reset
      *msg_flags = 0; // no msg so no flags
      return 0;
   }

   msg_obj_t *msg = &queue->slots[queue->head];
   if(msg->length > size) {
      debug_printf("port_receive: receive buffer is too small for message size %i\n", msg->length);
      return MSG_ERR_BUF_TOO_SMALL;
   }

   memcpy(buffer, msg->data, msg->length);
   *msg_flags = msg->flags;

   msg_dequeue(conn);

   if(queue->count == 0)
      *msg_flags |= MSG_LAST_MSG; // last message in queue

   // set flags for caller
   *channel_flags = conn->flags;
   conn->flags = 0; // reset
   return msg->length; // > 0 (enforced by send func)
}

// mark closed/disconnected, report close to peer
void port_close_connection(msg_channel_conn_t *conn) {
   if(!conn->connected) return;
   conn->connected = false;

   msg_channel_t *channel = conn->channel;

   // notify peer
   msg_channel_conn_t *peer_conn = &channel->conns[conn->server];
   if(!peer_conn->connected) return;

   task_state_t *peer_task = msg_get_task(peer_conn->taskid, peer_conn->taskuid);
   if(!peer_task) return;

   peer_conn->flags |= MSG_FLAG_CLOSED;

   if(peer_conn->blocked) {
      task_resume(peer_task);
      peer_conn->blocked = false;
   }

   // fail a sync call that is stored or awaiting reply
   if(peer_task->paused && peer_task->pause_reason == PAUSE_MSG_CALL
   && (channel->call_pending || channel->call_uid)) {
      peer_task->registers.ebx = MSG_ERR_PEER_DISCONNECTED;
      task_resume(peer_task);
   }

   msg_notify_status_t notified = call_msg_func(NULL, peer_task, peer_conn->handle, peer_conn->flags);
   msg_set_notify_pending(peer_task, peer_conn, notified == MSG_NOTIFY_DROPPED);
   if(notified == MSG_NOTIFY_DELIVERED)
      peer_conn->flags = 0; // reset

   channel->call_pending = false;
   channel->call_uid = 0;

   // deliver close flag to sync waiter
   if(peer_conn->server && !peer_task->msg_func)
      msg_deliver_to_waiter(channel->port, channel);
}

void port_stop_connection(kobj_inst_t *inst) {
   msg_channel_conn_t *conn = inst->data;
   port_close_connection(conn);
}

// releasing connection
// free entire channel when both sides are released
void port_free_connection(kobj_inst_t *inst) {
   msg_channel_conn_t *conn = inst->data;
   msg_channel_t *channel = conn->channel;
   msg_channel_conn_t *peer_conn = &channel->conns[conn->server];
   conn->released = true;
   if(!peer_conn->released) return;
   // free channel and drop from channels list
   msg_port_t *port = channel->port;
   if(port) { // already cleared in port_stop
      for(int i = 0; i < port->channel_count; i++) {
         if(port->channels[i] != channel) continue;
         port->channels[i] = NULL;
      }
   }
   kfree(channel);
}

// close all connections (notifies clients)
void port_stop(kobj_inst_t *inst) {
   msg_port_t *port = inst->data;

   for(int i = 0; i < port->channel_count; i++) {
      msg_channel_t *channel = port->channels[i];
      if(!channel) continue;
      port_close_connection(&channel->conns[0]);
      port->channels[i] = NULL; // ensure hrelease doesn't free it
      channel->port = NULL;
      // task should release handle
   }

   // wake server threads blocked in msg_receive on this port
   for(int i = 0; i < MAX_TASK_THREADS; i++) {
      msg_waiter_t *waiter = &port->waiters[i];
      if(!waiter->active) continue;
      waiter->active = false;
      task_state_t *t = msg_get_task(waiter->task_id, waiter->task_uid);
      if(!t || !t->paused || t->pause_reason != PAUSE_MSG_RECEIVE) continue;
      t->registers.ebx = MSG_ERR_PORT_NOT_FOUND;
      task_resume(t);
   }
}

void port_free(kobj_inst_t *inst) {
   msg_port_t *port = inst->data;
   for(msg_port_t **link = &ports; *link; link = &(*link)->next) {
      if(*link == port) {
         *link = port->next;
         break;
      }
   }
   kfree(port);
}

void msg_retry_task_notifications(task_state_t *task) {
   if(!task->msg_notify_pending) return;
   task->msg_notify_pending = false;

   for(int i = 0; i < PROCESS_MAX_KOBJ; i++) {
      kobj_ref_t *ref = task->process->kobj[i];
      if(!ref || ref->inst->obj->type != KOBJ_CHANNEL_CONN) continue;
      msg_channel_conn_t *conn = ref->inst->data;
      if(conn->taskuid != task->task_uid || !conn->notify_pending) continue;
      msg_notify_status_t notified = call_msg_func(NULL, task, conn->handle, conn->flags);
      msg_set_notify_pending(task, conn, notified == MSG_NOTIFY_DROPPED);
      if(notified == MSG_NOTIFY_DELIVERED)
         conn->flags = 0; // reset
      if(notified == MSG_NOTIFY_DROPPED)
         break;
   }
}

bool msg_conn_has_unread(msg_channel_conn_t *conn) {
   return conn->connected && (conn->queue.count > 0 || conn->flags);
}

// queue a notification for a channel that has unread msgs or uncleared flags
static bool msg_notify_if_unread(task_state_t *task, msg_channel_conn_t *conn) {
   if(!msg_conn_has_unread(conn))
      return false; // nothing to read
   msg_notify_status_t notified = call_msg_func(NULL, task, conn->handle, conn->flags);
   msg_set_notify_pending(task, conn, notified == MSG_NOTIFY_DROPPED);
   if(notified == MSG_NOTIFY_DELIVERED)
      conn->flags = 0;
   return true;
}

// called when setting msg func - check & dispatch queued msgs (including flags)
bool msg_dispatch_queued(task_state_t *task) {
   if(!task->msg_func)
      return false;

   bool found = false;

   // check task's channel connections
   for(int i = 0; i < PROCESS_MAX_KOBJ; i++) {
      kobj_ref_t *ref = task->process->kobj[i];
      if(!ref) continue;
      kobj_inst_t *inst = ref->inst;
      if(inst->obj->type == KOBJ_CHANNEL_CONN) {
         msg_channel_conn_t *conn = inst->data;
         if(conn->taskuid != task->task_uid) continue;
         if(msg_notify_if_unread(task, conn))
            found = true;
      } else if(inst->obj->type == KOBJ_PORT) {
         // signal flags
         msg_port_t *port = inst->data;
         if(port->owner_taskuid != task->task_uid || !port->signal_flags) continue;
         if(call_msg_func(NULL, task, 0, port->signal_flags) == MSG_NOTIFY_DELIVERED)
            port->signal_flags = 0;
         found = true;
      }
   }

   return found;
}

