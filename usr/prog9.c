// message passing test program

#include "prog.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/api.h"

void server_msg_func(uint32_t port_uid, uint32_t channel_uid, uint32_t flags) {
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
            printf("server read error %i\n", r);
         end_subroutine();
         return;
      }
      buf[r] = '\0';
      printf("server received msg: '%s' from channel %u\n", buf, channel_uid);
      flags |= channel_flags;
      if(flags)
         printf("server received flags: %u\n", flags);

      if(msg_flags & MSG_EXPECT_REPLY) {
         // echo back to client
         r = msg_reply(port_uid, channel_uid, buf, r);
         if(r < 0)
            printf("server send failed error %i\n", r);
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

void client_start() {
   override_msg(&client_msg_func);

   uint32_t port_uid;
   uint32_t channel_uid = port_connect("/prog9", &port_uid);
   if(MSG_IS_ERROR(channel_uid)) {
      printf("Failed to connect to port, error %i\n", (int)channel_uid);
      exit(1);
   }

   printf("client connected to port %u via channel %u\n", port_uid, channel_uid);

   while(true) {
      // send message to server
      int r = msg_send(port_uid, channel_uid, "test", 4);
      if(r < 0)
         printf("msg_send2 failed, code %i\n", r);

      r = msg_send(port_uid, channel_uid, "test2", 5);
      if(r < 0)
         printf("msg_send3 failed, code %i\n", r);

      // the only message the server replies to, so the only one that reserves a reply slot
      r = msg_request(port_uid, channel_uid, "echo test", strlen("echo test"));
      if(r < 0)
         printf("msg_send4 failed, code %i\n", r);

      msg_wait_for_read(port_uid, channel_uid);

      sleep(1000);
   }
}

void _start(int argc, char **args) {
   if(argc > 0 && strequ(args[0], "child")) {
      // child process client
      printf("Prog9 child process\n");

      // wait for server - todo autowait func
      uint32_t serverport;
      uint32_t channel;
      while(true) {
         printf("connecting\n");
         channel = port_connect("/prog9", &serverport);
         if(!MSG_IS_ERROR(channel))
            break;

         sleep(100);
      }

      printf("child connected to server on port %u channel %u\n", serverport, channel);

      char buf[10];
      strcpy(buf, "child");
      int r = msg_send(serverport, channel, buf, 5);
      if(r < 0)
         printf("child process send failed error %i\n", r);

      while(true) { yield(); }

   }

   // server
   char *childargs[1];
   childargs[0] = malloc(strlen("child")+1);
   strcpy(childargs[0], "child");
   int t = launch_task("/sys/prog9.elf", 1, childargs, false, false);
   if(t < 0) {
      printf("child process launch failed\n");
   }
   free(childargs[0]);

   uint32_t port = create_port("/prog9", true);
   if(MSG_IS_ERROR(port)) {
      printf("Failed to create port, error %i\n", (int)port);
      exit(1);
   }
   printf("Created port %u\n", port);

   override_msg(&server_msg_func);

   create_thread(&client_start);

   while(true) { yield(); }
}
