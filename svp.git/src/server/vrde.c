#include "vrde.h"
#include "server.h"
#include "channel.h"
#include "inqueue.h"
#include <stdlib.h>
#include <string.h>

struct vrde_usb_negotiate_reply g_usb_negotiate_reply = {
    VRDE_USB_NEGOTIATE_FLAGS,
    VRDE_USB_NEGOTIATE_VERSION,
    VRDE_USB_NEGOTIATE_CLIENT_FLAGS
};

__thread uint8_t g_vbuf[1024 * 1024];

int vrde_process_orders(struct vrde_framebuffer_info *info, void *data, int size,
                        struct inqueue *inq, struct svp_rect *clip_rects, int *clip_count);

void vrde_set_framebuffer(struct vrde_framebuffer_info *info, int width, int height,
                          enum VRDE_PIXEL_FORMAT format, uint8_t *bits)
{
    if (!info)
        return;
    info->width = width;
    info->height = height;
    info->format = format;
    info->bits = bits;
    switch (format) {
    case VRDE_RGB32:
        info->bpp = 32;
        break;
    case VRDE_BGR24:
        info->bpp = 24;
        break;
    case VRDE_RGB16:
        info->bpp = 16;
        break;
    case VRDE_RGB8:
        info->bpp = 8;
        break;
    default:
        dzlog_error("unknown vrde framebuffer format %d", format);
        info->bpp = 0;
    }
}

static void vrde_copy_framebuffer(uint8_t *bits, struct vrde_framebuffer_info *info,
                                        struct vrde_hdr *hdr)
{
    uint8_t *src = info->bits;
    uint8_t *dst = bits;
    int src_stride = info->width * info->bpp / 8;
    int dst_stride = hdr->w * info->bpp / 8;
    int n = hdr->h;
    if (!src || !dst)
        return;
    src += hdr->y * src_stride + hdr->x * info->bpp / 8;
    while (n > 0) {
        memcpy(dst, src, dst_stride);
        src += src_stride;
        dst += dst_stride;
        n--;
    }
}

static inline enum SVP_PIXEL_FORMAT vrde_pixel_format(enum VRDE_PIXEL_FORMAT format)
{
    switch (format) {
    case VRDE_RGB32:
        return SPF_RGB32;
    case VRDE_BGR24:
        return SPF_BGR24;
    case VRDE_RGB16:
        return SPF_RGB16;
    case VRDE_RGB8:
        return SPF_RGB8;
    default:
        return SPF_INVALID;
    }
}

static int vrde_set_bitmap(struct svp_bitmap *bitmap, struct vrde_databits *hdr, uint8_t *bits)
{
    if (!bitmap || !hdr || !bits)
        return -1;
    if (hdr->Bpp == 4)
        bitmap->format = SPF_RGB32;
    else if (hdr->Bpp == 3)
        bitmap->format = SPF_BGR24;
    else if (hdr->Bpp == 2)
        bitmap->format = SPF_RGB16;
    else if (hdr->Bpp == 1)
        bitmap->format = SPF_RGB8;
    else {
        dzlog_error("unknown pixel format for bitmap with Bpp %d", hdr->Bpp);
        bitmap->format = SPF_INVALID;
        return -1;
    }
    bitmap->x = hdr->x;
    bitmap->y = hdr->y;
    bitmap->w = hdr->w;
    bitmap->h = hdr->h;
    bitmap->size = hdr->size;
    bitmap->bits = (uint8_t *)malloc(bitmap->size);
    if (!bitmap->bits)
        return -1;
    memcpy(bitmap->bits, bits, bitmap->size);
    return 0;
}

int vrde_process_dirty_rect(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_databits *v_bits;
    uint8_t *v_data;
    struct svp_bitmap *s_bitmap;

    v_bits = (struct vrde_databits *)*pdata;
    v_data = *pdata + sizeof(*v_bits);

    if (*psize < 0 || *psize < sizeof(struct vrde_databits) + v_bits->size) {
        dzlog_debug("bad vrde_dirty_rect size, expect %d got %d",
                sizeof(struct vrde_databits) + v_bits->size, *psize);
        return -1;
    }

    cmd->type = SC_DIRTY_RECT;
    cmd->size = sizeof(struct svp_bitmap);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_bitmap = (struct svp_bitmap *)cmd->data;
    s_bitmap->x = v_bits->x;
    s_bitmap->y = v_bits->y;
    s_bitmap->w = v_bits->w;
    s_bitmap->h = v_bits->h;
    s_bitmap->size = v_bits->size;
    switch (v_bits->Bpp) {
    case 1:
        s_bitmap->format = SPF_RGB8;
        break;
    case 2:
        s_bitmap->format = SPF_RGB16;
        break;
    case 3:
        s_bitmap->format = SPF_BGR24;
        break;
    case 4:
        s_bitmap->format = SPF_RGB32;
        break;
    default:
        dzlog_debug("bad vrde_databits Bpp %d", v_bits->Bpp);
    }
    s_bitmap->bits = (uint8_t *)malloc(s_bitmap->size);
    if (!s_bitmap->bits) {
        free(s_bitmap);
        return -1;
    }
    memcpy(s_bitmap->bits, v_data, s_bitmap->size);

    *pdata += sizeof(*v_bits) + v_bits->size;
    *psize -= sizeof(*v_bits) + v_bits->size;

    return 0;
}

int vrde_process_solid_rect(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_solid_rect *v_solid_rect;
    struct svp_solid_rect *s_solid_rect;

    if (*psize < 0 || *psize < sizeof(struct vrde_solid_rect)) {
        dzlog_debug("bad vrde_solid_rect size, expect %d got %d",
                sizeof(struct vrde_solid_rect), *psize);
        return -1;
    }

    cmd->type = SC_SOLID_RECT;
    cmd->size = sizeof(struct svp_solid_rect);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_solid_rect = (struct svp_solid_rect *)cmd->data;
    v_solid_rect = (struct vrde_solid_rect *)*pdata;
    s_solid_rect->x = v_solid_rect->x;
    s_solid_rect->y = v_solid_rect->y;
    s_solid_rect->w = v_solid_rect->w;
    s_solid_rect->h = v_solid_rect->h;
    s_solid_rect->rgb = v_solid_rect->rgb;

    *pdata += sizeof(*v_solid_rect);
    *psize -= sizeof(*v_solid_rect);

    return 0;
}

