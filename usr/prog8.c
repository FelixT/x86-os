#include "prog.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"

// ipc shared memory test program

void _start(int argc, char **args) {
   override_draw(NULL, -1);

   // child/client
   if(argc > 0 && strequ(args[0], "child")) {
      printf("child task\n");

      // create shared memory
      char *img1_path = "/bmp/bg2.bmp";
      int img1_size = fpsize(img1_path);
      if(img1_size <= 0) {
         printf("loading img1 failed\n");
         exit(0);
      }
      shared_t shared = shared_create(img1_size);
      if(!shared.mem) {
         printf("shared_create failed\n");
         exit(0);
      }
      printf("created shared mem handle %i\n", shared.h);

      uint8_t *img1 = malloc(img1_size);
      int fd = open(img1_path, 0);
      read(fd, (char*)img1, img1_size);
      close(fd);

      bmp_draw(img1, 0, 0, 1, false);
      redraw();

      char *img2_path = "/bmp/bg16.bmp";
      int img2_size = fpsize(img2_path);
      if(img2_size <= 0) {
         printf("loading img2 failed\n");
         exit(0);
      }

      uint32_t port;
      uint32_t channel = port_connect("/prog8", &port);
      if(MSG_IS_ERROR(channel)) {
         printf("couldn't connect to channel\n");
         exit(0);
      }
      handle_t handles[] = {shared.h};
      uint8_t cmd = 1; // 1 = redraw, 0 = quit
      msg_send_t send_msg = {.buffer = &cmd, .size = 1, .handle_count = 1, .handles = handles};
      uint8_t buf[1];
      msg_recv_t recv_msg = {.buffer_size = 1, .buffer = buf, .handle_count = 0};

      memcpy(shared.mem, img1, img1_size);

      // notify server
      int r = msg_sync_send_obj(channel, &send_msg, &recv_msg);
      if(r < 0) {
         printf("send fail %i\n", r);
         exit(0);
      }

      printf("received %i\n", buf[0]);
      if(!buf[0]) {
         printf("server rejected shared mem\n");
         exit(0);
      }

      uint8_t *img2 = malloc(img2_size);
      fd = open(img2_path, 0);
      read(fd, (char*)img2, img2_size);
      close(fd);

      clear();
      bmp_draw(img2, 0, 0, 1, false);
      redraw();

      bool img = false;
      while(true) {
         if(img) {
            memcpy(shared.mem, img1, img1_size);
         } else {
            memcpy(shared.mem, img2, img2_size);
         }
         img = !img;
         sleep(1000);
         int r = msg_sync_send_obj(channel, &send_msg, &recv_msg);
         if(r < 0) {
            printf("send fail %i\n", r);
            break;
         }
         printf("received %i\n", buf[0]);
      }

      exit(0);
   }

   // parent/server
   uint32_t port = create_port("/prog8", false);

   char *cargs[1] = { "child" };
   int task = launch_task("/sys/prog8.elf", 1, cargs, false, true);
   unpause_task(task);

   handle_t handle;
   msg_recv_t recv_msg = {.buffer_size = 0, .buffer = NULL, .handle_count = 1, .handles = &handle};

   while(true) {
      uint32_t call_id;
      int r = msg_sync_receive_obj(port, &recv_msg, &call_id);
      if(r < 0) {
         printf("receive fail %i\n", r);
         break;
      }
      if(recv_msg.handles_received == 0) {
         printf("no handles received\n");
         // reply so client isn't left blocked in its call
         uint8_t msg[1] = {0};
         msg_sync_reply(call_id, msg, 1);
         continue;
      }
      uint8_t *mem = shared_map(handle);
      bmp_draw(mem, 0, 0, 1, false);
      redraw();

      // reply to client
      uint8_t msg[1] = {1};
      r = msg_sync_reply(call_id, msg, 1);
      if(r < 0) {
         printf("reply fail %i\n", r);
         break;
      }
      msg_recv_t timing_msg = {.buffer_size = 1, .buffer = msg, .handle_count = 0};
      while(true) {
         r = msg_sync_receive_obj(port, &timing_msg, &call_id);
         if(r < 0) {
            printf("receive fail %i\n", r);
            break;
         }
         if(r < 1 || !msg[0]) {
            // quit cmd (or empty msg)
            printf("received quit cmd\n");
            msg[0] = 0;
            msg_sync_reply(call_id, msg, 1); // don't leave client blocked in its call
            break;
         }
         // redraw cmd
         bmp_draw(mem, 0, 0, 1, false);
         redraw();
         r = msg_sync_reply(call_id, msg, 1);
         if(r < 0) {
            printf("reply fail %i\n", r);
            break;
         }
      }
   }

   exit(0);
}
