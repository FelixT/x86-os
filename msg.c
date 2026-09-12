#include "msg.h"

#include "stdbool.h"
#include "stdint.h"
#include "windowmgr.h" // for debug

#include "fs.h"

// message passing

// allows passing of small messages between tasks asynchronously with queues

// todo: sync/blocking messages with timeout
// todo: large messages use shared memory (cow)

uint32_t msg_uid_counter = 0;
uint32_t channel_uid_counter = 0;

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

msg_port_t *find_port_by_uid(uint32_t uid) {
   msg_port_t *cur = ports;
   while(cur) {
      if(cur->uid == uid)
         return cur;
      cur = cur->next;
   }
   return NULL;
}

uint32_t create_port(task_state_t *task, char *name, bool client_reserves) {
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
   if(process->port_count >= PROCESS_MAX_PORTS) {
      debug_printf("create_port: process %u has too many ports\n", process->uid);
      return MSG_ERR_LIMIT;
   }
   if(msg_uid_counter >= MSG_ERR_BASE) {
      debug_printf("create_port: port uid space exhausted\n");
      return MSG_ERR_NOMEM;
   }

   msg_port_t *port = malloc(sizeof(msg_port_t));
   if(!port)
      return MSG_ERR_NOMEM;

   process->ports[process->port_count++] = port;

   port->uid = msg_uid_counter++;
   strncpy(port->name, name, sizeof(port->name)-1);
   port->owner_uid = process->uid;
   port->owner_taskid = task->task_id;
   port->owner_taskuid = task->task_uid;
   for(int i = 0; i < MSG_MAX_CHANNELS; i++)
      port->channels[i] = NULL;
   port->channel_count = 0;
   port->client_reserves = client_reserves;
   port->next = ports;
   ports = port;
   return port->uid;
}

