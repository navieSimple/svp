#ifndef SVP_H
#define SVP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include <stdint.h>
#ifndef BUILD_SVP_CLIENT
#include <pthread.h>
#include "zlog/zlog.h"
#endif

#ifndef ___VBox_RemoteDesktop_VRDEOrders_h
#define VRDE_ORDER_DIRTY_RECT     (0)
#define VRDE_ORDER_SOLIDRECT      (1)
#define VRDE_ORDER_SOLIDBLT       (2)
#define VRDE_ORDER_DSTBLT         (3)
#define VRDE_ORDER_SCREENBLT      (4)
#define VRDE_ORDER_PATBLTBRUSH    (5)
#define VRDE_ORDER_MEMBLT         (6)
#define VRDE_ORDER_CACHED_BITMAP  (7)
#define VRDE_ORDER_DELETED_BITMAP (8)
#define VRDE_ORDER_LINE           (9)
#define VRDE_ORDER_BOUNDS         (10)
#define VRDE_ORDER_REPEAT         (11)
#define VRDE_ORDER_POLYLINE       (12)
#define VRDE_ORDER_ELLIPSE        (13)
#define VRDE_ORDER_SAVESCREEN     (14)
#define VRDE_ORDER_TEXT           (15)
#endif

#define SVP_MAGIC           0x7f5a
#define SVP_SIZEOFRGBA          4
#define SVP_PASSWORDSIZE        20
#define SVP_ADDRESSSIZE         60
#define SVP_PORTSSIZE           20
#define SVP_ADDRESS_OPTION_MAX  500
#define SVP_MAX_CHANNELS        8
#define SVP_MAX_CLIENTS         10
#define SVP_RECV_BUFSIZE        10485760
#define SVP_SEND_BUFSIZE        10485760
#define SVP_MAX_CURSOR_DATA     65536
#define SVP_CMD_BUFSIZE         10000
#define SVP_MAX_CLIP_RECT       64
#define SVP_MAX_CLIP_CMD        16

#define SVP_CMDQUEUE_SIZE   4096
#define SVP_CMDQUEUE_MASK   4095

enum SVP_CMD
{
    SC_NOP = 0,

    SC_SCREEN_RESIZE = 1,
    SC_CURSOR,

    SC_DIRTY_RECT = 10, /* svp_rect, svp_databits, variable size bitmap data */
    SC_SOLID_RECT,
    SC_SOLID_BLT,
    SC_DST_BLT,
    SC_SCREEN_BLT,
    SC_PATTERN_BLT_BRUSH,
    SC_MEM_BLT,
    SC_CACHED_BITMAP,
    SC_DELETED_BITMAP,
    SC_LINE,
    SC_CLIP_DRAW,
    SC_POLYLINE,
    SC_ELLIPSE,
    SC_SAVE_SCREEN,
    SC_RESTORE_SCREEN,
    SC_TEXT,
    SC_REFRESH_SCREEN,

    SC_KEYBOARD_EVENT = 30,
    SC_MOUSE_EVENT,
    SC_VIDEO_MODESET,
    SC_GRAPHIC_CONFIG,

    SC_MAIN_CHANNEL_INIT = 40,
    SC_CHANNEL_LIST,
    SC_CHANNEL_INIT,
    SC_CHANNEL_READY,

    SC_DISPLAY_WORKER_OUTPUT = 50,

    SC_USB_NEGOTIATE = 60,
    SC_USB_DEVICE_LIST_QUERY = 61,
    SC_USB_DEVICE_LIST_RESULT = 62,
    SC_USB_OPEN = 63,
    SC_USB_CLOSE = 64,
    SC_USB_RESET = 65,
    SC_USB_SET_CONFIG = 66,
    SC_USB_CLAIM_INTERFACE = 67,
    SC_USB_RELEASE_INTERFACE = 68,
    SC_USB_SET_INTERFACE = 69,
    SC_USB_QUEUE_URB = 70,
    SC_USB_REAP_URB = 71,
    SC_USB_CLEAR_HALTED_EP = 72,
    SC_USB_CANCEL_URB = 73,
    SC_USB_URB_DATA = 74,
    SC_USB_OPEN_STATUS = 75,
    SC_USB_RESET_STATUS = 76,
    SC_USB_SET_CONFIG_STATUS = 77,
    SC_USB_CLAIM_INTERFACE_STATUS = 78,
    SC_USB_RELEASE_INTERFACE_STATUS = 79,
    SC_USB_SET_INTERFACE_STATUS = 80,
    SC_USB_CLEAR_HALTED_EP_STATUS = 81,
    SC_USB_END = 82,    // place holder
    SC_END = 83         // place holder
};

