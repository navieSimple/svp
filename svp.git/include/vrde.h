#ifndef VRDE_H
#define VRDE_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "inqueue.h"

#define VRDE_DIRTY_RECT     0
#define VRDE_SOLID_RECT     1
#define VRDE_SOLID_BLT      2
#define VRDE_DST_BLT        3
#define VRDE_SCREEN_BLT     4
#define VRDE_PAT_BLT_BRUSH  5
#define VRDE_MEM_BLT        6
#define VRDE_CACHED_BITMAP  7
#define VRDE_DELETED_BITMAP 8
#define VRDE_LINE           9
#define VRDE_BOUNDS         10
#define VRDE_REPEAT         11
#define VRDE_POLYLINE       12
#define VRDE_ELLIPSE        13
#define VRDE_SAVE_SCREEN    14
#define VRDE_TEXT           15

#define VRDE_USB_OPEN               0
#define VRDE_USB_CLOSE              1
#define VRDE_USB_RESET              2
#define VRDE_USB_SET_CONFIG         3
#define VRDE_USB_CLAIM_INTERFACE    4
#define VRDE_USB_RELEASE_INTERFACE  5
#define VRDE_USB_INTERFACE_SETTING  6
#define VRDE_USB_QUEUE_URB          7
#define VRDE_USB_REAP_URB           8
#define VRDE_USB_CLEAR_HALTED_EP    9
#define VRDE_USB_CANCEL_URB         10

#define VRDE_USB_DEVICE_LIST        11
#define VRDE_USB_NEGOTIATE          12

enum VRDE_PIXEL_FORMAT
{
    VRDE_RGB32,
    VRDE_BGR24,
    VRDE_RGB16,
    VRDE_RGB8
};

struct vrde_framebuffer_info
{
    int width;
    int height;
    int bpp;
    enum VRDE_PIXEL_FORMAT format;
    uint8_t *bits;
};

/**************************************************************************************************/
#pragma pack(1)
struct vrde_hdr
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
};

struct vrde_solid_rect
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
};

struct vrde_solid_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
    uint8_t rop;
};

struct vrde_dst_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t rop;
};

struct vrde_screen_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int16_t x_src;
    int16_t y_src;
    uint8_t rop;
};

struct vrde_pat_blt_brush
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int8_t x_src;
    int8_t y_src;
    uint32_t rgb_fg;
    uint32_t rgb_bg;
    uint8_t rop;
    uint8_t pattern[8];
};

/* 128 bit bitmap hash. */
typedef uint8_t vrde_bitmap_hash_t[16];

struct vrde_mem_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int16_t x_src;
    int16_t y_src;
    uint8_t rop;
    vrde_bitmap_hash_t hash;
};

/* Header for bitmap bits. */
struct vrde_databits
{
    uint32_t size; /* Size of bitmap data without the header. */
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t Bpp; /* Bytes per pixel */
    /* Bitmap data follow. */
};

struct vrde_cached_bitmap
{
    vrde_bitmap_hash_t hash;
};

struct vrde_deleted_bitmap
{
    vrde_bitmap_hash_t hash;
};

struct vrde_line
{
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    int16_t x_bounds1;
    int16_t y_bounds1;
    int16_t x_bounds2;
    int16_t y_bounds2;
    uint8_t mix;
    uint32_t rgb;
};

struct vrde_point
{
    int16_t x;
    int16_t y;
};

struct vrde_polypoints
{
    uint8_t c;
    struct vrde_point a[16];
};

struct vrde_bounds
{
    struct vrde_point pt1;
    struct vrde_point pt2;
};

struct vrde_repeat
{
    struct vrde_bounds bounds;
};

struct vrde_polyline
{
    struct vrde_point pt_start;
    uint8_t  mix;
    uint32_t rgb;
    struct vrde_polypoints points;
};

struct vrde_ellipse
{
    struct vrde_point pt1;
    struct vrde_point pt2;
    uint8_t mix;
    uint8_t fill_mode;
    uint32_t rgb;
};

struct vrde_save_screen
{
    struct vrde_point pt1;
    struct vrde_point pt2;
    uint8_t ident;
    uint8_t restore;
};

struct vrde_glyph
{
    uint32_t size; /* size of vrde_glyph and bitmap */
    uint64_t handle;

    /* The glyph origin position on the screen. */
    int16_t x;
    int16_t y;

