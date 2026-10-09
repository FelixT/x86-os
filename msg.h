#ifndef MSG_H
#define MSG_H

#include "tasks.h"
#include "lib/api.h"

#define MSG_MAX_CHANNELS 16 // per port
#define MSG_MAX_LEN (128-2*sizeof(uint16_t))
#define MSG_QUEUE_DEPTH 14

typedef struct msg_obj_t {
   uint16_t length;
   uint16_t flags; // MSG_EXPECT_REPLY
   uint8_t data[MSG_MAX_LEN];
} __attribute__((packed)) msg_obj_t; // 128 bytes

typedef struct msg_queue_t {
   msg_obj_t slots[MSG_QUEUE_DEPTH];
   int head;
   int tail;
   int count;
   int reserved;
} msg_queue_t;

// bidirectional channel between two tasks, one of which is the server (port owner)
#define MSG_QUEUE_CLIENT_TO_SERVER 0
#define MSG_QUEUE_SERVER_TO_CLIENT 1

typedef struct msg_channel_t {
   uint32_t uid;
   uint32_t port_uid;
   uint32_t client_uid; // creator/client process uid (unused)
   uint32_t client_taskid;
   uint32_t client_taskuid;
   uint32_t server_taskid;
   uint32_t server_taskuid;
   bool server_connected;
   bool client_connected;
   msg_queue_t queues[2]; // 0 = client->server, 1 = server->client
   int server_unanswered; // messages server has read but not replied to (client_reserves)
   bool client_blocked; // waiting to write
   bool server_blocked;
   uint32_t client_flags; // used to send notifications to reader, cleared on receive
   uint32_t server_flags;
   bool client_notify_pending; // msg_func couldn't be called (recipient event queue full) - retry when a slot frees
   bool server_notify_pending;
   // sync
   uint32_t call_uid;
   msg_send_t client_send_msg; // store sent msg until server receives
   msg_recv_t client_recv_msg; // stored reply msg until server replies
} msg_channel_t;

typedef struct msg_waiter_t {
   bool active;
   int task_id;
   uint32_t task_uid;
   msg_recv_t recv_msg;
} msg_waiter_t;

// a port represents a message queue
typedef struct msg_port_t {
   uint32_t uid;
   char name[MSG_PORT_NAME_LEN];
   uint32_t owner_uid; // server process uid
   uint32_t owner_taskid;
   uint32_t owner_taskuid;
   msg_channel_t *channels[MSG_MAX_CHANNELS];
   int channel_count;
   bool client_reserves; // clients sending MSG_EXPECT_REPLY are required to reserve a queue spot for the response (ie to stop the server being blocked)
   // sync
   msg_waiter_t waiters[MAX_TASK_THREADS];
   
   struct msg_port_t *next;
} msg_port_t;

uint32_t create_port(task_state_t *task, char *name, bool client_reserves);
uint32_t port_connect(task_state_t *task, char *name, uint32_t *port_uid);
int port_send(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid, msg_send_t *send_msg, uint32_t flags);
int port_receive(task_state_t *task, uint32_t port_uid, uint32_t channel_uid, uint8_t *buffer, uint32_t size, uint32_t *channel_flags, uint32_t *msg_flags);
bool msg_wait_on_receive(task_state_t *task, uint32_t port_uid, uint32_t channel_uid);
bool close_port(registers_t *regs, task_state_t *task, uint32_t port_uid);
bool port_disconnect(registers_t *regs, task_state_t *task, uint32_t port_uid, uint32_t channel_uid);
void msg_retry_notifications(task_state_t *task);
void msg_cleanup_process(process_t *process);
void msg_cleanup_task(task_state_t *task);
bool msg_notif_is_stale(task_state_t *task, uint32_t port_uid, uint32_t channel_uid);
int msg_call(registers_t *regs, task_state_t *task, uint32_t channel_uid, msg_send_t *send_msg, msg_recv_t *recv_msg);
int msg_receive(registers_t *regs, task_state_t *task, uint32_t port_uid, msg_recv_t *recv_msg);
int msg_reply(registers_t *regs, task_state_t *task, uint32_t call_uid, msg_send_t *send_msg);
int msg_validate_send(msg_send_t *msg);
int msg_validate_recv(msg_recv_t *msg);

#endif