int find_channel(msg_port_t *port, uint32_t channel_uid) {
   for(int i = 0; i < port->channel_count; i++) {
      msg_channel_t *channel = port->channels[i];
      if(channel && channel->uid == channel_uid)
         return i;
   }
   return -1;
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

uint32_t port_connect(task_state_t *task, char *name, uint32_t *port_uid) {
   process_t *process = task->process;
   msg_port_t *port = find_port_by_name(name);
   if(!port) {
      debug_printf("port_connect: couldn't find port '%s'\n", name);
      return MSG_ERR_PORT_NOT_FOUND;
   }

   *port_uid = port->uid;

   if(port->owner_taskuid == task->task_uid) {
      debug_printf("port_connect: task %u of process %u can't connect to its own port\n", task->task_uid, process->uid);
      return MSG_ERR_SELF;
   }

   if(task->msg_channel_count >= TASK_MAX_CHANNELS) {
      debug_printf("port_connect: task %u has too many channels\n", task->task_uid);
      return MSG_ERR_LIMIT;
   }

   if(channel_uid_counter >= MSG_ERR_BASE) {
      debug_printf("port_connect: channel uid space exhausted\n");
      return MSG_ERR_NOMEM;
   }

   if(!port_free_channel_exists(port)) {
      debug_printf("port_connect: no free channels\n");
      return MSG_ERR_PORT_FULL;
   }

   msg_channel_t *channel = malloc(sizeof(msg_channel_t));
   if(!channel) {
      debug_printf("port_connect: failed to allocate channel\n");
      return MSG_ERR_NOMEM;
   }

   int c = port_claim_free_channel_index(port);
   if(c < 0) {
      debug_printf("port_connect: failed to claim free channel index\n");
      free((uint32_t)channel, sizeof(msg_channel_t));
      return MSG_ERR_PORT_FULL;
   }
   port->channels[c] = channel;
   task->msg_channels[task->msg_channel_count++] = channel;

   channel->uid = channel_uid_counter++;
   channel->port_uid = port->uid;
   channel->client_uid = process->uid;
   channel->server_taskuid = port->owner_taskuid;
   channel->server_taskid = port->owner_taskid;
   channel->client_taskuid = task->task_uid;
   channel->client_taskid = task->task_id;
   channel->queues[0].count = 0;
   channel->queues[0].reserved = 0;
   channel->queues[1].count = 0;
   channel->queues[1].reserved = 0;
   channel->server_unanswered = 0;
   channel->client_flags = 0;
   channel->server_flags = 0;
   channel->client_blocked = false;
   channel->server_blocked = false;
   channel->client_notify_pending = false;
   channel->server_notify_pending = false;
   channel->server_connected = true;
   channel->client_connected = true;
   return channel->uid;
}

typedef enum {
   MSG_NOTIFY_DELIVERED, // msg_func called or queued
   MSG_NOTIFY_NO_CALLBACK, // no msg_func (recipient can poll or snooze instead)
   MSG_NOTIFY_DROPPED // msg_func couldn't be queued - retry
} msg_notify_status_t;

static msg_notify_status_t call_msg_func(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid, uint32_t flags) {
   if(!task->msg_func) {
      task_wake(task);
      return MSG_NOTIFY_NO_CALLBACK;
   }
   uint32_t *args = malloc(sizeof(uint32_t) * 3); // todo: avoid allocating each call
   if(!args) return MSG_NOTIFY_DROPPED;
   args[2] = port_uid;
   args[1] = channel_uid;
   args[0] = flags;
   bool delivered;
   if(regs)
      delivered = task_call_subroutine(regs, task, "msg", (uint32_t)task->msg_func, args, 3);
   else
      delivered = task_queue_subroutine(task, "msg", (uint32_t)task->msg_func, args, 3);

   return delivered ? MSG_NOTIFY_DELIVERED : MSG_NOTIFY_DROPPED;
}

static void msg_set_notify_pending(task_state_t *task, msg_channel_t *channel, bool server, bool pending) {
   if(server)
      channel->server_notify_pending = pending;
   else
      channel->client_notify_pending = pending;
   if(pending)
      task->msg_notify_pending = true;
}

static task_state_t *msg_get_task(int task_id, uint32_t task_uid) {
   if(task_id < 0 || task_id >= TOTAL_TASKS) return NULL;
   task_state_t *task = &gettasks()[task_id];
   if(!task->enabled || task->task_uid != task_uid) return NULL;
   return task;
}

int port_send(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid, uint8_t *buffer, uint32_t length, uint32_t flags) {
   process_t *process = task->process;
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) {
      debug_printf("port_send: couldn't find port %u\n", port_uid);
      return MSG_ERR_PORT_NOT_FOUND;
   }
   int c = find_channel(port, channel_uid);
   if(c < 0) {
      debug_printf("port_send: couldn't find channel %u\n", channel_uid);
      return MSG_ERR_CHANNEL_NOT_FOUND;
   }

   if(length > MSG_MAX_LEN) {
      debug_printf("port_send: msg length %i too long (max %i)\n", length, MSG_MAX_LEN);
      return MSG_ERR_TOO_LONG; // fail instead of truncate
   }

   if(length == 0) {
      debug_printf("port_send: empty messages are invalid\n");
      return MSG_ERR_INVALID_MSG;
   }

   msg_channel_t *channel = port->channels[c];

   if(task->task_uid != channel->server_taskuid && task->task_uid != channel->client_taskuid) {
      debug_printf("port_send: task %u of process %u can't access channel %u\n", task->task_uid, process->uid, channel_uid);
      return MSG_ERR_DENIED;
   }

   bool server_to_client = task->task_uid == channel->server_taskuid;
   bool reserve_slot = port->client_reserves && !server_to_client && (flags & MSG_EXPECT_REPLY);
   // use explicit flag for replies to avoid consuming for unrelated sends
   bool writing_in_reserved = port->client_reserves && server_to_client && (flags & MSG_REPLY) && channel->server_unanswered > 0;

   if((server_to_client && !channel->client_connected) || (!server_to_client && !channel->server_connected)) {
      debug_printf("port_send: peer has disconnected\n");
      return MSG_ERR_PEER_DISCONNECTED;
   }

   if((server_to_client && !channel->server_connected) || (!server_to_client && !channel->client_connected)) {
      debug_printf("port_send: sender has disconnected from channel\n");
      return MSG_ERR_DISCONNECTED;
   }

   msg_queue_t *queue = &channel->queues[server_to_client];
   if(queue->count + queue->reserved - (writing_in_reserved ? 1 : 0) >= MSG_QUEUE_DEPTH) {
      // full
      debug_printf("port_send: queue is full\n");
      // send fails - notify sending task, and allow them to decide what to do: drop or wait for queue to free up
      return MSG_ERR_QUEUE_FULL;
   }

   uint32_t task_uid = server_to_client ? channel->client_taskuid : channel->server_taskuid;
   int task_id = server_to_client ? channel->client_taskid : channel->server_taskid;

   task_state_t *recipient_task = msg_get_task(task_id, task_uid);
   if(!recipient_task) {
      debug_printf("port_send: invalid/ended recipient task\n");
      return MSG_ERR_INVALID_TASK;
   }

   if(reserve_slot) {
      msg_queue_t *recv_queue = &channel->queues[1];
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
   memcpy(obj->data, buffer, length);
   obj->length = length;
   obj->flags = flags & (MSG_EXPECT_REPLY | MSG_REPLY);

   msg_notify_status_t notified = call_msg_func(regs, recipient_task, port_uid, channel_uid, server_to_client ? channel->client_flags : channel->server_flags);
   if(notified == MSG_NOTIFY_DROPPED)
      debug_printf("port_send: recipient event queue full, deferring notification\n");
   msg_set_notify_pending(recipient_task, channel, !server_to_client, notified == MSG_NOTIFY_DROPPED);
   if(notified == MSG_NOTIFY_DELIVERED) {
      if(server_to_client)
         channel->client_flags = 0;
      else
         channel->server_flags = 0;
   }

   return 0; // success
}

// wait until recipient has read msg, i.e. when there's space to send another msg in queue
bool msg_wait_on_receive(task_state_t *task, uint32_t port_uid, uint32_t channel_uid) {
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) return false;
   int c = find_channel(port, channel_uid);
   if(c < 0) return false;
   msg_channel_t *channel = port->channels[c];
   if(!channel) return false;

   if(task->task_uid != channel->client_taskuid && task->task_uid != channel->server_taskuid)
      return false;

   bool server = task->task_uid == channel->server_taskuid;

   if(channel->queues[server].count == 0
   || (server && !channel->client_connected) || (!server && !channel->server_connected)) {
      // nothing to receive/peer closed
      return false;
   }

   if(task->task_uid == channel->client_taskuid)
      channel->client_blocked = true;
   else
      channel->server_blocked = true;

   task_pause(task, PAUSE_MSG_WRITE);
   return true;
}