enum SVP_EVENT
{
    SE_GRAPHIC_UPDATE = 1,
    SE_GRAPHIC_RECONFIG,
    SE_OUTPUT_UPDATE,
    SE_CURSOR_UPDATE,
    SE_USB_UPDATE
};

enum SVP_CHANNEL
{
    SH_MAIN = 0,
    SH_DISPLAY,
    SH_CURSOR,
    SH_INPUT,
    SH_USB,

    SH_END
};

enum SVP_FEATURE
{
    SF_NONE = 0,
    SF_GDI = (1 << 0),
    SF_LZ4 = (1 << 1),
    SF_YUV = (1 << 8),
    SF_JPEG = (1 << 9)
};

enum SVP_PIXEL_FORMAT
{
    SPF_INVALID,
    SPF_RGB32,
    SPF_BGR24,
    SPF_RGB24,
    SPF_RGB16,
    SPF_RGB8,
    SPF_MONO
};

enum SVP_MOUSE_BUTTON
{
    SMB_LEFT = 1,
    SMB_RIGHT = 2,
    SMB_MIDDLE = 4,
    SMB_WHEEL_UP = 8,
    SMB_WHEEL_DOWN = 16
};

enum SVP_CODEC
{
    SX_RAW,
    SX_LZ4,
    SX_JPEG,

    SX_USER = 100,

    SX_AUTO = 255
};

struct svp_screen_info
{
    int w;
    int h;
    int bpp;
};

struct svp_bitmap
{
    enum SVP_PIXEL_FORMAT format;
    int x;
    int y;
    int w;
    int h;
    int size;
    uint8_t *bits;
};

struct svp_solid_rect
{
    int x;
    int y;
    int w;
    int h;
    uint32_t rgb;
};

struct svp_solid_blt
{
    int x;
    int y;
    int w;
    int h;
    int rgb;
    uint8_t rop;
};

struct svp_dst_blt
{
    int x;
    int y;
    int w;
    int h;
    uint8_t rop;
};

struct svp_screen_blt
{
    int x;
    int y;
    int w;
    int h;
    int x_src;
    int y_src;
    uint8_t rop;
};

struct svp_pat_blt_brush
{
    int x;
    int y;
    int w;
    int h;
    int x_src;
    int y_src;
    uint32_t rgb_fg;
    uint32_t rgb_bg;
    uint8_t rop;
    uint8_t pattern[8];
};

/* 128 bit bitmap hash. */
typedef uint8_t svp_bitmap_hash_t[16];

struct svp_mem_blt
{
    int x;
    int y;
    int w;
    int h;
    int x_src;
    int y_src;
    uint8_t rop;
    svp_bitmap_hash_t hash;
};

struct svp_cached_bitmap
{
    svp_bitmap_hash_t hash;
    struct svp_bitmap bitmap;
};

struct svp_deleted_bitmap
{
    svp_bitmap_hash_t hash;
};

struct svp_line
{
    int x1;
    int y1;
    int x2;
    int y2;
    int x_bounds1;
    int y_bounds1;
    int x_bounds2;
    int y_bounds2;
    uint8_t mix;
    uint32_t rgb;
};

struct svp_point
{
    int16_t x;
    int16_t y;
};

struct svp_polyline
{
    int x_start;
    int y_start;
    uint8_t  mix;
    uint32_t rgb;
    uint8_t count;
    struct svp_point points[16];
};

struct svp_ellipse
{
    int x;
    int y;
    int w;
    int h;
    uint8_t mix;
    uint8_t fill_mode;
    uint32_t rgb;
};

struct svp_save_screen
{
    int x;
    int y;
    int w;
    int h;
    uint8_t ident;
};

struct svp_restore_screen
{
    int x;
    int y;
    int w;
    int h;
    uint8_t ident;
    struct svp_bitmap bitmap;
};

struct svp_glyph
{
    uint64_t handle;

    /* The character origin in the bitmap. */
    int x_origin;
    int y_origin;

    /* 1BPP bitmap. Rows are byte aligned. Size is (((w + 7)/8) * h + 3) & ~3. */
    struct svp_bitmap bitmap;

    struct list_head link;
};

struct svp_text
{
    int x_bg;
    int y_bg;
    int w_bg;
    int h_bg;

    int x_opaque;
    int y_opaque;
    int w_opaque;
    int h_opaque;

    int max_glyph;

    int flags;
    int char_inc;
    uint32_t rgb_fg;
    uint32_t rgb_bg;

    int glyph_count;
    struct list_head glyph_list;
};

struct svp_rect
{
    int x;
    int y;
    int w;
    int h;
};