int vrde_process_solid_blt(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_solid_blt *v_solid_blt;
    struct svp_solid_blt *s_solid_blt;

    if (*psize < 0 || *psize < sizeof(struct vrde_solid_blt)) {
        dzlog_debug("bad vrde_solid_blt size, expect %d got %d",
                sizeof(struct vrde_solid_blt), *psize);
        return -1;
    }

    cmd->type = SC_SOLID_BLT;
    cmd->size = sizeof(struct svp_solid_blt);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_solid_blt = (struct svp_solid_blt *)cmd->data;
    v_solid_blt = (struct vrde_solid_blt *)*pdata;
    s_solid_blt->x = v_solid_blt->x;
    s_solid_blt->y = v_solid_blt->y;
    s_solid_blt->w = v_solid_blt->w;
    s_solid_blt->h = v_solid_blt->h;
    s_solid_blt->rgb = v_solid_blt->rgb;
    s_solid_blt->rop = v_solid_blt->rop;

    *pdata += sizeof(*v_solid_blt);
    *psize -= sizeof(*v_solid_blt);

    return 0;
}

int vrde_process_dst_blt(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_dst_blt *v_dst_blt;
    struct svp_dst_blt *s_dst_blt;

    if (*psize < 0 || *psize < sizeof(struct vrde_dst_blt)) {
        dzlog_debug("bad vrde_dst_blt size, expect %d got %d",
                sizeof(struct vrde_dst_blt), *psize);
        return -1;
    }

    cmd->type = SC_DST_BLT;
    cmd->size = sizeof(struct svp_dst_blt);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_dst_blt = (struct svp_dst_blt *)cmd->data;
    v_dst_blt = (struct vrde_dst_blt *)*pdata;
    s_dst_blt->x = v_dst_blt->x;
    s_dst_blt->y = v_dst_blt->y;
    s_dst_blt->w = v_dst_blt->w;
    s_dst_blt->h = v_dst_blt->h;
    s_dst_blt->rop = v_dst_blt->rop;

    *pdata += sizeof(*v_dst_blt);
    *psize -= sizeof(*v_dst_blt);

    return 0;
}

int vrde_process_screen_blt(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_screen_blt *v_screen_blt;
    struct svp_screen_blt *s_screen_blt;

    if (*psize < 0 || *psize < sizeof(struct vrde_screen_blt)) {
        dzlog_debug("bad vrde_screen_blt size, expect %d got %d",
                sizeof(struct vrde_screen_blt), *psize);
        return -1;
    }

    cmd->type = SC_SCREEN_BLT;
    cmd->size = sizeof(struct svp_screen_blt);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_screen_blt = (struct svp_screen_blt *)cmd->data;
    v_screen_blt = (struct vrde_screen_blt *)*pdata;
    s_screen_blt->x = v_screen_blt->x;
    s_screen_blt->y = v_screen_blt->y;
    s_screen_blt->w = v_screen_blt->w;
    s_screen_blt->h = v_screen_blt->h;
    s_screen_blt->x_src = v_screen_blt->x_src;
    s_screen_blt->y_src = v_screen_blt->y_src;
    s_screen_blt->rop = v_screen_blt->rop;

    *pdata += sizeof(*v_screen_blt);
    *psize -= sizeof(*v_screen_blt);

    return 0;
}

int vrde_process_pat_blt_brush(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_pat_blt_brush *v_pat_blt_brush;
    struct svp_pat_blt_brush *s_pat_blt_brush;

    if (*psize < 0 || *psize < sizeof(struct vrde_pat_blt_brush)) {
        dzlog_debug("bad vrde_pat_blt_brush size, expect %d got %d",
                sizeof(struct vrde_pat_blt_brush), *psize);
        return -1;
    }

    cmd->type = SC_PATTERN_BLT_BRUSH;
    cmd->size = sizeof(struct svp_pat_blt_brush);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_pat_blt_brush = (struct svp_pat_blt_brush *)cmd->data;
    v_pat_blt_brush = (struct vrde_pat_blt_brush *)*pdata;
    s_pat_blt_brush->x = v_pat_blt_brush->x;
    s_pat_blt_brush->y = v_pat_blt_brush->y;
    s_pat_blt_brush->w = v_pat_blt_brush->w;
    s_pat_blt_brush->h = v_pat_blt_brush->h;
    s_pat_blt_brush->x_src = v_pat_blt_brush->x_src;
    s_pat_blt_brush->y_src = v_pat_blt_brush->y_src;
    s_pat_blt_brush->rgb_fg = v_pat_blt_brush->rgb_fg;
    s_pat_blt_brush->rgb_bg = v_pat_blt_brush->rgb_bg;
    s_pat_blt_brush->rop = v_pat_blt_brush->rop;
    if (sizeof(s_pat_blt_brush->pattern) != sizeof(v_pat_blt_brush))
        return -1;
    memcpy(s_pat_blt_brush->pattern, v_pat_blt_brush->pattern, sizeof(v_pat_blt_brush->pattern));

    *pdata += sizeof(*v_pat_blt_brush);
    *psize -= sizeof(*v_pat_blt_brush);

    return 0;
}

int vrde_process_mem_blt(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_mem_blt *v_mem_blt;
    struct svp_mem_blt *s_mem_blt;

    if (*psize < 0 || *psize < sizeof(struct vrde_mem_blt)) {
        dzlog_debug("bad vrde_mem_blt size, expect %d got %d",
                sizeof(struct vrde_mem_blt), *psize);
        return -1;
    }

    cmd->type = SC_MEM_BLT;
    cmd->size = sizeof(struct svp_mem_blt);
    cmd->data = (uint8_t *)malloc(cmd->size);

    s_mem_blt = (struct svp_mem_blt *)cmd->data;
    v_mem_blt = (struct vrde_mem_blt *)*pdata;
    s_mem_blt->x = v_mem_blt->x;
    s_mem_blt->y = v_mem_blt->y;
    s_mem_blt->w = v_mem_blt->w;
    s_mem_blt->h = v_mem_blt->h;
    s_mem_blt->x_src = v_mem_blt->x_src;
    s_mem_blt->y_src = v_mem_blt->y_src;
    s_mem_blt->rop = v_mem_blt->rop;
    if (sizeof(s_mem_blt->hash) != sizeof(v_mem_blt->hash))
        return -1;
    memcpy(s_mem_blt->hash, v_mem_blt->hash, sizeof(v_mem_blt->hash));

    *pdata += sizeof(*v_mem_blt);
    *psize -= sizeof(*v_mem_blt);

    return 0;
}