int port_receive(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid, uint8_t *buffer, uint32_t size, uint32_t *channel_flags, uint32_t *msg_flags) {
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) {
      debug_printf("port_receive: couldn't find port %u\n", port_uid);
      return MSG_ERR_PORT_NOT_FOUND;
   }
   int c = find_channel(port, channel_uid);
   if(c < 0) {
      debug_printf("port_receive: couldn't find channel %u\n", channel_uid);
      return MSG_ERR_CHANNEL_NOT_FOUND;
   }
   
   msg_channel_t *channel = port->channels[c];

   if(task->task_uid != channel->client_taskuid && task->task_uid != channel->server_taskuid) {
      debug_printf("port_receive: task %u can't access channel %u\n", task->task_uid, channel_uid);
      return MSG_ERR_DENIED;
   }

   bool server = task->task_uid == channel->server_taskuid;

   if((server && !channel->server_connected) || (!server && !channel->client_connected)) {
      debug_printf("port_receive: host has disconnected from channel\n");
      return MSG_ERR_DISCONNECTED;
   }

   msg_queue_t *queue = &channel->queues[!server];
   
   if(queue->count == 0) {
      // not a failure so still return flags to caller
      if(server) {
         *channel_flags = channel->server_flags;
         channel->server_flags = 0; // reset
      } else {
         *channel_flags = channel->client_flags;
         channel->client_flags = 0;
      }
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

   if(server && port->client_reserves && (msg->flags & MSG_EXPECT_REPLY)) {
      // slot stays reserved until server replies
      channel->server_unanswered++;
   }

   // remove from queue
   queue->head = (queue->head + 1) % MSG_QUEUE_DEPTH;
   queue->count--;

   // see if we've unblocked the other side by reading
   if(server && channel->client_blocked) {
      task_state_t *unpause_task = &gettasks()[channel->client_taskid];
      if(unpause_task->enabled && unpause_task->task_uid == channel->client_taskuid) {
         task_resume(unpause_task);
         task_execute_queued_subroutine(regs, unpause_task->task_id);
      }
      channel->client_blocked = false;
   }
   if(!server && channel->server_blocked) {
      task_state_t *unpause_task = &gettasks()[channel->server_taskid];
      if(unpause_task->enabled && unpause_task->task_uid == channel->server_taskuid) {
         task_resume(unpause_task);
         task_execute_queued_subroutine(regs, unpause_task->task_id);
      }
      channel->server_blocked = false;
   }

   if(queue->count == 0)
      *msg_flags |= MSG_LAST_MSG; // last message in queue

   // set flags for caller
   if(server) {
      *channel_flags = channel->server_flags;
      channel->server_flags = 0; // reset
   } else {
      *channel_flags = channel->client_flags;
      channel->client_flags = 0;
   }

   return msg->length; // > 0 (enforced by send func)
}