    /* The glyph bitmap dimensions. Note w == h == 0 for the space character. */
    uint16_t w;
    uint16_t h;

    /* The character origin in the bitmap. */
    int16_t x_origin;
    int16_t y_origin;

    /* 1BPP bitmap. Rows are byte aligned. Size is (((w + 7)/8) * h + 3) & ~3. */
    uint8_t bitmap[];
};

struct vrde_text
{
    uint32_t size;

    int16_t x_bg;
    int16_t y_bg;
    uint16_t w_bg;
    uint16_t h_bg;

    int16_t x_opaque;
    int16_t y_opaque;
    uint16_t w_opaque;
    uint16_t h_opaque;

    uint16_t max_glyph;

    uint8_t glyphs;
    uint8_t flags;
    uint16_t char_inc;
    uint32_t rgb_fg;
    uint32_t rgb_bg;

    /* struct svp_glyph(s) follow. Size of each glyph structure may vary. */
};

struct vrde_usb_req_open
{
    uint8_t code;
    uint32_t id;
};

struct vrde_usb_req_close
{
    uint8_t code;
    uint32_t id;
};

struct vrde_usb_req_reset
{
    uint8_t code;
    uint32_t id;
};

struct vrde_usb_req_set_config
{
    uint8_t code;
    uint32_t id;
    uint8_t configuration;
};

struct vrde_usb_req_claim_interface
{
    uint8_t code;
    uint32_t id;
    uint8_t iface;
};

struct vrde_usb_req_release_interface
{
    uint8_t code;
    uint32_t id;
    uint8_t iface;
};

struct vrde_usb_req_interface_setting
{
    uint8_t code;
    uint32_t id;
    uint8_t iface;
    uint8_t setting;
};

struct vrde_usb_status
{
    uint8_t status;
    uint32_t id;
};

#define VRDE_USB_TRANSFER_CTRL 0
#define VRDE_USB_TRANSFER_ISOC 1
#define VRDE_USB_TRANSFER_BULK 2
#define VRDE_USB_TRANSFER_INTR 3
#define VRDE_USB_TRANSFER_MSG  4

#define VRDE_USB_DIRECT_SETUP    0
#define VRDE_USB_DIRECT_IN       1
#define VRDE_USB_DIRECT_OUT      2

struct vrde_usb_req_queue_urb
{
    uint8_t code;
    uint32_t id;
    uint32_t handle;    /* Distinguishes that particular URB. Later used in CancelURB and returned by ReapURB */
    uint8_t type;
    uint8_t ep;
    uint8_t direction;
    uint32_t urblen;    /* Length of the URB. */
    uint32_t datalen;   /* Length of the data. */
    void *data;         /* In RDP layout the data follow. */
};

/* The queue URB has no explicit return. The reap URB reply will be
 * eventually the indirect result.
 */


/* VRDE_USB_REQ_REAP_URB
 * Notificationg from server to client that server expects an URB
 * from any device.
 * Only sent if negotiated URB return method is polling.
 * Normally, the client will send URBs back as soon as they are ready.
 */
struct vrde_usb_req_reap_urb
{
    uint8_t code;
};

#define VRDE_USB_XFERR_OK    0
#define VRDE_USB_XFERR_STALL 1
#define VRDE_USB_XFERR_DNR   2
#define VRDE_USB_XFERR_CRC   3
/* VRDE_USB_VERSION_2: New error codes. OHCI Completion Codes. */
#define VRDE_USB_XFERR_BS    4  /* BitStuffing */
#define VRDE_USB_XFERR_DTM   5  /* DataToggleMismatch */
#define VRDE_USB_XFERR_PCF   6  /* PIDCheckFailure */
#define VRDE_USB_XFERR_UPID  7  /* UnexpectedPID */
#define VRDE_USB_XFERR_DO    8  /* DataOverrun */
#define VRDE_USB_XFERR_DU    9  /* DataUnderrun */
#define VRDE_USB_XFERR_BO    10 /* BufferOverrun */
#define VRDE_USB_XFERR_BU    11 /* BufferUnderrun */
#define VRDE_USB_XFERR_ERR   12 /* VBox protocol error. */

#define VRDE_USB_REAP_URB_FLAG_CONTINUED 0x0
#define VRDE_USB_REAP_URB_FLAG_LAST      0x1
/* VRDE_USB_VERSION_3: Fragmented URBs. */
#define VRDE_USB_REAP_URB_FLAG_FRAGMENT  0x2