int vrde_process_cached_bitmap(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_cached_bitmap *v_cache;
    struct vrde_databits *v_databits;
    uint8_t *v_bits;
    struct svp_cached_bitmap *s_cache = 0;

    if (*psize < 0 || *psize < sizeof(*v_cache) + sizeof(*v_databits)) {
        dzlog_debug("bad vrde_cached_bitmap size, expect at least %d got %d",
                sizeof(*v_cache) + sizeof(*v_databits), *psize);
        return -1;
    }
    v_cache = (struct vrde_cached_bitmap *)*pdata;
    v_databits = (struct vrde_databits *)(*pdata + sizeof(*v_cache));
    v_bits = *pdata + sizeof(*v_cache) + sizeof(*v_databits);
    if (*psize < sizeof(*v_cache) + sizeof(*v_databits) + v_databits->size) {
        dzlog_error("bad vrde_cached_bitmap size, at least %d got %d",
                sizeof(*v_cache) + sizeof(*v_databits) + v_databits->size, *psize);
        return -1;
    }
    cmd->type = SC_CACHED_BITMAP;
    cmd->size = sizeof(*s_cache);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_cache = (struct svp_cached_bitmap *)cmd->data;
    if (!s_cache)
        goto out_error;
    memset(s_cache, 0, sizeof(*s_cache));
    memcpy(s_cache->hash, v_cache->hash, sizeof(v_cache->hash));
    if (vrde_set_bitmap(&s_cache->bitmap, v_databits, v_bits) != 0) {
        dzlog_error("vrde_cached_bitmap set bitmap error");
        goto out_error;
    }
    *psize -= v_bits + v_databits->size - *pdata;
    *pdata += v_bits + v_databits->size - *pdata;
    return 0;
out_error:
    if (s_cache)
        free(s_cache->bitmap.bits);
    free(s_cache);
    return -1;
}

int vrde_process_deleted_bitmap(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_deleted_bitmap *v_cache;
    struct svp_deleted_bitmap *s_cache = 0;

    if (*psize < 0 || *psize < sizeof(*v_cache)) {
        dzlog_debug("bad vrde_deleted_bitmap size, expect %d got %d",
                sizeof(*v_cache), *psize);
        return -1;
    }
    v_cache = (struct vrde_deleted_bitmap *)*pdata;
    cmd->type = SC_DELETED_BITMAP;
    cmd->size = sizeof(*s_cache);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_cache = (struct svp_deleted_bitmap *)cmd->data;
    if (!s_cache)
        goto out_error;
    memset(s_cache, 0, sizeof(*s_cache));
    memcpy(s_cache->hash, v_cache->hash, sizeof(v_cache->hash));
    *psize -= sizeof(*v_cache);
    *pdata += sizeof(*v_cache);
    return 0;
out_error:
    free(s_cache);
    return -1;
}

int vrde_process_line(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_line *v_line;
    struct svp_line *s_line = 0;

    if (*psize < 0 || *psize < sizeof(*v_line)) {
        dzlog_debug("bad vrde_line size, expect %d got %d",
                sizeof(*v_line), *psize);
        return -1;
    }
    v_line = (struct vrde_line *)*pdata;
    cmd->type = SC_LINE;
    cmd->size = sizeof(*s_line);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_line = (struct svp_line *)cmd->data;
    if (!s_line)
        goto out_error;
    memset(s_line, 0, sizeof(*s_line));
    s_line->x1 = v_line->x1;
    s_line->y1 = v_line->y1;
    s_line->x2 = v_line->x2;
    s_line->y2 = v_line->y2;
    s_line->x_bounds1 = v_line->x_bounds1;
    s_line->y_bounds1 = v_line->y_bounds1;
    s_line->x_bounds2 = v_line->x_bounds2;
    s_line->y_bounds2 = v_line->y_bounds2;
    s_line->mix = v_line->mix;
    s_line->rgb = v_line->rgb;

    *psize -= sizeof(*v_line);
    *pdata += sizeof(*v_line);
    return 0;
out_error:
    free(s_line);
    return -1;
}

int vrde_process_polyline(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_polyline *v_polyline;
    struct svp_polyline *s_polyline = 0;
    int i;

    if (*psize < 0 || *psize < sizeof(*v_polyline)) {
        dzlog_debug("bad vrde_polyline size, expect %d got %d",
                sizeof(*v_polyline), *psize);
        return -1;
    }
    v_polyline = (struct vrde_polyline *)*pdata;
    cmd->type = SC_POLYLINE;
    cmd->size = sizeof(*s_polyline);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_polyline = (struct svp_polyline *)cmd->data;
    if (!s_polyline)
        goto out_error;
    memset(s_polyline, 0, sizeof(*s_polyline));
    s_polyline->x_start = v_polyline->pt_start.x;
    s_polyline->y_start = v_polyline->pt_start.y;
    s_polyline->mix = v_polyline->mix;
    s_polyline->rgb = v_polyline->rgb;
    if (v_polyline->points.c > 16) {
        dzlog_debug("polyline has too many points, at most %d got %d", 16, v_polyline->points.c);
        goto out_error;
    }
    s_polyline->count = v_polyline->points.c;
    for (i = 0; i < s_polyline->count; i++) {
        s_polyline->points[i].x = v_polyline->points.a[i].x;
        s_polyline->points[i].y = v_polyline->points.a[i].y;
    }
    *psize -= sizeof(*v_polyline);
    *pdata += sizeof(*v_polyline);
    return 0;
out_error:
    free(s_polyline);
    return -1;
}

int vrde_process_ellipse(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_ellipse *v_ellipse;
    struct svp_ellipse *s_ellipse = 0;

    if (*psize < 0 || *psize < sizeof(*v_ellipse)) {
        dzlog_debug("bad vrde_ellipse size at least %d got %d",
                sizeof(*v_ellipse), *psize);
        return -1;
    }
    v_ellipse = (struct vrde_ellipse *)*pdata;
    cmd->type = SC_ELLIPSE;
    cmd->size = sizeof(*s_ellipse);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_ellipse = (struct svp_ellipse *)cmd->data;
    if (!s_ellipse)
        goto out_error;
    memset(s_ellipse, 0, sizeof(*s_ellipse));
    s_ellipse->x = v_ellipse->pt1.x;
    s_ellipse->y = v_ellipse->pt1.y;
    s_ellipse->w = v_ellipse->pt2.x - v_ellipse->pt1.x;
    s_ellipse->h = v_ellipse->pt2.y - v_ellipse->pt1.y;
    if (s_ellipse->w < 0 || s_ellipse->h < 0) {
        dzlog_debug("negative size ellipse w %d h %d", s_ellipse->w, s_ellipse->h);
        goto out_error;
    }
    s_ellipse->mix = v_ellipse->mix;
    s_ellipse->fill_mode = v_ellipse->fill_mode;
    s_ellipse->rgb = v_ellipse->rgb;
    *psize -= sizeof(*v_ellipse);
    *pdata += sizeof(*v_ellipse);
    return 0;
out_error:
    free(s_ellipse);
    return -1;
}