// remove from task list of channels
static void task_forget_channel(task_state_t *task, msg_channel_t *channel) {
   for(int i = 0; i < task->msg_channel_count; i++) {
      if(task->msg_channels[i] != channel) continue;
      task->msg_channels[i] = task->msg_channels[task->msg_channel_count-1];
      task->msg_channel_count--;
      return;
   }
}

static void process_forget_port(process_t *process, msg_port_t *port) {
   for(int i = 0; i < process->port_count; i++) {
      if(process->ports[i] != port) continue;
      process->ports[i] = process->ports[process->port_count-1];
      process->port_count--;
      return;
   }
}

static void msg_free_channel(msg_port_t *port, int c) {
   msg_channel_t *channel = port->channels[c];
   if(!channel) return;

   task_state_t *client = msg_get_task(channel->client_taskid, channel->client_taskuid);
   if(client)
      task_forget_channel(client, channel);

   port->channels[c] = NULL;
   free((uint32_t)channel, sizeof(msg_channel_t));
}

bool port_close_connection(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid) {
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) return false;
   int c = find_channel(port, channel_uid);
   if(c < 0) return false;
   msg_channel_t *channel = port->channels[c];
   if(!channel) return false;

   if(task != NULL && task->task_uid != channel->server_taskuid && task->task_uid != channel->client_taskuid)
      return false;

   if(task == NULL || task->task_uid == channel->server_taskuid) {
      // server closed connection, notify client
      channel->client_flags |= MSG_FLAG_CLOSED;
      task_state_t *client_task = msg_get_task(channel->client_taskid, channel->client_taskuid);
      if(channel->client_connected && client_task) {
         if(channel->client_blocked) {
            task_resume(client_task);
            channel->client_blocked = false;
         }

         msg_notify_status_t notified = call_msg_func(regs, client_task, port_uid, channel_uid, channel->client_flags);
         msg_set_notify_pending(client_task, channel, false, notified == MSG_NOTIFY_DROPPED);
         if(notified == MSG_NOTIFY_DELIVERED)
            channel->client_flags = 0; // reset
      }
      channel->server_connected = false;
   } else {
      // client closed, notify server
      channel->server_flags |= MSG_FLAG_CLOSED;
      task_state_t *server_task = msg_get_task(channel->server_taskid, channel->server_taskuid);
      if(channel->server_connected && server_task) {
         if(channel->server_blocked) {
            task_resume(server_task);
            channel->server_blocked = false;
         }

         msg_notify_status_t notified = call_msg_func(regs, server_task, port_uid, channel_uid, channel->server_flags);
         msg_set_notify_pending(server_task, channel, true, notified == MSG_NOTIFY_DROPPED);
         if(notified == MSG_NOTIFY_DELIVERED)
            channel->server_flags = 0; // reset
      }
      channel->client_connected = false;
   }

   if(!channel->server_connected && !channel->client_connected)
      msg_free_channel(port, c); // both sides closed
   return true;
}

bool port_disconnect(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid) {
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) return false;
   int c = find_channel(port, channel_uid);
   if(c < 0) return false;
   msg_channel_t *channel = port->channels[c];
   if(!channel) return false;

   if(task->task_uid != channel->server_taskuid && task->task_uid != channel->client_taskuid)
      return false;

   task_forget_channel(task, channel);

   return port_close_connection(regs, task, port_uid, channel_uid);
}

