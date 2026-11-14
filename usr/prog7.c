#include "prog.h"

// multithreading + futex test program

// futex shared between thread1&2
volatile uint32_t flag = 0;

void process() {
   // second process
   write_str("Process launched\n");
   handle_t conn_h;
   int r = port_connect("/prog7", &conn_h);
   if(r < 0)
      exit(1);
   uint8_t buf = 0;
   msg_send_t send_msg = {.buffer = &buf, .size = 1, .handles = NULL, .handle_count = 0};
   handle_t handles[1];
   msg_recv_t recv_msg = {.buffer = NULL, .buffer_size = 0, .handles = handles, .handle_count = 1};
   r = msg_sync_send_obj(conn_h, &send_msg, &recv_msg);
   if(r < 0)
      exit(1);
   
   uint32_t *futex = map(handles[0], 0);
   if(!futex)
      exit(1);

   while(true) {
      while(*futex == 0)
         futex_wait(futex, 0);
      write_str("Process\n");
      *futex = 0;
   }

   exit(0);
}

void thread() {
   while(true) {
      write_str("Second thread\n");
      flag = 1;
      futex_wake((void*)&flag);
      sleep(2000);
   }
}

void thread2() {
   while(true) {
      // wait for thread1
      while(flag == 0)
         futex_wait((void*)&flag, 0);
      write_str("Third thread\n");
      flag = 0;
   }
}

void _start(int argc, char **args) {
   (void)args;
   if(argc > 0) {
      process();
      exit(0);
   }

   char *new_args[1] = {"child"};
   handle_t prog_h;
   int t = launch_task("/sys/prog7.elf", 1, new_args, true, true, &prog_h);

   set_window_title("Prog7");
   create_thread(&thread);
   create_thread(&thread2);

   handle_t h = shared_create(sizeof(uint32_t));
   if(h < 0)
      exit(1);

   uint32_t *futex = map(h, 0);
   if(!futex)
      exit(1);

   *futex = 0;

   handle_t port_h;
   int r = create_port("/prog7", false, &port_h);
   if(r < 0)
      exit(1);
   
   if(!unpause_task(t, prog_h))
      exit(1);
   hrelease(prog_h);

   // wait for process to send
   uint8_t buf;
   uint32_t call_uid, flags;
   handle_t conn_h;
   r = msg_sync_receive(port_h, &buf, 1, &call_uid, &conn_h, &flags);
   if(r < 0)
      exit(1);

   write_str("Connected\n");
   msg_handle_t handles[1] = {MSG_HANDLE(h)};
   msg_send_t msg = {.buffer = NULL, .size = 0, .handles = handles, .handle_count = 1};
   r = msg_sync_reply_obj(conn_h, call_uid, &msg);
   if(r < 0)
      exit(1);
   
   while(true) {
      write_str("Main thread\n");
      *futex = 1;
      futex_wake(futex);
      sleep(1700);
   }

   exit(0);
}