#define VRDE_USB_REAP_URB_VALID_FLAGS    VRDE_USB_REAP_URB_FLAG_LAST
/* VRDE_USB_VERSION_3: Fragmented URBs. */
#define VRDE_USB_REAP_URB_VALID_FLAGS_3  (VRDE_USB_REAP_URB_FLAG_LAST | VRDE_USB_REAP_URB_FLAG_FRAGMENT)

struct vrde_usb_urb
{
    uint32_t    id;        /* From which device the URB arrives. */
    uint8_t     flags;     /* VRDE_USB_REAP_FLAG_* */
    uint8_t     error;     /* VRDE_USB_XFER_* */
    uint32_t    handle;    /* Handle of returned URB. Not 0. */
    uint32_t    len;       /* Length of data actually transferred. */
    /* 'len' bytes of data follow if direction of this URB was VRDE_USB_DIRECTION_IN. */
};

struct vrde_usb_req_clear_halted_ep
{
    uint8_t code;
    uint32_t id;
    uint8_t ep;
};

struct vrde_usb_req_cancel_urb
{
    uint8_t code;
    uint32_t id;
    uint32_t handle;
};

struct vrde_usb_req_device_list
{
    uint8_t code;
};

#define VRDE_USB_SPEED_UNKNOWN    0 /* Unknown. */
#define VRDE_USB_SPEED_LOW        1 /* Low speed (1.5 Mbit/s). */
#define VRDE_USB_SPEED_FULL       2 /* Full speed (12 Mbit/s). */
#define VRDE_USB_SPEED_HIGH       3 /* High speed (480 Mbit/s). */
#define VRDE_USB_SPEED_VARIABLE   4 /* Variable speed - USB 2.5 / wireless. */
#define VRDE_USB_SPEED_SUPERSPEED 5 /* Super Speed - USB 3.0 */

/* Data is a list of the following variable length structures. */
struct vrde_usb_device_desc
{
    /* Offset of the next structure. 0 if last. */
    uint16_t oNext;

    /* Identifier of the device assigned by client. */
    uint32_t id;

    /** USB version number. */
    uint16_t        bcdUSB;
    /** Device class. */
    uint8_t         bDeviceClass;
    /** Device subclass. */
    uint8_t         bDeviceSubClass;
    /** Device protocol */
    uint8_t         bDeviceProtocol;
    /** Vendor ID. */
    uint16_t        idVendor;
    /** Product ID. */
    uint16_t        idProduct;
    /** Revision, integer part. */
    uint16_t        bcdRev;
    /** Offset of the UTF8 manufacturer string relative to the structure start. */
    uint16_t        oManufacturer;
    /** Offset of the UTF8 product string relative to the structure start. */
    uint16_t        oProduct;
    /** Offset of the UTF8 serial number string relative to the structure start. */
    uint16_t        oSerialNumber;
    /** Physical USB port the device is connected to. */
    uint16_t        idPort;

    /** The USB device speed: VRDE_USBDEVICESPEED_*. */
    uint16_t        u16DeviceSpeed;
};

#define VRDE_USB_NEGOTIATE_FLAGS 6
#define VRDE_USB_NEGOTIATE_VERSION 3
#define VRDE_USB_NEGOTIATE_CLIENT_FLAGS 1

struct vrde_usb_negotiate_reply
{
    uint8_t flags;
    uint32_t u32Version; /* This field presents only if the VRDE_USB_CAPS2_FLAG_VERSION flag is set. */
    uint32_t u32ClientFlags;   /* This field presents only if both VRDE_USB_CAPS2_FLAG_VERSION and
                                * VRDE_USB_CAPS2_FLAG_EXT flag are set.
                                * See VRDE_USB_CLIENT_CAPS_*
                                */
};

#pragma pack()

extern struct vrde_usb_negotiate_reply g_usb_negotiate_reply;

void vrde_set_framebuffer(struct vrde_framebuffer_info *info, int width, int height,
                          enum VRDE_PIXEL_FORMAT format, uint8_t *bits);

struct inqueue;
int vrde_process(struct vrde_framebuffer_info *info, void *data, int size, struct inqueue *inq);
int vrde_process_usb(uint32_t client_id, void *data, int size, struct inqueue *inq);
int vrde_translate_usb_input(int req, const void *in_data, int in_size,
                             uint8_t *out_req, void **out_data, uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif // VRDE_H
