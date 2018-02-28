#ifndef PACKET_H
#define PACKET_H

#include "svp.h"

#pragma pack(push,1)
struct pac_hdr
{
    int16_t c_magic;
    int16_t c_cmd;
    int32_t c_size;
};

struct pac_lz4_hdr
{
    int32_t orig_size;
};

struct pac_screen_resize
{
    int16_t w;
    int16_t h;
};

struct pac_point
{
    int16_t x;
    int16_t y;
};

struct pac_polypoints
{
    uint8_t c;
    struct pac_point a[16];
};

struct pac_rect
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
};

/* Header for bitmap bits. */
struct pac_bitmap
{
    uint32_t size; /* size of bitmap data without the header. */
    int16_t format; /* format */
    int16_t codec; /* compress codec */
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    /* bitmap data follow. */
};

struct pac_solid_rect
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
};

struct pac_solid_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
    uint8_t rop;
};

struct pac_dst_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t rop;
};

struct pac_screen_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int16_t x_src;
    int16_t y_src;
    uint8_t rop;
};

struct pac_pattern_blt_brush
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
typedef uint8_t pac_bitmap_hash_t[16];

struct pac_mem_blt
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int16_t x_src;
    int16_t y_src;
    uint8_t rop;
    pac_bitmap_hash_t hash;
};

struct pac_cached_bitmap
{
    pac_bitmap_hash_t hash;
};

struct pac_deleted_bitmap
{
    pac_bitmap_hash_t hash;
};

struct pac_line
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

struct pac_polyline
{
    struct pac_point pt_start;
    uint8_t  mix;
    uint32_t rgb;
    struct pac_polypoints points;
};

struct pac_ellipse
{
    struct pac_point pt1;
    struct pac_point pt2;
    uint8_t mix;
    uint8_t fill_mode;
    uint32_t rgb;
};

struct pac_save_screen
{
    struct pac_point pt1;
    struct pac_point pt2;
    uint8_t ident;
    uint8_t restore;
};

struct pac_glyph
{
    uint32_t size; /* size of bitmap data WITH the header. */
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

struct pac_text
{
    uint32_t size; /* size of all glyphs and their bitmap data WITH the header. */

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

struct pac_clip_draw
{
    uint16_t cmd_count;
    uint16_t rect_count;
    struct pac_rect rect[];
};

struct pac_cursor
{
    uint16_t x_hot;
    uint16_t y_hot;
    uint16_t w;
    uint16_t h;
    uint16_t mask_len;
    uint16_t data_len;
    /* The 1BPP mask and the 24BPP bitmap follow. */
};

struct pac_mouse_event
{
    int16_t x;
    int16_t y;
    int16_t buttons;
};

struct pac_keyboard_event
{
    uint8_t down;
    uint32_t scancode;
};

struct pac_video_modeset
{
    uint16_t w;
    uint16_t h;
    uint8_t depth;
};

struct pac_graphic_config
{
    uint32_t size;
};

struct pac_main_channel_init
{
    uint32_t client_version; //!< supported features
    uint16_t rtt; //!< round trip time in ms.
    uint16_t bandwidth; //!< xx Mbit/s
};

struct pac_channel_list
{
    uint32_t server_version;
    uint16_t count;
    struct {
        uint16_t type;
        uint16_t port;
    } channel[SVP_MAX_CHANNELS];
};

struct pac_channel_init
{
    uint16_t channel; //!< channel type
    uint16_t queue_length; //!< advise output queue length in server
    uint32_t features; //!< supported features
};

struct pac_channel_ready
{
    uint32_t features; //!< supported features
};

struct pac_usb_device
{
    uint32_t id;

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
};

struct pac_usb_device_list
{
    int32_t count;
    struct pac_usb_device device[];
};

struct pac_usb_status
{
    uint32_t dev_id;
    uint8_t status;
};

struct pac_usb_ids
{
    uint32_t client_id;
    uint32_t dev_id;
};

struct pac_usb_urb
{
    uint32_t id;        /* From which device the URB arrives. */
    uint8_t nodata;     /* contains no data */
    uint8_t flags;      /* SUF_* */
    uint8_t error;      /* SUS_* */
    uint32_t handle;    /* Handle of returned URB. Not 0. */
    uint32_t len;       /* Length of data actually transferred. */
    /* 'len' bytes of data follow if direction of this URB was SUD_IN. */
};

struct pac_usb_set_config
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t config;
};

struct pac_usb_claim_interface
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t iface;
};