int vrde_process_save_screen(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_save_screen *v_save;
    struct vrde_databits *v_databits;
    uint8_t *v_bits;
    struct svp_save_screen *s_save = 0;
    struct svp_restore_screen *s_restore = 0;

    if (*psize < 0 || *psize < sizeof(*v_save)) {
        dzlog_error("bad vrde_save_screen size, at least %d got %d",
                sizeof(*v_save), *psize);
        return -1;
    }
    v_save = (struct vrde_save_screen *)*pdata;
    if (v_save->restore) {
        if (*psize < sizeof(*v_save) + sizeof(*v_databits)) {
            dzlog_error("bad vrde_save_screen size, at least %d got %d",
                    sizeof(*v_save) + sizeof(*v_databits), *psize);
            return -1;
        }
        v_databits = (struct vrde_databits *)(*pdata + sizeof(*v_save));
        if (*psize < sizeof(*v_save) + sizeof(*v_databits) + v_databits->size) {
            dzlog_error("bad vrde_save_screen size, at least %d got %d",
                    sizeof(*v_save) + sizeof(*v_databits) + v_databits->size, *psize);
            return -1;
        }
        v_bits = *pdata + sizeof(*v_save) + sizeof(*v_databits);
        cmd->type = SC_RESTORE_SCREEN;
        cmd->size = sizeof(*s_restore);
        cmd->data = (uint8_t *)malloc(cmd->size);
        s_restore = (struct svp_restore_screen *)cmd->data;;
        if (!s_restore)
            goto out_error;
        memset(s_restore, 0, sizeof(*s_restore));
        s_restore->ident = v_save->ident;
        s_restore->x = v_save->pt1.x;
        s_restore->y = v_save->pt1.y;
        s_restore->w = v_save->pt2.x - v_save->pt1.x;
        s_restore->h = v_save->pt2.y - v_save->pt1.y;
        if (s_restore->w < 0 || s_restore->h < 0) {
            dzlog_error("negative save_screen size w %d h %d", s_restore->w, s_restore->h);
            goto out_error;
        }
        if (vrde_set_bitmap(&s_restore->bitmap, v_databits, v_bits) != 0) {
            dzlog_debug("vrde_save_screen set restore bitmap error");
            goto out_error;
        }
        *psize -= sizeof(*v_save) + sizeof(*v_databits) + v_databits->size;
        *pdata += sizeof(*v_save) + sizeof(*v_databits) + v_databits->size;
    } else {
        cmd->type = SC_SAVE_SCREEN;
        cmd->size = sizeof(*s_save);
        cmd->data = (uint8_t *)malloc(cmd->size);
        s_save = (struct svp_save_screen *)cmd->data;;
        if (!s_save)
            goto out_error;
        s_save->ident = v_save->ident;
        s_save->x = v_save->pt1.x;
        s_save->y = v_save->pt1.y;
        s_save->w = v_save->pt2.x - v_save->pt1.x;
        s_save->h = v_save->pt2.y - v_save->pt1.y;
        if (s_save->w < 0 || s_save->h < 0) {
            dzlog_debug("negative save_screen size w %d h %d", s_save->w, s_save->h);
            goto out_error;
        }
        *psize -= sizeof(*v_save);
        *pdata += sizeof(*v_save);
    }
    return 0;
out_error:
    free(s_save);
    if (s_restore)
        free(s_restore->bitmap.bits);
    free(s_restore);
    return -1;
}

int vrde_process_text(uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    // FIXME: process text order size bug
    struct vrde_text *v_text;
    struct vrde_glyph *v_glyph;
    struct svp_text *s_text = 0;
    struct svp_glyph *s_glyph = 0;
    struct svp_glyph *s_tmp = 0;
    int i;
    uint8_t *v_ptr;

    if (*psize < 0 || *psize < sizeof(*v_text)) {
        dzlog_debug("bad vrde_text size, expect at least %d got %d",
                sizeof(*v_text), *psize);
        return -1;
    }
    v_text = (struct vrde_text *)*pdata;
    if (*psize < sizeof(*v_text) + sizeof(*v_glyph) * v_text->glyphs) {
        dzlog_debug("bad vrde_text size, expect at least %d got %d",
                sizeof(*v_text) + sizeof(*v_glyph) * v_text->glyphs, *psize);
        return -1;
    }

    cmd->type = SC_TEXT;
    cmd->size = sizeof(*s_text);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_text = (struct svp_text *)cmd->data;
    if (!s_text)
        goto out_error;
    memset(s_text, 0, sizeof(*s_text));
    INIT_LIST_HEAD(&s_text->glyph_list);
    s_text->x_bg = v_text->x_bg;
    s_text->y_bg = v_text->y_bg;
    s_text->w_bg = v_text->w_bg;
    s_text->h_bg = v_text->h_bg;
    s_text->x_opaque = v_text->x_opaque;
    s_text->y_opaque = v_text->y_opaque;
    s_text->w_opaque = v_text->w_opaque;
    s_text->h_opaque = v_text->h_opaque;
    s_text->max_glyph = v_text->max_glyph;
    s_text->flags = v_text->flags;
    s_text->char_inc = v_text->char_inc;
    s_text->rgb_fg = v_text->rgb_fg;
    s_text->rgb_bg = v_text->rgb_bg;
    s_text->glyph_count = v_text->glyphs;
    v_ptr = *pdata + sizeof(*v_text);
    for (i = 0; i < v_text->glyphs; i++) {
        if (*psize < v_ptr - *pdata) {
            dzlog_debug("bad vrde_text size, at least %d got %d",
                    v_ptr - *pdata, *psize);
            goto out_error;
        }
        v_glyph = (struct vrde_glyph *)v_ptr;
        if (v_glyph->w < 0 || v_glyph->h < 0
                || v_glyph->size < sizeof(*v_glyph) + padded_length(v_glyph->w, v_glyph->h)) {
            dzlog_debug("bad vrde_glyph size, at least %d got %d",
                    sizeof(*v_glyph) + padded_length(v_glyph->w, v_glyph->h), v_glyph->size);
            goto out_error;
        }
        s_glyph = (struct svp_glyph *)malloc(sizeof(*s_glyph));
        if (!s_glyph)
            goto out_error;
        list_add_tail(&s_glyph->link, &s_text->glyph_list);
        s_glyph->handle = v_glyph->handle;
        s_glyph->x_origin = v_glyph->x_origin;
        s_glyph->y_origin = v_glyph->y_origin;
        s_glyph->bitmap.format = SPF_MONO;
        s_glyph->bitmap.x = v_glyph->x;
        s_glyph->bitmap.y = v_glyph->y;
        s_glyph->bitmap.w = v_glyph->w;
        s_glyph->bitmap.h = v_glyph->h;
        s_glyph->bitmap.size = v_glyph->size - sizeof(*v_glyph);
        s_glyph->bitmap.bits = (uint8_t *)malloc(s_glyph->bitmap.size);
        if (!s_glyph->bitmap.bits) {
            list_del(&s_glyph->link);
            free(s_glyph);
            s_glyph = 0;
            goto out_error;
        }
        memcpy(s_glyph->bitmap.bits, v_ptr + sizeof(*v_glyph), s_glyph->bitmap.size);
        v_ptr += v_glyph->size;
    }
    *psize -= v_ptr - *pdata;
    *pdata += v_ptr - *pdata;
    return 0;
out_error:
    if (s_text) {
        list_for_each_entry_safe(s_glyph, s_tmp, &s_text->glyph_list, link) {
            free(s_glyph->bitmap.bits);
            free(s_glyph);
        }
    }
    free(s_text);
    return -1;
}

