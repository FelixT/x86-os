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

typedef struct msg_channel_t msg_channel_t;

typedef struct msg_channel_conn_t {
   bool server; // server or client
   int taskid;
   uint32_t taskuid;
   msg_channel_t *channel; // shared channel obj
   bool connected; // closing (stop) disconnects, including port close
   bool released; // released when user explicitly releases handle
   bool blocked;
   uint32_t flags;
   bool notify_pending;
   msg_queue_t queue; // inbound
   handle_t handle; // handle is reported back in notifications
} msg_channel_conn_t;

typedef struct msg_port_t msg_port_t;

typedef struct msg_channel_t {
   msg_port_t *port;
   int server_unanswered; // messages server has read but not replied to (client_reserves)
   msg_channel_conn_t conns[2]; // connections, 0 = server, 1 = client (conns[!server])

   // sync
   uint32_t call_uid;
   bool call_pending; // client_send_msg holds a call the server hasn't received yet
   msg_send_t client_send_msg; // store sent msg until server receives
   msg_recv_t client_recv_msg; // stored reply msg until server replies
   msg_recv_t *client_recv_msg_user; // client's msg_recv_t* (in its address space), receipt written on reply
} msg_channel_t;

typedef struct msg_waiter_t {
   bool active;
   int task_id;
   uint32_t task_uid;
   msg_recv_t recv_msg;
   msg_recv_t *recv_msg_user; // server's msg_recv_t* (in its address space), receipt written on call
} msg_waiter_t;

// a port represents a message queue
typedef struct msg_port_t {
   char name[MSG_PORT_NAME_LEN];
   uint32_t owner_uid; // server process uid
   uint32_t owner_taskid;
   uint32_t owner_taskuid;
   msg_channel_t *channels[MSG_MAX_CHANNELS];
   int channel_count;
   bool client_reserves; // clients sending MSG_EXPECT_REPLY are required to reserve a queue spot for the response (ie to stop the server being blocked)
   uint32_t signal_flags;
   // sync
   msg_waiter_t waiters[MAX_TASK_THREADS];
   
   struct msg_port_t *next;
} msg_port_t;

typedef struct msg_copy_receipt_t {
   int bytes; // bytes copied
   int handles; // handles copied
   int sender_status; // 0 for no error, < 0 for msg_err_t
   int receiver_status; // 0 for no error
   handle_t conn_h;
} msg_copy_receipt_t;

msg_port_t *find_port_by_name(char *name);
int create_port(task_state_t *task, char *name, bool client_reserves, handle_t *port_h);
void port_free(kobj_inst_t *inst);
void port_stop(kobj_inst_t *inst);
int port_connect(task_state_t *task, char *name, handle_t *conn_h);
int port_send(registers_t *regs, task_state_t *task, handle_t conn_h, msg_send_t *send_msg, uint32_t flags);
int port_receive(task_state_t *task, handle_t conn_h, uint8_t *buffer, uint32_t size, uint32_t *channel_flags, uint32_t *msg_flags);
int msg_wait_on_receive(task_state_t *task, handle_t conn_h);
void port_stop_connection(kobj_inst_t *inst);
void port_free_connection(kobj_inst_t *inst);
void msg_retry_task_notifications(task_state_t *task);
bool msg_notif_is_stale(task_state_t *task, handle_t conn_h);
int msg_call(task_state_t *task, handle_t conn_h, msg_send_t *send_msg, msg_recv_t *recv_msg);
int msg_receive(registers_t *regs, task_state_t *task, handle_t port_h, msg_recv_t *recv_msg);
int msg_reply(registers_t *regs, task_state_t *task, handle_t conn_h, uint32_t call_uid, msg_send_t *send_msg);
int msg_validate_send(task_state_t *task, msg_send_t *msg);
int msg_validate_recv(task_state_t *task, msg_recv_t *msg);
bool msg_dispatch_queued(task_state_t *task);
int port_signal(msg_port_t *port, uint32_t flag);

#endif
