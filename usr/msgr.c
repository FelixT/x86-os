#include "prog.h"

#include "../lib/string.h"
#include "lib/stdlib.h"
#include "lib/stdio.h"

typedef struct connection_t {
   bool connected;
   char name[MSG_PORT_NAME_LEN];
   uint32_t port_uid;
   uint32_t channel_uid;
} connection_t;

#define MAX_CONNECTIONS 16
connection_t connections[MAX_CONNECTIONS];

int find_connection(char *name) {
   for(int i = 0; i < MAX_CONNECTIONS; i++) {
      if(!connections[i].connected) continue;
      if(strequ(connections[i].name, name))
         return i;
   }
   return -1;
}

int find_connection_by_uid(uint32_t port_uid, uint32_t channel_uid) {
   for(int i = 0; i < MAX_CONNECTIONS; i++) {
      if(!connections[i].connected) continue;
      if(connections[i].port_uid == port_uid && connections[i].channel_uid == channel_uid)
         return i;
   }
   return -1;
}

int find_free_connection() {
   for(int i = 0; i < MAX_CONNECTIONS; i++) {
      if(!connections[i].connected)
         return i;
   }
   return -1;
}

void checkcmd(char *buf) {
   if(strstartswith(buf, "exit")) {
      exit(0);
   }

   if(strstartswith(buf, "help")) {
      printf("Commands:\n");
      printf(" connect <port name>\n");
      printf(" sendi <port index> [echo] message\n");
   }

   if(strstartswith(buf, "connect ")) {
      char *name = buf + strlen("connect ");
      uint32_t port_uid;
      uint32_t channel_uid = port_connect(name, &port_uid);
      if(MSG_IS_ERROR(channel_uid)) {
         printf("connect: error %i\n", (int)channel_uid);
         end_subroutine();
         return;
      }
      
      int c = find_free_connection();
      if(c < 0) {
         printf("connect: no free connection slots\n");
         end_subroutine();
         return;
      }

      connections[c].connected = true;
      connections[c].port_uid = port_uid;
      connections[c].channel_uid = channel_uid;
      strncpy(connections[c].name, name, sizeof(connections[c].name)-1);
      printf("connect: connected to port %u channel %u index %i\n", port_uid, channel_uid, c);
   }

   if(strstartswith(buf, "sendi ")) {
      char *args = buf + strlen("sendi ");

      int c = strtoint(args);
      if(c < 0 || c >= MAX_CONNECTIONS || !connections[c].connected) {
         printf("sendi: invalid connection index %i\n", c);
         end_subroutine();
         return;
      }

      char *msg = strchr(args, ' ');
      if(!msg) {
         printf("sendi: no message provided\n");
         end_subroutine();
         return;
      }
      msg++; // skip space

      uint32_t flags = 0;

      if(strstartswith(msg, "echo ")) {
         flags |= MSG_EXPECT_REPLY;
         msg += strlen("echo ");
      }

      int r = msg_send_flags(connections[c].port_uid, connections[c].channel_uid, msg, strlen(msg), flags);
      if(r < 0) {
         printf("sendi: msg_send failed, code %i\n", r);
      }
   }

   if(strstartswith(buf, "closei ")) {
      char *args = buf + strlen("closei ");

      int c = strtoint(args);
      if(c < 0 || c >= MAX_CONNECTIONS || !connections[c].connected) {
         printf("closei: invalid connection index %i\n", c);
         end_subroutine();
         return;
      }

      if(!port_disconnect(connections[c].port_uid, connections[c].channel_uid)) {
         printf("closei: port_disconnect failed for index %i\n", c);
         end_subroutine();
         return;
      }
      connections[c].connected = false;
      printf("closei: closed connection index %i\n", c);
   }

   end_subroutine();
}

void msg_func(uint32_t port_uid, uint32_t channel_uid, uint32_t flags) {
   // drain messages
   char buf[256];
   while(true) {
      uint32_t channel_flags;
      uint32_t msg_flags;
      int r = msg_read(port_uid, channel_uid, buf, 256, &channel_flags, &msg_flags);
      if(r <= 0) {
         if(r < 0)
            printf("msg_read failed, code %i\n", r);
         end_subroutine();
      }
      buf[r] = '\0';

      int i = find_connection_by_uid(port_uid, channel_uid);
      if(i < 0) {
         printf("msg_func: received msg for unknown connection port %u channel %u\n", port_uid, channel_uid);
         end_subroutine();
      }
      printf("%s (%i): '%s'", connections[i].name, i, buf);
      flags |= channel_flags;
      if(flags)
         printf(" - received flags: %u", flags);
      printf("\n");
   }

   end_subroutine();
}

void _start() {
   set_window_title("msgr");
   override_term_checkcmd(&checkcmd);
   override_msg(&msg_func);

   while(true) { yield(); }

   exit(0);
}
