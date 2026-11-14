// common c header between usrmode and kernel api

#ifndef LIB_API_H
#define LIB_API_H

#include "stdint.h"
#include "stdbool.h"

typedef enum {
   API_RESERVED_START, // calls start at callid 1
   API_WRITE_STRING,
   API_GET_TASK_EVENT_COUNT,
   API_YIELD,
   API_CONTROL_HANDLE,
   API_HANDLE_INFO,
   API_RESERVED_2, // deprecated
   API_RETURN_FRAMEBUFFER,
   API_RESERVED_3,
   API_REDRAW_WINDOW,
   API_END_TASK,
   API_WM_OVERRIDE,
   API_END_SUBROUTINE,
   API_RESERVED_4,
   API_RETURN_WINDOW_WIDTH,
   API_RETURN_WINDOW_HEIGHT,
   API_RESERVED_5,
   API_RESERVED_6,
   API_SET_WINDOW_SETTING,
   API_GET_WINDOW_SETTING,
   API_SET_WINDOW_POSITION,
   API_DRAW_BMP, // unsafe
   API_WRITE_STRING_AT,
   API_CLEAR_WINDOW,
   API_SET_WINDOW_MINIMISED,
   API_READ_DIR,
   API_GET_WINDOW_POSITION,
   API_RESERVED_7,
   API_RESERVED_8,
   API_RESERVED_9,
   API_QUEUE_EVENT,
   API_SLEEP,
   API_LAUNCH_TASK,
   API_GET_SETTING,
   API_RESERVED_10,
   API_SET_SETTING,
   API_RESERVED_11,
   API_REDRAW_REGION,
   API_RESERVED_12,
   API_RESERVED_13,
   API_RESERVED_14,
   API_NEW_FILE,
   API_SET_WINDOW_TITLE,
   API_SET_WORKING_DIR,
   API_GET_WORKING_DIR,
   API_RESERVED_15,
   API_RESERVED_16,
   API_READ,
   API_SEEK,
   API_DEBUG_WRITE_STR,
   API_MKDIR,
   API_SBRK,
   API_OPEN,
   API_WRITE,
   API_CREATE_SCROLLBAR,
   API_SET_SCROLLABLE_HEIGHT,
   API_SCROLL_TO,
   API_GET_TIMER_TICK,
   API_RENAME,
   API_FSIZE,
   API_SET_WINDOW_SIZE,
   API_GET_FONT_INFO,
   API_CREATE_WINDOW,
   API_CLOSE_WINDOW,
   API_UNMAP,
   API_CREATE_THREAD,
   API_GET_TASKS,
   API_KILL_TASK,
   API_UNLINK,
   API_RMDIR,
   API_PIPE,
   API_UNPAUSE,
   API_DUP2,
   API_CLOSE,
   API_DUP_HANDLE,
   API_GET_TIME,
   API_FUTEX_WAIT,
   API_FUTEX_WAKE,
   API_SHARED_CREATE,
   API_FOCUS_TASK,
   API_MAP,
   API_GET_TASKID,
   API_PCI_CONNECT,
   API_PCI_EXISTS,
   API_DMA,
   API_CLOSE_HANDLE,
   API_ESCALATE,
   API_FPSIZE,
   API_TRUNCATE,
   API_CREATE_PORT,
   API_PORT_CONNECT,
   API_OVERRIDE_MSG,
   API_MSG_SEND,
   API_MSG_READ,
   API_WAIT_ON_RECEIVE,
   API_SNOOZE,
   API_RESERVED_17,
   API_RESERVED_18,
   API_MSG_CALL,
   API_MSG_RECEIVE,
   API_MSG_REPLY,
   API_RESERVED_END // must be at end
} api_call_t;

#define MAX_PATH 256

typedef struct api_task_t {
   int id;
   uint32_t uid;
   bool enabled;
   bool paused;
   uint32_t ticks;
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
   char working_dir[MAX_PATH];
   char exe_path[MAX_PATH];
   char main_window_name[32];
} api_task_t;

typedef struct api_launch_task_t {
   char *path;
   int argc;
   char **args;
   bool copy; // copy environment of current task (use same wd & copy handles)
   bool paused; // start paused
   bool handle; // return handle
} api_launch_task_t;

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
   SETTING_SYS_FONT_PADDING,
   SETTING_THEME_GRADIENTSTYLE,
   SETTING_MOUSE_SPEED,
   SETTING_MOUSE_WRAP
} api_setting_t;

#define MSG_ERR_BASE 0xFFFFFF00
#define MSG_IS_ERR(x) ((uint32_t)(x) >= MSG_ERR_BASE)

#define MSG_PORT_NAME_LEN 32
#define MSG_MAX_HANDLES 4

// per message flags (msg_send/msg_read)
#define MSG_EXPECT_REPLY (1u<<0) // on client_reserves, client sends with this will reserve a reply slot so the server isn't blocked responding
#define MSG_LAST_MSG (1u<<1) // reading last message in queue (msg_read)
#define MSG_REPLY (1u<<2) // send is reply to MSG_EXPECT_REPLY request - releases the reserved slot