struct svp_clip_draw
{
    int rect_count;
    struct svp_rect rect[SVP_MAX_CLIP_RECT];
    int cmd_count;
    struct {
        int type;
        uint8_t *data;
        int size;
    } cmd[SVP_MAX_CLIP_CMD];
};

struct svp_cursor
{
    int x_hot;
    int y_hot;
    int w;
    int h;
    struct svp_bitmap mask;
    struct svp_bitmap pixmap;
};

struct svp_usb_device
{
    uint16_t vid;
    uint16_t pid;
    uint16_t usb_version;
    uint8_t klass;
    uint8_t sub_class;
    uint8_t protocol;
    uint16_t port;
    uint16_t revision;
    uint16_t speed;

    char vender_name[32];
    char product_name[32];
    char serial_number[32];

    struct list_head link;
};

struct svp_usb_device_list
{
    int count;
    struct list_head list;
};

struct svp_usb_ids
{
    uint32_t client_id;
    uint32_t dev_id;
};

struct svp_usb_set_config
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t config;
};

struct svp_usb_claim_interface
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t iface;
};

struct svp_usb_release_interface
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t iface;
};

struct svp_usb_set_interface
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t iface;
    uint8_t setting;
};

enum SVP_USB_XFER_TYPE
{
    SUXT_CTRL = 0,
    SUXT_ISOC = 1,
    SUXT_BULK = 2,
    SUXT_INTR = 3,
    SUXT_MSG = 4
};

enum SVP_USB_DIRECTION
{
    SUD_SETUP = 0,
    SUD_IN = 1,
    SUD_OUT = 2
};

struct svp_usb_queue_urb
{
    uint32_t client_id;
    uint32_t dev_id;
    uint32_t handle;    /* Distinguishes that particular URB. Later used in CancelURB and returned by ReapURB */
    uint8_t type;
    uint8_t ep;
    uint8_t direction;
    uint32_t urblen;    /* Length of the URB. */
    uint32_t datalen;   /* Length of the data. */
    void *data;         /* In RDP layout the data follow. */
};

struct svp_usb_clear_halted_ep
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t ep;
};

struct svp_usb_cancel_urb
{
    uint32_t client_id;
    uint32_t dev_id;
    uint32_t handle;
};

enum SVP_URB_STATUS
{
    SUS_OK = 0,
    SUS_STALL = 1,
    SUS_DNR = 2,
    SUS_CRC = 3,
    SUS_BS = 4,  /* BitStuffing */
    SUS_DTM = 5,  /* DataToggleMismatch */
    SUS_PCF = 6,  /* PIDCheckFailure */
    SUS_UPID = 7,  /* UnexpectedPID */
    SUS_DO = 8,  /* DataOverrun */
    SUS_DU = 9,  /* DataUnderrun */
    SUS_BO = 10, /* BufferOverrun */
    SUS_BU = 11, /* BufferUnderrun */
    SUS_ERR = 12 /* VBox protocol error. */
};

enum SUS_URB_FLAGS
{
    SUF_CONTINUED = 0x0,
    SUF_LAST = 0x1,
    SUF_FRAGMENT = 0x2,
    SUF_VALID_FLAGS = SUF_LAST,
    SUF_FLAGS_V3 = (SUF_LAST | SUF_FRAGMENT)
};

#ifndef BUILD_SVP_CLIENT
struct svp_backend_operations
{
    void (*client_logon)(struct svp_backend_operations *, int id);
    void (*client_logout)(struct svp_backend_operations *, int id);
    void (*keyboard_input)(struct svp_backend_operations *, int pressed_down, uint32_t scancode);
    void (*mouse_input)(struct svp_backend_operations *, int x, int y, int button_mask);
    void (*video_modeset)(struct svp_backend_operations *, int w, int h, int depth);
    void (*usb_input)(struct svp_backend_operations *, int client_id, int req, const void *data, int size);
    void *priv;
};

struct in_cmd;
struct svp_frontend_operations
{
    void (*init)(struct svp_frontend_operations *, const char *addr, uint16_t port);
    void (*fini)(struct svp_frontend_operations *);
    void (*screen_resize)(struct svp_frontend_operations *, int w, int h);
    void (*graphic_update)(struct svp_frontend_operations *, struct in_cmd *cmd);
    void (*cursor_update)(struct svp_frontend_operations *, struct svp_cursor *cursor);
    void (*usb_update)(struct svp_frontend_operations *, struct in_cmd *cmd);
    void *priv;
};

extern struct svp_backend_operations *g_bops;

int svp_get_interface(struct svp_backend_operations *bops, struct svp_frontend_operations *fops);

#endif

#ifdef __cplusplus
}
#endif

#endif // SVP_H