int vrde_process_bounds(struct vrde_framebuffer_info *info, uint8_t **pdata, int *psize, struct in_cmd *cmd)
{
    struct vrde_bounds *v_bounds;
    struct svp_clip_draw *s_clip_draw = 0;
    struct inqueue *inq = inq_create();
    struct in_cmd sub_cmd;
    int rc;
    int i;

    if (*psize < 0 || *psize < sizeof(*v_bounds)) {
        dzlog_debug("bad vrde_bounds size at least %d got %d",
                sizeof(*v_bounds), *psize);
        return -1;
    }
    if (!inq) {
        dzlog_debug("create inqueue error");
        return -1;
    }
    v_bounds = (struct vrde_bounds *)*pdata;
    cmd->type = SC_CLIP_DRAW;
    cmd->size = sizeof(*s_clip_draw);
    cmd->data = (uint8_t *)malloc(cmd->size);
    s_clip_draw = (struct svp_clip_draw *)cmd->data;
    if (!s_clip_draw)
        goto out_error;
    memset(s_clip_draw, 0, sizeof(*s_clip_draw));
    s_clip_draw->rect_count = 1;
    s_clip_draw->rect[0].x = v_bounds->pt1.x;
    s_clip_draw->rect[0].y = v_bounds->pt1.y;
    s_clip_draw->rect[0].w = v_bounds->pt2.x - v_bounds->pt1.x;
    s_clip_draw->rect[0].h = v_bounds->pt2.y - v_bounds->pt1.y;
    if (s_clip_draw->rect[0].w < 0 || s_clip_draw->rect[0].h < 0) {
        dzlog_debug("negative clip bounds w %d h %d", s_clip_draw->rect[0].w, s_clip_draw->rect[0].h);
        goto out_error;
    }
    *psize -= sizeof(*v_bounds);
    *pdata += sizeof(*v_bounds);
    rc = vrde_process_orders(info, *pdata, *psize, inq, s_clip_draw->rect, &s_clip_draw->rect_count);
    *pdata += *psize;
    *psize = 0;
    if (rc) {
        dzlog_debug("process sub_cmd error");
        goto out_error;
    }
    s_clip_draw->cmd_count = inq_size(inq);
    if (s_clip_draw->cmd_count > SVP_MAX_CLIP_CMD) {
        dzlog_debug("too many sub cmds %d", s_clip_draw->cmd_count);
        goto out_error;
    }
    i = 0;
    while (inq_pop(inq, &sub_cmd) == 1 && i < SVP_MAX_CLIP_CMD) {
        s_clip_draw->cmd[i].type = sub_cmd.type;
        s_clip_draw->cmd[i].data = sub_cmd.data;
        s_clip_draw->cmd[i].size = sub_cmd.size;
        i++;
    }
    if (i != s_clip_draw->cmd_count) {
        dzlog_debug("sub cmd count error, expect %d got %d", s_clip_draw->cmd_count, i);
        s_clip_draw->cmd_count = i; // for cleanup usage
        goto out_error;
    }
    return 0;
out_error:
    inq_clear(inq, in_cmd_cleanup);
    inq_destroy(inq);
    if (s_clip_draw) {
        for (i = 0; i < s_clip_draw->cmd_count; i++) {
            sub_cmd.type = s_clip_draw->cmd[i].type;
            sub_cmd.data = s_clip_draw->cmd[i].data;
            sub_cmd.size = s_clip_draw->cmd[i].size;
            in_cmd_cleanup(&sub_cmd);
        }
    }
    free(s_clip_draw);
    return -1;
}

int vrde_process_repeat(struct vrde_framebuffer_info *info, uint8_t **pdata, int *psize,
                        struct in_cmd *cmd, struct svp_rect *clip_rects, int *clip_count)
{
    struct vrde_repeat *v_repeat;

    if (!clip_rects || !clip_count) {
        dzlog_debug("error: encounter vrde_repeat without vrde_bounds.");
        goto out_error;
    }
    if (*clip_count < 0) {
        dzlog_debug("bad clip_count %d", *clip_count);
        goto out_error;
    }
    if (*clip_count >= SVP_MAX_CLIP_RECT) {
        dzlog_debug("warning: too many clip rects, exceeds limit %d", SVP_MAX_CLIP_RECT);
        *psize -= sizeof(*v_repeat);
        *pdata += sizeof(*v_repeat);
        return 0;
    }
    if (*psize < sizeof(*v_repeat)) {
        dzlog_debug("bad vrde_repeat size, expect %d got %d",
                sizeof(*v_repeat), *psize);
        return -1;
    }
    cmd->type = SC_NOP;
    cmd->data = 0;
    cmd->size = 0;
    v_repeat = (struct vrde_repeat *)*pdata;
    clip_rects[*clip_count].x = v_repeat->bounds.pt1.x;
    clip_rects[*clip_count].y = v_repeat->bounds.pt1.y;
    clip_rects[*clip_count].w = v_repeat->bounds.pt2.x - v_repeat->bounds.pt1.x;
    clip_rects[*clip_count].h = v_repeat->bounds.pt2.y - v_repeat->bounds.pt1.y;
    if (clip_rects[*clip_count].w < 0 || clip_rects[*clip_count].h < 0) {
        dzlog_debug("negative clip rect w %d h %d", clip_rects[*clip_count].w, clip_rects[*clip_count].h);
        goto out_error;
    }
    *clip_count += 1;

    *psize -= sizeof(*v_repeat);
    *pdata += sizeof(*v_repeat);
    return 0;
out_error:
    return -1;
}

