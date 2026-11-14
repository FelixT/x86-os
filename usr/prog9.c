// message passing test program

#include "prog.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/api.h"

static handle_t child_conn_h = 0; // connection to child process - used by sender to periodically wake child process
static volatile handle_t client_conn_h = 0; // client thread's conn to our own port, set once connected

// async ipc test processes
// 'client' thread sends msgs 'test', 'test2' and sends 'echo test' with msg_request (MSG_EXPECT_REPLY / reserved reply queue slot)
// it waits until the request is read by the server before sleeping 0.8s and repeating
// server process echo's back requests (MSG_EXPECT_REPLY) to the sender
// 'child' client process sends msg 'child' to the server and waits until it receives a msg from the server (via snooze) repeatedly
// msg_func belongs to the process and always runs on the main thread, so the server and
// the in-process client share one callback (msg_func) that dispatches on conn_h

static void server_handle(handle_t conn_h, uint32_t flags) {
   if(!conn_h) {
      printf("server: port signal flags %u\n", flags);
      return;
   }
   if(flags & MSG_FLAG_CLOSED) {
      printf("server: channel closed by client\n");
      return;
   }

   // drain messages
   char buf[256];
   while(true) {
      uint32_t channel_flags, msg_flags;
      int r = msg_read(conn_h, buf, 255, &channel_flags, &msg_flags);
      if(r <= 0) {
         if(r < 0)
            printf("server read error %i\n", r);
         return;
      }
      buf[r] = '\0';
      printf("server received msg: '%s' from conn %u\n", buf, conn_h);
      flags |= channel_flags;
      if(flags)
         printf("server received flags: %u\n", flags);

      if(strequ(buf, "child"))
         child_conn_h = conn_h;

      if(msg_flags & MSG_EXPECT_REPLY) {
         // echo back to client
         r = msg_reply(conn_h, buf, r);
         if(r < 0)
            printf("server send failed error %i\n", r);
         // wake child
         if(child_conn_h) {
            r = msg_send(child_conn_h, buf, strlen(buf));
            if(r < 0)
               printf("server wake child failed with code %i\n", r);
         }
      }

      if(msg_flags & MSG_LAST_MSG)
         break;
   }
}

static void client_handle(handle_t conn_h, uint32_t flags) {
   if(flags & MSG_FLAG_CLOSED) {
      printf("client: channel closed by server\n");
      return;
   }

   // drain messages
   char buf[256];
   while(true) {
      uint32_t channel_flags, msg_flags;
      int r = msg_read(conn_h, buf, 255, &channel_flags, &msg_flags);
      if(r <= 0) {
         if(r < 0)
            printf("client read error %i\n", r);
         return;
      }
      buf[r] = '\0';
      printf("client received msg: '%s'\n", buf);
      flags |= channel_flags;
      if(flags)
         printf("client received flags: %u\n", flags);

      if(msg_flags & MSG_LAST_MSG)
         break;
   }
}

void msg_func(handle_t conn_h, uint32_t flags) {
   if(conn_h && conn_h == client_conn_h)
      client_handle(conn_h, flags);
   else
      server_handle(conn_h, flags);
   end_subroutine();
}

void client_main() {
   // runs on a second thread, its notifications arrive through msg_func on the main thread
   handle_t conn_h;
   int r = port_connect("/prog9", &conn_h);
   if(r < 0) {
      printf("Failed to connect to port, error %i\n", r);
      exit(1);
   }
   client_conn_h = conn_h;

   printf("client connected to port\n");

   while(true) {
      // send message to server
      int r = msg_send(conn_h, "test", 4);
      if(r < 0)
         printf("msg_send failed, code %i\n", r);

      r = msg_send(conn_h, "test2", 5);
      if(r < 0)
         printf("msg_send 2 failed, code %i\n", r);

      // the only message the server replies to, so the only one that reserves a reply slot
      r = msg_request(conn_h, "echo test", strlen("echo test"));
      if(r < 0)
         printf("msg_request failed, code %i\n", r);

      r = msg_wait_for_read(conn_h);
      if(r < 0 && r != MSG_ERR_EMPTY_MSG) // empty: the server already read everything
         printf("msg_wait_for_read failed, code %i\n", r);

      sleep(800);
   }
}