struct pac_usb_release_interface
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t iface;
};

struct pac_usb_set_interface
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t iface;
    uint8_t setting;
};

struct pac_usb_queue_urb
{
    uint32_t client_id;
    uint32_t dev_id;
    uint32_t handle;    /* Distinguishes that particular URB. Later used in CancelURB and returned by ReapURB */
    uint8_t type;
    uint8_t ep;
    uint8_t direction;
    uint32_t urblen;    /* Length of the URB. */
    uint32_t datalen;   /* Length of the data. */
    /* In RDP layout the data follow. */
};

struct pac_usb_clear_halted_ep
{
    uint32_t client_id;
    uint32_t dev_id;
    uint8_t ep;
};

struct pac_usb_cancel_urb
{
    uint32_t client_id;
    uint32_t dev_id;
    uint32_t handle;
};

#pragma pack(pop)

static inline int padded_length(int w, int h)
{
    return (((w + 7)/8) * h + 3) & ~3;
}

static inline uint32_t svp_cmd_len(uint16_t cmd, uint8_t *data)
{
    switch (cmd) {
    case SC_DIRTY_RECT: {
        struct pac_bitmap *bitmap = (struct pac_bitmap *)data;
        return sizeof(*bitmap) + bitmap->size;
    }
    case SC_SOLID_RECT:
        return sizeof(struct pac_solid_rect);
    case SC_SOLID_BLT:
        return sizeof(struct pac_solid_blt);
    case SC_DST_BLT:
        return sizeof(struct pac_dst_blt);
    case SC_SCREEN_BLT:
        return sizeof(struct pac_screen_blt);
    case SC_PATTERN_BLT_BRUSH:
        return sizeof(struct pac_pattern_blt_brush);
    case SC_MEM_BLT:
        return sizeof(struct pac_mem_blt);
    case SC_CACHED_BITMAP: {
        struct pac_bitmap *bitmap = (struct pac_bitmap *)(data + sizeof(struct pac_cached_bitmap));
        return sizeof(struct pac_cached_bitmap) + sizeof(*bitmap) + bitmap->size;
    }
    case SC_DELETED_BITMAP:
        return sizeof(struct pac_deleted_bitmap);
    case SC_LINE:
        return sizeof(struct pac_line);
    case SC_POLYLINE:
        return sizeof(struct pac_polyline);
    case SC_ELLIPSE:
        return sizeof(struct pac_ellipse);
    case SC_SAVE_SCREEN:
        return sizeof(struct pac_save_screen);
    case SC_RESTORE_SCREEN: {
        struct pac_save_screen *screen = (struct pac_save_screen *)data;
        struct pac_bitmap *bitmap = (struct pac_bitmap *)(data + sizeof(*screen));
        return sizeof(struct pac_save_screen) + sizeof(struct pac_bitmap) + bitmap->size;
    }
    case SC_TEXT: {
        struct pac_text *text = (struct pac_text *)data;
        return text->size;
    }
    case SC_USB_DEVICE_LIST_QUERY:
        return 0;
    case SC_USB_DEVICE_LIST_RESULT: {
        struct pac_usb_device_list *usb_list = (struct pac_usb_device_list *)data;
        return sizeof(*usb_list) + usb_list->count * sizeof(struct pac_usb_device);
    }
    default:
        return -1;
    }
}

#endif // PACKET_H