// channel notification flags
#define MSG_FLAG_CLOSED (1u<<0)

// msg per handle flags
#define MSG_HANDLE_MOVE (1u<<0) // releases senders handle on success

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
   MSG_ERR_EMPTY_MSG = -14,
   MSG_ERR_INVALID_TASK = -15,
   MSG_ERR_RECEIVE_QUEUE_FULL = -16, // with client_reserves, the server has no free queue slots to send a reply,
   MSG_ERR_PEER_DISCONNECTED = -17,
   MSG_ERR_DISCONNECTED = -18,
   MSG_ERR_MSG_NOT_FOUND = -19, // msg_reply - call_uid not found
   MSG_ERR_TIMEOUT = -20,
   MSG_ERR_INVALID_MSG = -21, // invalid msg_t* or buffer
   MSG_ERR_TOO_MANY_HANDLES = -22,
   MSG_ERR_PEER_INVALID_BUF = -23,
   MSG_ERR_INVALID_HANDLES = -24,
   MSG_ERR_PEER_INVALID_HANDLES = -25,
   MSG_ERR_PEER_INVALID_MSG = -26, // receiver's msg_recv_t* became invalid
   MSG_ERR_PEER_TOO_MANY_HANDLES = -27, // peer sent more handles than could be delivered, e.g. receive handle_count too small for a MSG_HANDLE_MOVE
   MSG_ERR_HANDLE_FAIL = -28, // invalid handle/couldn't create handle
   MSG_ERR_PEER_HANDLE_FAIL = -29, // invalid handle/couldn't create handle for peer
   MSG_ERR_INVALID_CALLID = -30,
   MSG_ERR_CHANNEL_BUSY = -31
} msg_err_t;

typedef int handle_t;

typedef struct msg_handle_t {
   handle_t handle;
   uint32_t rights; // rights mask
   uint32_t flags; // MSG_HANDLE_MOVE
} msg_handle_t;

typedef struct msg_send_t {
   void *buffer;
   int size;
   msg_handle_t *handles;
   int handle_count;
} msg_send_t;

typedef struct msg_recv_t {
   void *buffer;
   int buffer_size;
   handle_t *handles;
   int handle_count; // max handles to receive
   // written by kernel on reply/receive
   int bytes_received;
   int sent_size; // msg size as sent
   int handles_sent;
   int handles_received;
   uint32_t call_uid; // set on receive
   uint32_t flags; // channel flags if conn_h != 0, otherwise signal flags
   handle_t conn_h; // conn handle the message or flags came from (0 for none/signal)
   uint32_t message_flags;
} msg_recv_t;

typedef enum { // callback args:
   WM_KEYPRESS, // uint16_t c, int w
   WM_KEYRELEASE, // uint16_t c, int w
   WM_CHECKCMD, // char *buffer
   WM_CLICK, // int x, int y, int w
   WM_MOUSERELEASE, // int x, int y, int w
   WM_DRAW, // (use NULL to disable default terminal draw)
   WM_RESIZE, // uint16_t *framebuffer, int width, int height, int w - width not including scrollbar if shown
   WM_DRAG, // int x, int y, int w
   WM_HOVER, // int x, int y, int w
   WM_CLOSE, // int w
   WM_RIGHTCLICK, // int x, int y, int w
   WM_MOUSEOUT, // int w
} wm_action_t;

#define KEY_BACKSPACE 0x08
#define KEY_ESCAPE 0x1B
#define KEY_RETURN 0x0D
#define KEY_CONTROL 0x80
#define KEY_SHIFT 0x81
#define KEY_UPARROW 0x100
#define KEY_DOWNARROW 0x101
#define KEY_LEFTARROW 0x102
#define KEY_RIGHTARROW 0x103

typedef enum {
   W_SETTING_BGCOLOUR,
   W_SETTING_TXTCOLOUR
} w_setting_t; 

typedef enum {
   PCI_CTRL_DMA, // arg: enable (1/0)
} pci_ctrl_t;

typedef enum {
   KOBJ_DMA,
   KOBJ_PCI, // pci device
   KOBJ_SHARED,
   KOBJ_FILE, // open file instance
   KOBJ_PORT, // msg port
   KOBJ_CHANNEL_CONN, // msg channel connection
   KOBJ_PROCESS, // allows handles to processes
   KOBJ_NONE // must be at end
} kobj_type_t;

#define KOBJ_RIGHT_TRANSFER (1u<<0) // sent to other process
#define KOBJ_RIGHT_DUPLICATE (1u<<1) // dup within process
#define KOBJ_RIGHT_READ (1u<<2)
#define KOBJ_RIGHT_WRITE (1u<<3)
#define KOBJ_RIGHT_CONTROL (1u<<4) // hcontrol
#define KOBJ_RIGHT_SAME ((uint32_t)-1)

#endif