void child_main() {
   printf("Prog9 child process\n");
   set_window_title("Prog9 Child");

   // wait for server - todo autowait func
   handle_t conn_h;
   while(true) {
      printf("connecting\n");
      int r = port_connect("/prog9", &conn_h);
      if(r >= 0)
         break;

      sleep(100);
   }

   printf("child connected to server on conn handle %u\n", conn_h);

   char buf[10];
   strcpy(buf, "child");
   int r = msg_send(conn_h, buf, 5);
   if(r < 0)
      printf("child process send failed error %i\n", r);

   while(true) {
      // wait to be woken by msg
      uint32_t channel_flags, msg_flags;
      int r = msg_read(conn_h, buf, 10, &channel_flags, &msg_flags);
      if(r == MSG_READ_EMPTY)
         printf("read: queue empty\n");
      if(r < 0)
         printf("read: error\n");
      if(r > 0)
         printf("msg: %s\n", buf);
      snooze();
   }
}


void server_main() {
   set_window_title("Prog9 Server");
   char *childargs[1];
   childargs[0] = malloc(strlen("child")+1);
   strcpy(childargs[0], "child");
   int t = launch_task("/sys/prog9.elf", 1, childargs, false, false, NULL);
   if(t < 0) {
      printf("child process launch failed\n");
   }
   strcpy(childargs[0], "sync");
   t = launch_task("/sys/prog9.elf", 1, childargs, false, false, NULL);
   if(t < 0) {
      printf("sync process launch failed\n");
   }
   free(childargs[0]);

   handle_t port_h;
   int r = create_port("/prog9", true, &port_h);
   if(r < 0) {
      printf("Failed to create port, error %i\n", r);
      exit(1);
   }
   printf("Created port %u\n", port_h);

   override_msg(&msg_func);

   create_thread(&client_main);

   while(true) { snooze(); }
}

// process for testing sync ipc
// client thread sends 'synctest' to server every 2s
// server echoes msg back to client

void sync_thread() {
   // 'sync' client thread
   printf("Prog9 sync thread\n");

   handle_t conn_h;
   int r = port_connect("/prog9/sync", &conn_h);
   if(r < 0) {
      printf("sync connect failed: %i\n", r);
      exit(1);
   }

   char buf[32] = "synctest";
   char buf2[32] = "asynctest";
   char recv_buf[64];
   int i = 0;
   while(true) {
      if((i%8) == 0) {
         // send async msg. if client is waited it is delivered instantly
         bool ending = i == 8*14;
         if(ending)
            strcpy(buf2, "ending");
         int r = msg_send(conn_h, buf2, 32);
         if(r < 0)
            printf("child async send status %i\n", r);
         if(ending) {
            hrelease(conn_h); // 'ending' msg should be delivered, then flag port disconnect (separately)
            exit(1);
         }
      }

      int r = msg_sync_send(conn_h, (uint8_t*)buf, 64, (uint8_t*)recv_buf, 32);
      if(r > 0)
         printf("child received response '%s'\n", recv_buf);
      else
         printf("child send/recv status %i\n", r);

      sleep(800);

      i++;
   }
}

void sync_main() {
   printf("Prog9 sync process\n");
   set_window_title("Prog9 Sync");

   handle_t port_h;
   int r = create_port("/prog9/sync", false, &port_h);
   if(r < 0) {
      printf("create port failed: %i\n", r);
      exit(1);
   }
   create_thread(&sync_thread);

   char buf[64];
   uint32_t call_id;

   while(true) {
      uint32_t flags;
      handle_t conn_h;
      int r = msg_sync_receive(port_h, (uint8_t*)buf, 64, &call_id, &conn_h, &flags);
      if(r == 0) {
         // flags
         printf("server read flags %u\n", flags);
         if(conn_h && flags & MSG_FLAG_CLOSED)
            exit(1);
      }
      if(r > 0) {
         printf("server received msg '%s'\n", buf);
         if(call_id) {
            // reply
            r = msg_sync_reply(conn_h, call_id, (uint8_t*)buf, 64);
            if(r < 0)
               printf("server reply status %i\n", r);
         } // otherwise async/notif
      }
   }
}

void _start(int argc, char **args) {
   if(argc > 0 && strequ(args[0], "sync")) {
      sync_main();
   }

   if(argc > 0 && strequ(args[0], "child")) {
      // child process client
      child_main();
   }

   // server
   server_main();
}
