// message passing test program

#include "prog.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/api.h"

static uint32_t child_channel_uid = (uint32_t)-1; // used by sender to periodically wake child process

// async ipc test processes
// 'client' thread sends msgs 'test', 'test2' and sends 'echo test' with msg_request (MSG_EXPECT_REPLY / reserved reply queue slot)
// it waits until the request is read by the server before sleeping 2s and repeating
// server process echo's back requests (MSG_EXPECT_REPLY) to the sender
// 'child' client process sends msg 'child' to the server and waits until it receives a msg from the server (via snooze) repeatedly

void server_msg_func(uint32_t port_uid, uint32_t channel_uid, uint32_t flags) {
   if(channel_uid && flags & MSG_FLAG_CLOSED) {
      printf("server: channel %u closed by client\n", channel_uid);
      end_subroutine();
   }

   // drain messages
   char buf[256];
   while(true) {
      uint32_t channel_flags;
      uint32_t msg_flags;
      int r = msg_read(port_uid, channel_uid, buf, 256, &channel_flags, &msg_flags);
      if(r <= 0) {
         if(r < 0)
            printf("server read error %i\n", r);
         end_subroutine();
         return;
      }
      buf[r] = '\0';
      printf("server received msg: '%s' from channel %u\n", buf, channel_uid);
      flags |= channel_flags;
      if(flags)
         printf("server received flags: %u\n", flags);

      if(strequ(buf, "child"))
         child_channel_uid = channel_uid;

      if(msg_flags & MSG_EXPECT_REPLY) {
         // echo back to client
         r = msg_reply(port_uid, channel_uid, buf, r);
         if(r < 0)
            printf("server send failed error %i\n", r);
         // wake child
         if(child_channel_uid != (uint32_t)-1) {
            r = msg_send(port_uid, child_channel_uid, buf, strlen(buf));
            if(r < 0)
               printf("server wake child failed with code %i\n", r);
         }
      }

      if(msg_flags & MSG_LAST_MSG)
         break;
   }

   end_subroutine();
}

void client_msg_func(uint32_t port_uid, uint32_t channel_uid, uint32_t flags) {
   if(flags & MSG_FLAG_CLOSED) {
      printf("server: channel %u closed by client\n", channel_uid);
      end_subroutine();
   }

   // drain messages
   char buf[256];
   while(true) {
      uint32_t channel_flags;
      uint32_t msg_flags;
      int r = msg_read(port_uid, channel_uid, buf, 256, &channel_flags, &msg_flags);
      if(r <= 0) {
         if(r < 0)
            printf("client read error %i\n", r);
         end_subroutine();
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

   end_subroutine();
}

void client_main() {
   override_msg(&client_msg_func);

   uint32_t port_uid;
   uint32_t channel_uid = port_connect("/prog9", &port_uid);
   if(MSG_IS_ERR(channel_uid)) {
      printf("Failed to connect to port, error %i\n", (int)channel_uid);
      exit(1);
   }

   printf("client connected to port %u via channel %u\n", port_uid, channel_uid);

   while(true) {
      // send message to server
      int r = msg_send(port_uid, channel_uid, "test", 4);
      if(r < 0)
         printf("msg_send failed, code %i\n", r);

      r = msg_send(port_uid, channel_uid, "test2", 5);
      if(r < 0)
         printf("msg_send 2 failed, code %i\n", r);

      // the only message the server replies to, so the only one that reserves a reply slot
      r = msg_request(port_uid, channel_uid, "echo test", strlen("echo test"));
      if(r < 0)
         printf("msg_request failed, code %i\n", r);

      msg_wait_for_read(port_uid, channel_uid);

      sleep(800);
   }
}

void child_main() {
   printf("Prog9 child process\n");
   set_window_title("Prog9 Child");

   // wait for server - todo autowait func
   uint32_t serverport;
   uint32_t channel;
   while(true) {
      printf("connecting\n");
      channel = port_connect("/prog9", &serverport);
      if(!MSG_IS_ERR(channel))
         break;

      sleep(100);
   }

   printf("child connected to server on port %u channel %u\n", serverport, channel);

   char buf[10];
   strcpy(buf, "child");
   int r = msg_send(serverport, channel, buf, 5);
   if(r < 0)
      printf("child process send failed error %i\n", r);

   while(true) {
      // wait to be woken by msg
      uint32_t channel_flags;
      uint32_t msg_flags;
      int r = msg_read(serverport, channel, buf, 10, &channel_flags, &msg_flags);
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
   int t = launch_task("/sys/prog9.elf", 1, childargs, false, false);
   if(t < 0) {
      printf("child process launch failed\n");
   }
   strcpy(childargs[0], "sync");
   t = launch_task("/sys/prog9.elf", 1, childargs, false, false);
   if(t < 0) {
      printf("sync process launch failed\n");
   }
   free(childargs[0]);

   uint32_t port = create_port("/prog9", true);
   if(MSG_IS_ERR(port)) {
      printf("Failed to create port, error %i\n", (int)port);
      exit(1);
   }
   printf("Created port %u\n", port);

   override_msg(&server_msg_func);

   create_thread(&client_main);

   while(true) { snooze(); }
}

// process for testing sync ipc
// client thread sends 'synctest' to server every 2s
// server echoes msg back to client

void sync_thread() {
   // 'sync' client thread
   printf("Prog9 sync thread\n");

   uint32_t port_uid;
   uint32_t channel_uid;
   channel_uid = port_connect("/prog9/sync", &port_uid);

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
         int r = msg_send(port_uid, channel_uid, buf2, 32);
         if(r < 0)
            printf("child async send status %i\n", r);
         if(ending)
            exit(1); // 'ending' msg should be delivered, then flag port disconnect (separately)
      }

      int r = msg_sync_send(channel_uid, (uint8_t*)buf, 64, (uint8_t*)recv_buf, 32);
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

   uint32_t port = create_port("/prog9/sync", false);
   create_thread(&sync_thread);

   char buf[64];
   uint32_t call_id;

   while(true) {
      uint32_t channel_uid, flags;
      int r = msg_sync_receive(port, (uint8_t*)buf, 64, &call_id, &channel_uid, &flags);
      if(r == 0) {
         // flags
         printf("server read flags %u\n", flags);
         if(channel_uid && flags & MSG_FLAG_CLOSED)
            exit(1);
      }
      if(r > 0) {
         printf("server received msg '%s'\n", buf);
         if(call_id) {
            // reply
            r = msg_sync_reply(call_id, (uint8_t*)buf, 64);
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