int vrde_process_orders(struct vrde_framebuffer_info *info, void *data, int size,
                        struct inqueue *inq, struct svp_rect *clip_rects, int *clip_count)
{
    struct in_cmd cmd;
    uint32_t order;
    int len = size;
    uint8_t *ptr = (uint8_t *)data;
    int rc;

    while (len > 0) {
        if (len < sizeof(uint32_t)) {
            dzlog_debug("bad vrde order size, expect %d got %d",
                    sizeof(uint32_t), len);
            goto out_error;
        }
        order = *(uint32_t *)ptr;
        ptr += sizeof(uint32_t);
        len -= sizeof(uint32_t);

        switch (order) {
        case VRDE_DIRTY_RECT:
            rc = vrde_process_dirty_rect(&ptr, &len, &cmd);
            break;
        case VRDE_SOLID_RECT:
            rc = vrde_process_solid_rect(&ptr, &len, &cmd);
            break;
        case VRDE_SOLID_BLT:
            rc = vrde_process_solid_blt(&ptr, &len, &cmd);
            break;
        case VRDE_DST_BLT:
            rc = vrde_process_dst_blt(&ptr, &len, &cmd);
            break;
        case VRDE_SCREEN_BLT:
            rc = vrde_process_screen_blt(&ptr, &len, &cmd);
            break;
        case VRDE_PAT_BLT_BRUSH:
            rc = vrde_process_pat_blt_brush(&ptr, &len, &cmd);
            break;
        case VRDE_MEM_BLT:
            rc = vrde_process_mem_blt(&ptr, &len, &cmd);
            break;
        case VRDE_CACHED_BITMAP:
            rc = vrde_process_cached_bitmap(&ptr, &len, &cmd);
            break;
        case VRDE_DELETED_BITMAP:
            rc = vrde_process_deleted_bitmap(&ptr, &len, &cmd);
            break;
        case VRDE_LINE:
            rc = vrde_process_line(&ptr, &len, &cmd);
            break;
        case VRDE_POLYLINE:
            rc = vrde_process_polyline(&ptr, &len, &cmd);
            break;
        case VRDE_ELLIPSE:
            rc = vrde_process_ellipse(&ptr, &len, &cmd);
            break;
        case VRDE_SAVE_SCREEN:
            rc = vrde_process_save_screen(&ptr, &len, &cmd);
            break;
        case VRDE_TEXT:
            rc = vrde_process_text(&ptr, &len, &cmd);
            break;
        case VRDE_BOUNDS:
            rc = vrde_process_bounds(info, &ptr, &len, &cmd);
            break;
        case VRDE_REPEAT:
            rc = vrde_process_repeat(info, &ptr, &len, &cmd, clip_rects, clip_count);
            break;
        default:
            dzlog_debug("unknown vrde order %d", order);
            rc = -1;
        }

        if (rc) {
            dzlog_debug("process vrde order %d error", order);
            goto out_error;
        }
        if (cmd.type != SC_NOP)
            inq_push(inq, cmd.type, cmd.data, cmd.size);
    }
    if (len == 0)
        return 0;
out_error:
    inq_clear(inq, in_cmd_cleanup);
    return -1;
}

int vrde_process(struct vrde_framebuffer_info *info, void *data, int size, struct inqueue *inq)
{
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    struct svp_bitmap *rect;
    int bits_size;

    if (!info || !data || size < 0 || size < sizeof(*hdr) || !inq)
        return -1;
    if (size == sizeof(*hdr)) {
        bits_size = hdr->w * hdr->h * info->bpp / 8;
        rect = (struct svp_bitmap *)malloc(sizeof(*rect));
        if (!rect)
            return -1;
        rect->x = hdr->x;
        rect->y = hdr->y;
        rect->w = hdr->w;
        rect->h = hdr->h;
        rect->size = hdr->w * hdr->h * info->bpp / 8;
        rect->format = vrde_pixel_format(info->format);
        rect->bits = (uint8_t *)malloc(bits_size);
        if (rect->format == SPF_INVALID) {
            free(rect);
            return -1;
        }
        vrde_copy_framebuffer(rect->bits, info, hdr);
        inq_push(inq, SC_DIRTY_RECT, rect, sizeof(*rect));
    } else {
        if (vrde_process_orders(info, (uint8_t *)data + sizeof(*hdr), size - sizeof(*hdr), inq, 0, 0) != 0)
            goto out_error;
    }
    return 0;

out_error:
    inq_clear(inq, in_cmd_cleanup);
    return -1;
}

#define CHECK_SIZE(expected, size, msgTypeString) \
    if (size != expected) { \
        dzlog_error("bad %s size expect %d got %d", msgTypeString, expected, size); \
        return -1; \
    }