void destroy_port(registers_t *regs, msg_port_t *port) {
   // close all connections (notifies clients)
   for(int i = 0; i < port->channel_count; i++) {
      msg_channel_t *channel = port->channels[i];
      if(!channel) continue;
      port_close_connection(regs, NULL, port->uid, channel->uid);
      if(!port->channels[i]) continue;
      
      msg_free_channel(port, i);
   }

   // remove from linked list
   if(port == ports) {
      ports = port->next;
   } else {
      msg_port_t *prev = ports;
      msg_port_t *cur = ports->next;
      while(cur) {
         if(cur == port) {
            prev->next = port->next;
            break;
         }
         prev = cur;
         cur = cur->next;
      }
   }

   free((uint32_t)port, sizeof(msg_port_t));
}

bool close_port(registers_t *regs, task_state_t *task, uint32_t port_uid) {
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) return false;
   if(port->owner_uid != task->process->uid) return false; // ports are process resource, any thread can close

   // remove from process port list
   process_forget_port(task->process, port);

   destroy_port(regs, port);

   return true;
}

static void msg_retry_task_notifications(task_state_t *task) {
   if(!task->msg_notify_pending) return;
   task->msg_notify_pending = false;

   // channels this task opened as a client
   for(int i = 0; i < task->msg_channel_count; i++) {
      msg_channel_t *channel = task->msg_channels[i];
      if(!channel || !channel->client_notify_pending) continue;
      msg_notify_status_t notified = call_msg_func(NULL, task, channel->port_uid, channel->uid, channel->client_flags);
      msg_set_notify_pending(task, channel, false, notified == MSG_NOTIFY_DROPPED);
      if(notified == MSG_NOTIFY_DELIVERED)
         channel->client_flags = 0; // reset
   }

   // channels on ports this task owns as a server
   process_t *process = task->process;
   for(int p = 0; p < process->port_count; p++) {
      msg_port_t *port = process->ports[p];
      if(!port || port->owner_taskuid != task->task_uid) continue;
      for(int c = 0; c < port->channel_count; c++) {
         msg_channel_t *channel = port->channels[c];
         if(!channel || !channel->server_notify_pending) continue;
         msg_notify_status_t notified = call_msg_func(NULL, task, port->uid, channel->uid, channel->server_flags);
         msg_set_notify_pending(task, channel, true, notified == MSG_NOTIFY_DROPPED);
         if(notified == MSG_NOTIFY_DELIVERED)
            channel->server_flags = 0; // reset
      }
   }
}

bool msg_notif_is_stale(task_state_t *task, uint32_t port_uid, uint32_t channel_uid) {
   msg_port_t *port = find_port_by_uid(port_uid);
   if(!port) return true;
   int c = find_channel(port, channel_uid);
   if(c < 0) return true;
   msg_channel_t *channel = port->channels[c];
   if(task->task_uid != channel->server_taskuid && task->task_uid != channel->client_taskuid)
      return true;
   bool server = task->task_uid == channel->server_taskuid;

   // receiver has disconnected
   if(!(server ? channel->server_connected : channel->client_connected))
      return true;

   // not stale, still messages to read
   if(channel->queues[!server].count > 0)
      return false;

   // queue is drained, but channel flags still haven't been cleared
   if(server ? channel->server_flags : channel->client_flags)
      return false;

   return true; // no msgs, no flags
}

void msg_retry_notifications(task_state_t *task) {
   process_t *process = task->process;
   for(int i = 0; i < process->no_threads; i++) {
      task_state_t *thread = process->threads[i];
      if(!thread || !thread->enabled || thread->process != process) continue;
      msg_retry_task_notifications(thread);
   }
}

void msg_cleanup_process(process_t *process) {
   for(int i = 0; i < process->port_count; i++) {
      msg_port_t *port = process->ports[i];
      destroy_port(NULL, port);
   }
   process->port_count = 0;
}

void msg_cleanup_task(task_state_t *task) {
   // close channels
   while(task->msg_channel_count > 0) {
      msg_channel_t *channel = task->msg_channels[0];
      int before = task->msg_channel_count;
      if(channel)
         port_close_connection(NULL, task, channel->port_uid, channel->uid);
      // peer still connected or already closed
      if(task->msg_channel_count == before)
         task_forget_channel(task, channel);
   }

   // close ports owned by this thread
   process_t *process = task->process;
   for(int i = process->port_count - 1; i >= 0; i--) {
      msg_port_t *port = process->ports[i];
      if(!port || port->owner_taskuid != task->task_uid) continue;
      process_forget_port(process, port);
      destroy_port(NULL, port);
   }
}
