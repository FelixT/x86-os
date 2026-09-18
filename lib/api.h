// common c header between usrmode and kernel api

#ifndef LIB_API_H
#define LIB_API_H

#include "stdint.h"
#include "stdbool.h"

typedef struct api_task_t {
   int id;
   uint32_t uid;
   bool enabled;
   bool paused;
   int parentid;
   // for processes
   uint32_t process_uid;
   uint32_t heap_start; // heap/end of ds (vmem location)
   uint32_t heap_end;
   uint32_t prog_start; // physical addr of start of program
   uint32_t prog_entry; // addr of program entry point, may be different from prog_start
   uint32_t prog_size;
   uint32_t vmem_start; // virtual address where program is loaded
   uint32_t vmem_end;
   int no_allocated;
   bool privileged;
   char working_dir[256];
   char exe_path[256];
   char main_window_name[32];
} api_task_t;

typedef enum {
   SETTING_WIN_BGCOLOUR,
   SETTING_WIN_TXTCOLOUR,
   SETTING_DESKTOP_ENABLED,
   SETTING_DESKTOP_BGIMG_ENABLED,
   SETTING_DESKTOP_BGIMG_PATH, // str
   SETTING_WIN_TITLEBARCOLOUR,
   SETTING_THEME_TYPE,
   SETTING_WIN_TITLEBARCOLOUR2,
   SETTING_SYS_FONT_PATH, // str
   SETTING_BGCOLOUR,
   SETTINGS_SYS_FONT_PADDING,
   SETTING_THEME_GRADIENTSTYLE
} api_setting_t;

#define MSG_ERR_BASE 0xFFFFFF00
#define MSG_IS_ERROR(x) ((uint32_t)(x) >= MSG_ERR_BASE)

#define MSG_PORT_NAME_LEN 32

// per message flags (msg_send/msg_read)
#define MSG_EXPECT_REPLY 1 // on client_reserves, client sends with this will reserve a reply slot so the server isn't blocked responding
#define MSG_LAST_MSG 2 // reading last message in queue (msg_read)
#define MSG_REPLY 4 // send is reply to MSG_EXPECT_REPLY request - releases the reserved slot

// channel notification flags
#define MSG_FLAG_CLOSED 1

typedef enum {
   MSG_READ_EMPTY = 0,
   MSG_ERR_NO_PRIVILEGE = -1,
   MSG_ERR_DENIED = -2,
   MSG_ERR_NAME_TAKEN = -3,
   MSG_ERR_PORT_NOT_FOUND = -4,
   MSG_ERR_CHANNEL_NOT_FOUND = -5,
   MSG_ERR_SELF = -6, // task can't connect to self
   MSG_ERR_LIMIT = -7,
   MSG_ERR_PORT_FULL = -8, // no free channels
   MSG_ERR_NOMEM = -9,
   MSG_ERR_BUF_TOO_SMALL = -10,
   MSG_ERR_QUEUE_FULL = -11,
   MSG_ERR_INVALID_BUF = -12,
   MSG_ERR_TOO_LONG = -13,
   MSG_ERR_INVALID_MSG = -14,
   MSG_ERR_INVALID_TASK = -15,
   MSG_ERR_RECEIVE_QUEUE_FULL = -16, // with client_reserves, the server has no free queue slots to send a reply,
   MSG_ERR_PEER_DISCONNECTED = -17,
   MSG_ERR_DISCONNECTED = -18,
   MSG_ERR_MSG_NOT_FOUND = -19, // msg_reply - call_uid not found
   MSG_ERR_TIMEOUT = -20
} msg_err_t;

#define W_SETTING_BGCOLOUR 0
#define W_SETTING_TXTCOLOUR 1

#endif