int vrde_process_usb(uint32_t client_id, void *data, int size, struct inqueue *inq)
{
    union
    {
        uint8_t code;
        struct vrde_usb_req_open reqOpen;
        struct vrde_usb_req_close reqClose;
        struct vrde_usb_req_reset reqReset;
        struct vrde_usb_req_set_config reqSetConfig;
        struct vrde_usb_req_claim_interface reqClaimInterface;
        struct vrde_usb_req_release_interface reqReleaseInterface;
        struct vrde_usb_req_interface_setting reqInterfaceSetting;
        struct vrde_usb_req_queue_urb reqQueueURB;
        struct vrde_usb_req_reap_urb reqReapURB;
        struct vrde_usb_req_clear_halted_ep reqClearHaltedEP;
        struct vrde_usb_req_cancel_urb reqCancelURB;
        struct vrde_usb_req_device_list reqDeviceList;
    } *u = (typeof(u))data;
    struct in_cmd cmd;
    struct svp_usb_ids *ids;
    struct svp_usb_set_config *set_config;
    struct svp_usb_claim_interface *claim_iface;
    struct svp_usb_release_interface *relse_iface;
    struct svp_usb_set_interface *set_iface;
    struct svp_usb_queue_urb *queue_urb;
    struct svp_usb_clear_halted_ep *clr_ep;
    struct svp_usb_cancel_urb *cancel_urb;

    switch (u->code) {
    case VRDE_USB_OPEN:
        CHECK_SIZE(sizeof(u->reqOpen), size, "vrde_usb_req_open");
        ids = (struct svp_usb_ids *)malloc(sizeof(*ids));
        if (!ids)
            return -1;
        ids->client_id = client_id;
        ids->dev_id = u->reqOpen.id;
        inq_push(inq, SC_USB_OPEN, ids, sizeof(*ids));
        break;
    case VRDE_USB_CLOSE:
        CHECK_SIZE(sizeof(u->reqClose), size, "vrde_usb_req_close");
        ids = (struct svp_usb_ids *)malloc(sizeof(*ids));
        if (!ids)
            return -1;
        ids->client_id = client_id;
        ids->dev_id = u->reqClose.id;
        inq_push(inq, SC_USB_CLOSE, ids, sizeof(*ids));
        break;
    case VRDE_USB_RESET:
        CHECK_SIZE(sizeof(u->reqReset), size, "vrde_usb_req_reset");
        ids = (struct svp_usb_ids *)malloc(sizeof(*ids));
        if (!ids)
            return -1;
        ids->client_id = client_id;
        ids->dev_id = u->reqReset.id;
        inq_push(inq, SC_USB_RESET, ids, sizeof(*ids));
        break;
    case VRDE_USB_SET_CONFIG:
        CHECK_SIZE(sizeof(u->reqSetConfig), size, "vrde_usb_req_set_config");
        set_config = (struct svp_usb_set_config *)malloc(sizeof(*set_config));
        if (!set_config)
            return -1;
        set_config->client_id = client_id;
        set_config->dev_id = u->reqSetConfig.id;
        set_config->config = u->reqSetConfig.configuration;
        inq_push(inq, SC_USB_SET_CONFIG, set_config, sizeof(*set_config));
        break;
    case VRDE_USB_CLAIM_INTERFACE:
        CHECK_SIZE(sizeof(u->reqClaimInterface), size, "vrde_usb_req_claim_interface");
        claim_iface = (struct svp_usb_claim_interface *)malloc(sizeof(*claim_iface));
        if (!claim_iface)
            return -1;
        claim_iface->client_id = client_id;
        claim_iface->dev_id = u->reqClaimInterface.id;
        claim_iface->iface = u->reqClaimInterface.iface;
        inq_push(inq, SC_USB_CLAIM_INTERFACE, claim_iface, sizeof(*claim_iface));
        break;
    case VRDE_USB_RELEASE_INTERFACE:
        CHECK_SIZE(sizeof(u->reqReleaseInterface), size, "vrde_usb_req_release_interface");
        relse_iface = (struct svp_usb_release_interface *)malloc(sizeof(*relse_iface));
        if (!relse_iface)
            return -1;
        relse_iface->client_id = client_id;
        relse_iface->dev_id = u->reqReleaseInterface.id;
        relse_iface->iface = u->reqReleaseInterface.iface;
        inq_push(inq, SC_USB_RELEASE_INTERFACE, relse_iface, sizeof(*relse_iface));
        break;
    case VRDE_USB_INTERFACE_SETTING:
        CHECK_SIZE(sizeof(u->reqInterfaceSetting), size, "vrde_usb_req_interface_setting");
        set_iface = (struct svp_usb_set_interface *)malloc(sizeof(*set_iface));
        if (!set_iface)
            return -1;
        set_iface->client_id = client_id;
        set_iface->dev_id = u->reqInterfaceSetting.id;
        set_iface->iface = u->reqInterfaceSetting.iface;
        set_iface->setting = u->reqInterfaceSetting.setting;
        inq_push(inq, SC_USB_SET_INTERFACE, set_iface, sizeof(*set_iface));
        break;
    case VRDE_USB_QUEUE_URB:
        CHECK_SIZE(sizeof(u->reqQueueURB), size, "vrde_usb_req_queue_urb");
        queue_urb = (struct svp_usb_queue_urb *)malloc(sizeof(*queue_urb));
        if (!queue_urb)
            return -1;
        queue_urb->client_id = client_id;
        queue_urb->dev_id = u->reqQueueURB.id;
        queue_urb->handle = u->reqQueueURB.handle;
        queue_urb->type = u->reqQueueURB.type;
        queue_urb->ep = u->reqQueueURB.ep;
        queue_urb->direction = u->reqQueueURB.direction;
        queue_urb->urblen = u->reqQueueURB.urblen;
        queue_urb->datalen = u->reqQueueURB.datalen;
        if (queue_urb->datalen > 0) {
            queue_urb->data = malloc(queue_urb->datalen);
            if (queue_urb->data)
                memcpy(queue_urb->data, u->reqQueueURB.data, queue_urb->datalen);
        } else {
            queue_urb->data = 0;
        }
        inq_push(inq, SC_USB_QUEUE_URB, queue_urb, sizeof(*queue_urb));
        break;
    case VRDE_USB_REAP_URB:
        CHECK_SIZE(sizeof(u->reqReapURB), size, "vrde_usb_req_reap_urb");
        ids = (struct svp_usb_ids *)malloc(sizeof(*ids));
        if (!ids)
            return -1;
        ids->client_id = client_id;
        ids->dev_id = -1;
        inq_push(inq, SC_USB_REAP_URB, ids, sizeof(*ids));
        break;
    case VRDE_USB_CLEAR_HALTED_EP:
        CHECK_SIZE(sizeof(u->reqClearHaltedEP), size, "vrde_usb_req_clear_halted_ep");
        clr_ep = (struct svp_usb_clear_halted_ep *)malloc(sizeof(*clr_ep));
        if (!clr_ep)
            return -1;
        clr_ep->client_id = client_id;
        clr_ep->dev_id = u->reqClearHaltedEP.id;
        clr_ep->ep = u->reqClearHaltedEP.ep;
        inq_push(inq, SC_USB_CLEAR_HALTED_EP, clr_ep, sizeof(*clr_ep));
        break;
    case VRDE_USB_CANCEL_URB:
        CHECK_SIZE(sizeof(u->reqCancelURB), size, "vrde_usb_req_cancel_urb");
        cancel_urb = (struct svp_usb_cancel_urb *)malloc(sizeof(*cancel_urb));
        if (!cancel_urb)
            return -1;
        cancel_urb->client_id = client_id;
        cancel_urb->dev_id = u->reqCancelURB.id;
        cancel_urb->handle = u->reqCancelURB.handle;
        inq_push(inq, SC_USB_CANCEL_URB, cancel_urb, sizeof(*cancel_urb));
        break;
    case VRDE_USB_DEVICE_LIST:
        CHECK_SIZE(sizeof(u->reqDeviceList), size, "vrde_usb_device_list request");
        ids = (struct svp_usb_ids *)malloc(sizeof(*ids));
        if (!ids)
            return -1;
        ids->client_id = client_id;
        ids->dev_id = -1;
        inq_push(inq, SC_USB_DEVICE_LIST_QUERY, ids, sizeof(*ids));
        break;
    case VRDE_USB_NEGOTIATE:
        ids = (struct svp_usb_ids *)malloc(sizeof(*ids));
        if (!ids)
            return -1;
        ids->client_id = client_id;
        ids->dev_id = -1;
        inq_push(inq, SC_USB_NEGOTIATE, ids, sizeof(*ids));
        break;
    default:
        dzlog_error("not supported usb code %d", u->code);
        return -1;
    }
}

int vrde_translate_usb_device_list(struct pac_usb_device_list *in_list, int in_size,
                                   void **out_data, uint32_t *out_size)
{
    int i;
    struct vrde_usb_device_desc *v_dev = (struct vrde_usb_device_desc *)g_vbuf;
    struct pac_usb_device *p_dev = in_list->device;

    CHECK_SIZE(sizeof(*in_list) + in_list->count * sizeof(struct pac_usb_device), in_size, "usb_device_list reply");
    for (i = 0; i < in_list->count; i++) {
        v_dev->oNext = sizeof(*v_dev) + sizeof(p_dev->vender_name)
                + sizeof(p_dev->product_name) + sizeof(p_dev->serial_number);
        v_dev->id = p_dev->id;
        v_dev->bcdUSB = p_dev->usb_version;
        v_dev->bDeviceClass = p_dev->klass;
        v_dev->bDeviceSubClass = p_dev->sub_class;
        v_dev->bDeviceProtocol = p_dev->protocol;
        v_dev->idVendor = p_dev->vid;
        v_dev->idProduct = p_dev->pid;
        v_dev->bcdRev = p_dev->revision;
        v_dev->oManufacturer = sizeof(*v_dev);
        v_dev->oProduct = v_dev->oManufacturer + sizeof(p_dev->vender_name);
        v_dev->oSerialNumber = v_dev->oProduct + sizeof(p_dev->product_name);
        v_dev->idPort = p_dev->port;
        v_dev->u16DeviceSpeed = p_dev->speed;
        memcpy((char *)v_dev + v_dev->oManufacturer, p_dev->vender_name, sizeof(p_dev->vender_name));
        memcpy((char *)v_dev + v_dev->oProduct, p_dev->product_name, sizeof(p_dev->product_name));
        memcpy((char *)v_dev + v_dev->oSerialNumber, p_dev->serial_number, sizeof(p_dev->serial_number));
        v_dev = (struct vrde_usb_device_desc *)((uint8_t *)v_dev + v_dev->oNext);
    }
    *out_data = g_vbuf;
    *out_size = (uint8_t *)v_dev - g_vbuf;
//    dzlog_debug("final size %d", *out_size);
    return 0;
}

int vrde_translate_usb_urb_data(struct pac_usb_urb *in_data, int in_size,
                                void **out_data, uint32_t *out_size)
{
    struct pac_usb_urb *p_urb = in_data;
    int size = in_size;
    struct vrde_usb_urb *v_urb = (struct vrde_usb_urb *)g_vbuf;

    if (size < sizeof(*p_urb)) {
        dzlog_error("bad urb data packet size %d", size);
        return -1;
    }

    while (size >= sizeof(*p_urb)) {
        v_urb->id = p_urb->id;
        v_urb->flags = p_urb->flags;
        v_urb->error = p_urb->error;
        v_urb->handle = p_urb->handle;
        v_urb->len = p_urb->len;
//        dzlog_debug("urb status %d len %d", p_urb->error, p_urb->len);
        if (p_urb->nodata) {
            v_urb = (struct vrde_usb_urb *)((uint8_t *)v_urb + sizeof(*v_urb));
            size -= sizeof(*p_urb);
            p_urb = (struct pac_usb_urb *)((uint8_t *)p_urb + sizeof(*p_urb));
        } else {
            memcpy((uint8_t *)v_urb + sizeof(*v_urb), (uint8_t *)p_urb + sizeof(*p_urb), p_urb->len);
            v_urb = (struct vrde_usb_urb *)((uint8_t *)v_urb + sizeof(*v_urb) + v_urb->len);
            size -= sizeof(*p_urb) + p_urb->len;
            p_urb = (struct pac_usb_urb *)((uint8_t *)p_urb + sizeof(*p_urb) + p_urb->len);
        }
    }
    *out_data = g_vbuf;
    *out_size = (uint8_t *)v_urb - g_vbuf;
//    dzlog_debug("final size %d", *out_size);
    return 0;
}

int vrde_translate_usb_status(struct pac_usb_status *p_status, int in_size,
                              void **out_data, uint32_t *out_size)
{
    struct vrde_usb_status *v_status = (struct vrde_usb_status *)g_vbuf;

    v_status->status = p_status->status;
    v_status->id = p_status->dev_id;
    *out_data = g_vbuf;
    *out_size = sizeof(*v_status);
//    dzlog_debug("final size %d", *out_size);
    return 0;
}

int vrde_translate_usb_input(int in_req, const void *in_data, int in_size,
                             uint8_t *out_req, void **out_data, uint32_t *out_size)
{
    switch (in_req) {
    case SC_USB_NEGOTIATE:
        *out_req = VRDE_USB_NEGOTIATE;
        *out_data = &g_usb_negotiate_reply;
        *out_size = sizeof(g_usb_negotiate_reply);
        break;
    case SC_USB_DEVICE_LIST_RESULT:
        *out_req = VRDE_USB_DEVICE_LIST;
        return vrde_translate_usb_device_list((struct pac_usb_device_list *)in_data, in_size,
                                              out_data, out_size);
    case SC_USB_URB_DATA:
        *out_req = VRDE_USB_REAP_URB;
        return vrde_translate_usb_urb_data((struct pac_usb_urb *)in_data, in_size,
                                           out_data, out_size);
    case SC_USB_OPEN_STATUS:
        *out_req = VRDE_USB_OPEN;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    case SC_USB_RESET_STATUS:
        *out_req = VRDE_USB_RESET;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    case SC_USB_SET_CONFIG_STATUS:
        *out_req = VRDE_USB_SET_CONFIG;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    case SC_USB_CLAIM_INTERFACE_STATUS:
        *out_req = VRDE_USB_CLAIM_INTERFACE;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    case SC_USB_RELEASE_INTERFACE_STATUS:
        *out_req = VRDE_USB_RELEASE_INTERFACE;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    case SC_USB_SET_INTERFACE_STATUS:
        *out_req = VRDE_USB_INTERFACE_SETTING;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    case SC_USB_CLEAR_HALTED_EP_STATUS:
        *out_req = VRDE_USB_CLEAR_HALTED_EP;
        return vrde_translate_usb_status((struct pac_usb_status *)in_data, in_size, out_data, out_size);
    default:
        dzlog_error("failed to translate usb input with code %d", in_req);
        return -1;
    }
    return 0;
}
