#include "graphic.h"
#include "inqueue.h"
#include "outqueue.h"
#include "codec.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>

#define ARRAY_ELEMS(x) (sizeof(x) / sizeof((x)[0]))

static int graphic_process(struct graphic_context *gctx, struct in_cmd *in);

void graphic_reconfig(struct graphic_context *);

static int calc_bitmap_size(struct graphic_context *gctx, struct svp_bitmap *s_bitmap);
static int calc_glyph_size(struct graphic_context *gctx, struct svp_glyph *s_glyph);
static int calc_text_size(struct graphic_context *gctx, struct svp_text *s_text);

void graphic_option_reload(struct graphic_option *opt)
{
    settings_read_int(GS, KEY_PREFER_CODEC, &opt->force_codec, SX_RAW);
    settings_read_int(GS, KEY_LZ4_HC, &opt->lz4_hc, 0);
}

struct graphic_context *graphic_context_create()
{
    struct graphic_context *gctx = (struct graphic_context *)malloc(sizeof(*gctx));
    if (!gctx)
        return 0;
    gctx->reconfig = graphic_reconfig;
    gctx->outq = outq_create();
    if (!gctx->outq) {
        free(gctx);
        return 0;
    }
    graphic_option_reload(&gctx->opt);
    gctx->cdc = codec_create();
    if (!gctx->cdc) {
        free(gctx);
        return 0;
    }
    gctx->cdc->init(gctx->cdc);
    gctx->cdc->configure(gctx->cdc, &gctx->opt);
    return gctx;
}

void graphic_context_destroy(struct graphic_context *gctx)
{
    gctx->cdc->fini(gctx->cdc);
    codec_destroy(gctx->cdc);
    outq_destroy(gctx->outq);
    free(gctx);
}

int graphic_get_option(struct graphic_context *gctx, struct graphic_option *opt)
{
    if (!gctx || !opt)
        return -1;
    memcpy(opt, &gctx->opt, sizeof(*opt));
    return 0;
}

int graphic_set_option(struct graphic_context *gctx, struct graphic_option *opt)
{
    if (!gctx || !opt || !gctx->cdc)
        return -1;
    if (opt->force_codec != SX_AUTO && !gctx->cdc->encoder_for_id(gctx->cdc, opt->force_codec))
        return -1;
    memcpy(&gctx->opt, opt, sizeof(*opt));
    gctx->cdc->configure(gctx->cdc, &gctx->opt);
    return 0;
}

int graphic_input(struct graphic_context *gctx, struct inqueue *inq)
{
    struct in_cmd i_cmd;
    int n;

    if (!gctx || !inq)
        return -1;
    n = SVP_CMDQUEUE_SIZE - outq_size(gctx->outq);
    if (n > inq_size(inq))
        n = inq_size(inq);
    while (n > 0) {
        if (inq_pop(inq, &i_cmd) != 1) {
            dzlog_debug("pop inqueue error");
            return -1;
        }
        if (graphic_process(gctx, &i_cmd) != 0) {
            dzlog_debug("process error");
            return -1;
        }
        in_cmd_cleanup(&i_cmd);
        n--;
    }
    return 0;
}

int graphic_output(struct graphic_context *gctx, struct outqueue *outq)
{
    struct out_cmd cmd;
    if (!gctx || !outq)
        return -1;
    while (outq_pop(gctx->outq, &cmd) == 1)
        outq_push(outq, cmd.type, cmd.data, cmd.size);
    return 0;
out_error:
    return -1;
}

static int graphic_process_bitmap(struct graphic_context *gctx, struct svp_bitmap *s_bitmap,
                                  struct pac_bitmap *o_bitmap)
{
    uint8_t *o_bits = (uint8_t *)o_bitmap + sizeof(*o_bitmap);

    if (!gctx || !s_bitmap || !o_bitmap)
        return -1;

    o_bitmap->size = s_bitmap->size;
    o_bitmap->format = s_bitmap->format;
    o_bitmap->codec = SX_RAW;
    o_bitmap->x = s_bitmap->x;
    o_bitmap->y = s_bitmap->y;
    o_bitmap->w = s_bitmap->w;
    o_bitmap->h = s_bitmap->h;
    memcpy(o_bits, s_bitmap->bits, s_bitmap->size);

    gctx->cdc->encode(gctx->cdc, o_bitmap);

    return 0;
}

static int graphic_process_dirty_rect(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size;
    uint8_t *data = 0;
    struct svp_bitmap *s_bitmap;
    struct pac_hdr *hdr;
    struct pac_bitmap *o_bitmap;

    if (!in || !in->data || !out)
        goto out_error;
    if (in->size != sizeof(*s_bitmap))
        goto out_error;

    s_bitmap = (struct svp_bitmap *)in->data;
    size = sizeof(struct pac_hdr) + sizeof(struct pac_bitmap) + s_bitmap->size;
    data = (uint8_t *)malloc(size);
    if (!data)
        goto out_error;
    hdr = (struct pac_hdr *)data;
    o_bitmap = (struct pac_bitmap *)(data + sizeof(*hdr));

    if (graphic_process_bitmap(gctx, s_bitmap, o_bitmap) != 0)
        goto out_error;
    size -= s_bitmap->size;
    size += o_bitmap->size;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_DIRTY_RECT;
    hdr->c_size = size - sizeof(*hdr);

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_solid_rect(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_solid_rect);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_solid_rect *s_rect = (struct svp_solid_rect *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_solid_rect *o_rect = (struct pac_solid_rect *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_rect))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_SOLID_RECT;
    hdr->c_size = size - sizeof(*hdr);

    o_rect->x = s_rect->x;
    o_rect->y = s_rect->y;
    o_rect->w = s_rect->w;
    o_rect->h = s_rect->h;
    o_rect->rgb = s_rect->rgb;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_solid_blt(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_solid_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_solid_blt *s_blt = (struct svp_solid_blt *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_solid_blt *o_blt = (struct pac_solid_blt *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_blt))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_SOLID_BLT;
    hdr->c_size = size - sizeof(*hdr);

    o_blt->x = s_blt->x;
    o_blt->y = s_blt->y;
    o_blt->w = s_blt->w;
    o_blt->h = s_blt->h;
    o_blt->rgb = s_blt->rgb;
    o_blt->rop = s_blt->rop;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_dst_blt(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_dst_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_dst_blt *s_blt = (struct svp_dst_blt *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_dst_blt *o_blt = (struct pac_dst_blt *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_blt))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_DST_BLT;
    hdr->c_size = size - sizeof(*hdr);

    o_blt->x = s_blt->x;
    o_blt->y = s_blt->y;
    o_blt->w = s_blt->w;
    o_blt->h = s_blt->h;
    o_blt->rop = s_blt->rop;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_screen_blt(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_screen_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_screen_blt *s_blt = (struct svp_screen_blt *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_screen_blt *o_blt = (struct pac_screen_blt *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_blt))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_SCREEN_BLT;
    hdr->c_size = size - sizeof(*hdr);

    o_blt->x = s_blt->x;
    o_blt->y = s_blt->y;
    o_blt->w = s_blt->w;
    o_blt->h = s_blt->h;
    o_blt->x_src = s_blt->x_src;
    o_blt->y_src = s_blt->y_src;
    o_blt->rop = s_blt->rop;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_pattern_blt_brush(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_pattern_blt_brush);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_pat_blt_brush *s_blt = (struct svp_pat_blt_brush *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_pattern_blt_brush *o_blt = (struct pac_pattern_blt_brush *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_blt))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_PATTERN_BLT_BRUSH;
    hdr->c_size = size - sizeof(*hdr);

    o_blt->x = s_blt->x;
    o_blt->y = s_blt->y;
    o_blt->w = s_blt->w;
    o_blt->h = s_blt->h;
    o_blt->x_src = s_blt->x_src;
    o_blt->y_src = s_blt->y_src;
    o_blt->rgb_fg = s_blt->rgb_fg;
    o_blt->rgb_bg = s_blt->rgb_bg;
    o_blt->rop = s_blt->rop;
    memcpy(o_blt->pattern, s_blt->pattern, sizeof(o_blt->pattern));

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_mem_blt(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_mem_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_mem_blt *s_blt = (struct svp_mem_blt *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_mem_blt *o_blt = (struct pac_mem_blt *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_blt))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_MEM_BLT;
    hdr->c_size = size - sizeof(*hdr);

    o_blt->x = s_blt->x;
    o_blt->y = s_blt->y;
    o_blt->w = s_blt->w;
    o_blt->h = s_blt->h;
    o_blt->x_src = s_blt->x_src;
    o_blt->y_src = s_blt->y_src;
    o_blt->rop = s_blt->rop;
    memcpy(o_blt->hash, s_blt->hash, sizeof(s_blt->hash));

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_cached_bitmap(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size;
    uint8_t *data = 0;
    struct svp_cached_bitmap *s_cache;
    struct pac_hdr *hdr;
    struct pac_cached_bitmap *o_cache;
    struct pac_bitmap *o_bitmap;

    if (!in || !in->data || !out)
        goto out_error;
    if (in->size != sizeof(*s_cache))
        goto out_error;

    s_cache = (struct svp_cached_bitmap *)in->data;
    size = sizeof(struct pac_hdr) + sizeof(struct pac_cached_bitmap)
            + sizeof(struct pac_bitmap) + s_cache->bitmap.size;
    data = (uint8_t *)malloc(size);
    if (!data)
        goto out_error;
    hdr = (struct pac_hdr *)data;
    o_cache = (struct pac_cached_bitmap *)(data + sizeof(*hdr));
    o_bitmap = (struct pac_bitmap *)(data + sizeof(*hdr) + sizeof(*o_cache));

    memcpy(o_cache->hash, s_cache->hash, sizeof(s_cache->hash));
    if (graphic_process_bitmap(gctx, &s_cache->bitmap, o_bitmap) != 0)
        goto out_error;
    size -= s_cache->bitmap.size;
    size += o_bitmap->size;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_CACHED_BITMAP;
    hdr->c_size = size - sizeof(*hdr);

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_deleted_bitmap(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_deleted_bitmap);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_deleted_bitmap *s_cache = (struct svp_deleted_bitmap *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_deleted_bitmap *o_cache = (struct pac_deleted_bitmap *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_cache))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_DELETED_BITMAP;
    hdr->c_size = size - sizeof(*hdr);

    memcpy(o_cache->hash, s_cache->hash, sizeof(o_cache->hash));

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_line(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_line);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_line *s_line = (struct svp_line *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_line *o_line = (struct pac_line *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_line))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_LINE;
    hdr->c_size = size - sizeof(*hdr);

    o_line->x1 = s_line->x1;
    o_line->y1 = s_line->y1;
    o_line->x2 = s_line->x2;
    o_line->y2 = s_line->y2;

    o_line->x_bounds1 = s_line->x_bounds1;
    o_line->y_bounds1 = s_line->y_bounds1;
    o_line->x_bounds2 = s_line->x_bounds2;
    o_line->y_bounds2 = s_line->y_bounds2;

    o_line->mix = s_line->mix;
    o_line->rgb = s_line->rgb;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_polyline(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_polyline);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_polyline *s_polyline = (struct svp_polyline *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_polyline *o_polyline = (struct pac_polyline *)(data + sizeof(*hdr));
    int i;

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_polyline))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_POLYLINE;
    hdr->c_size = size - sizeof(*hdr);

    o_polyline->pt_start.x = s_polyline->x_start;
    o_polyline->pt_start.y = s_polyline->y_start;
    o_polyline->mix = s_polyline->mix;
    o_polyline->rgb = s_polyline->rgb;
    o_polyline->points.c = s_polyline->count;
    if (s_polyline->count > 16)
        return -1;
    for (i = 0; i < s_polyline->count; i++) {
        o_polyline->points.a[i].x = s_polyline->points[i].x;
        o_polyline->points.a[i].y = s_polyline->points[i].y;
    }

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_ellipse(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_ellipse);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_ellipse *s_el = (struct svp_ellipse *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_ellipse *o_el = (struct pac_ellipse *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_el))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_ELLIPSE;
    hdr->c_size = size - sizeof(*hdr);

    o_el->pt1.x = s_el->x;
    o_el->pt1.y = s_el->y;
    o_el->pt2.x = s_el->x + s_el->w;
    o_el->pt2.y = s_el->y + s_el->h;
    o_el->mix = s_el->mix;
    o_el->fill_mode = s_el->fill_mode;
    o_el->rgb = s_el->rgb;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_save_screen(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_save_screen);
    uint8_t *data = (uint8_t *)malloc(size);
    struct svp_save_screen *s_save = (struct svp_save_screen *)in->data;
    struct pac_hdr *hdr = (struct pac_hdr *)data;
    struct pac_save_screen *o_save = (struct pac_save_screen *)(data + sizeof(*hdr));

    if (!in || !in->data || !data || !out)
        goto out_error;
    if (in->size != sizeof(*s_save))
        goto out_error;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_SAVE_SCREEN;
    hdr->c_size = size - sizeof(*hdr);

    o_save->pt1.x = s_save->x;
    o_save->pt1.y = s_save->y;
    o_save->pt2.x = s_save->x + s_save->w;
    o_save->pt2.y = s_save->y + s_save->h;
    o_save->ident = s_save->ident;
    o_save->restore = 0;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_restore_screen(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size;
    uint8_t *data = 0;
    struct svp_restore_screen *s_restore;
    struct pac_hdr *hdr;
    struct pac_save_screen *o_restore;
    struct pac_bitmap *o_bitmap;

    if (!gctx || !in || !in->data || !out)
        return -1;
    if (in->size != sizeof(*s_restore))
        goto out_error;

    s_restore = (struct svp_restore_screen *)in->data;
    size = sizeof(struct pac_hdr) + sizeof(struct pac_save_screen)
            + sizeof(struct pac_bitmap) + s_restore->bitmap.size;
    data = (uint8_t *)malloc(size);
    if (!data)
        return -1;
    hdr = (struct pac_hdr *)data;
    o_restore = (struct pac_save_screen *)(data + sizeof(*hdr));
    o_bitmap = (struct pac_bitmap *)((uint8_t *)o_restore + sizeof(*o_restore));

    o_restore->pt1.x = s_restore->x;
    o_restore->pt1.y = s_restore->y;
    o_restore->pt2.x = s_restore->x + s_restore->w;
    o_restore->pt2.y = s_restore->y + s_restore->h;
    o_restore->ident = s_restore->ident;
    o_restore->restore = 1;
    if (graphic_process_bitmap(gctx, &s_restore->bitmap, o_bitmap) != 0)
        goto out_error;

    size -= s_restore->bitmap.size;
    size += o_bitmap->size;

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_RESTORE_SCREEN;
    hdr->c_size = size - sizeof(*hdr);

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = size;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_text(struct graphic_context *gctx, struct in_cmd *in, struct out_cmd *out)
{
    int size;
    uint8_t *data = 0;
    struct svp_text *s_text;
    struct svp_glyph *s_glyph;
    struct pac_hdr *hdr;
    struct pac_text *o_text;
    struct pac_glyph *o_glyph;
    struct pac_bitmap *o_bitmap;
    uint8_t *ptr;

    if (!gctx || !in || !in->data || !out)
        return -1;
    if (in->size != sizeof(*s_text))
        goto out_error;

    s_text = (struct svp_text *)in->data;
    size = sizeof(struct pac_hdr) + calc_text_size(gctx, s_text);
    data = (uint8_t *)malloc(size);
    if (!data)
        return -1;
    hdr = (struct pac_hdr *)data;
    o_text = (struct pac_text *)(data + sizeof(*hdr));

    o_text->size = size - sizeof(struct pac_hdr);
    o_text->x_bg = s_text->x_bg;
    o_text->y_bg = s_text->y_bg;
    o_text->w_bg = s_text->w_bg;
    o_text->h_bg = s_text->h_bg;
    o_text->x_opaque = s_text->x_opaque;
    o_text->y_opaque = s_text->y_opaque;
    o_text->w_opaque = s_text->w_opaque;
    o_text->h_opaque = s_text->h_opaque;
    o_text->max_glyph = s_text->max_glyph;
    o_text->glyphs = s_text->glyph_count;
    o_text->flags = s_text->flags;
    o_text->char_inc = s_text->char_inc;
    o_text->rgb_fg = s_text->rgb_fg;
    o_text->rgb_bg = s_text->rgb_bg;

    ptr = (uint8_t *)o_text + sizeof(*o_text);
    list_for_each_entry(s_glyph, &s_text->glyph_list, link) {
        o_glyph = (struct pac_glyph *)ptr;
        o_glyph->size = calc_glyph_size(gctx, s_glyph);
        o_glyph->handle = s_glyph->handle;
        o_glyph->x = s_glyph->bitmap.x;
        o_glyph->y = s_glyph->bitmap.y;
        o_glyph->w = s_glyph->bitmap.w;
        o_glyph->h = s_glyph->bitmap.h;
        o_glyph->x_origin = s_glyph->x_origin;
        o_glyph->y_origin = s_glyph->y_origin;
        memcpy((uint8_t *)o_glyph + sizeof(*o_glyph), s_glyph->bitmap.bits, s_glyph->bitmap.size);
        ptr += o_glyph->size;
    }

    o_text->size = (ptr - (uint8_t *)o_text) - sizeof(*o_text);

    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_TEXT;
    hdr->c_size = ptr - (uint8_t *)o_text;

    out->type = hdr->c_cmd;
    out->data = data;
    out->size = ptr - data;
    return 0;
out_error:
    free(data);
    return -1;
}

static int graphic_process_clip_draw(struct graphic_context *gctx, struct in_cmd *in)
{
    struct pac_hdr *o_hdr;
    struct pac_clip_draw *o_clip;
    struct svp_clip_draw *s_clip;
    struct in_cmd sub_in;
    struct out_cmd out;
    struct out_cmd sub_out;
    int i;
    int rc;

    if (!gctx || !in)
        return -1;
    if (!in->data || in->size != sizeof(*s_clip))
        return -1;
    s_clip = (struct svp_clip_draw *)in->data;

    out.type = SC_CLIP_DRAW;
    out.size = sizeof(*o_hdr) + sizeof(*o_clip) + s_clip->rect_count * sizeof(struct pac_rect);
    out.data = (uint8_t *)malloc(out.size);
    if (!out.data)
        return -1;

    o_hdr = (struct pac_hdr *)out.data;
    o_hdr->c_magic = SVP_MAGIC;
    o_hdr->c_cmd = out.type;
    o_hdr->c_size = out.size - sizeof(*o_hdr);

    o_clip = (struct pac_clip_draw *)(out.data + sizeof(*o_hdr));
    if (s_clip->rect_count > ARRAY_ELEMS(s_clip->rect))
        goto out_error;
    o_clip->rect_count = s_clip->rect_count;
    for (i = 0; i < s_clip->rect_count; i++) {
        o_clip->rect[i].x = s_clip->rect[i].x;
        o_clip->rect[i].y = s_clip->rect[i].y;
        o_clip->rect[i].w = s_clip->rect[i].w;
        o_clip->rect[i].h = s_clip->rect[i].h;
    }
    if (s_clip->cmd_count > ARRAY_ELEMS(s_clip->cmd))
        goto out_error;
    o_clip->cmd_count = s_clip->cmd_count;
    if (outq_push(gctx->outq, out.type, out.data, out.size) != 0) {
        dzlog_debug("push outqueue error");
        goto out_error;
    }

    for (i = 0; i < s_clip->cmd_count; i++) {
        sub_in.type = s_clip->cmd[i].type;
        sub_in.data = s_clip->cmd[i].data;
        sub_in.size = s_clip->cmd[i].size;
        rc = graphic_process(gctx, &sub_in);
        if (rc) {
            dzlog_debug("process sub command idx %d error", i);
            goto out_error;
        }
    }

    return 0;
out_error:
    free(out.data);
    return -1;
}

static int graphic_process(struct graphic_context *gctx, struct in_cmd *in)
{
    int err = 0;
    int no_push = 0;
    struct out_cmd out;
    switch (in->type) {
    case SC_DIRTY_RECT:
        err = graphic_process_dirty_rect(gctx, in, &out);
        break;
    case SC_SOLID_RECT:
        err = graphic_process_solid_rect(gctx, in, &out);
        break;
    case SC_SOLID_BLT:
        err = graphic_process_solid_blt(gctx, in, &out);
        break;
    case SC_DST_BLT:
        err = graphic_process_dst_blt(gctx, in, &out);
        break;
    case SC_SCREEN_BLT:
        err = graphic_process_screen_blt(gctx, in, &out);
        break;
    case SC_PATTERN_BLT_BRUSH:
        err = graphic_process_pattern_blt_brush(gctx, in, &out);
        break;
    case SC_MEM_BLT:
        err = graphic_process_mem_blt(gctx, in, &out);
        break;
    case SC_CACHED_BITMAP:
        err = graphic_process_cached_bitmap(gctx, in, &out);
        break;
    case SC_DELETED_BITMAP:
        err = graphic_process_deleted_bitmap(gctx, in, &out);
        break;
    case SC_LINE:
        err = graphic_process_line(gctx, in, &out);
        break;
    case SC_POLYLINE:
        err = graphic_process_polyline(gctx, in, &out);
        break;
    case SC_ELLIPSE:
        err = graphic_process_ellipse(gctx, in, &out);
        break;
    case SC_SAVE_SCREEN:
        err = graphic_process_save_screen(gctx, in, &out);
        break;
    case SC_RESTORE_SCREEN:
        err = graphic_process_restore_screen(gctx, in, &out);
        break;
    case SC_TEXT:
        err = graphic_process_text(gctx, in, &out);
        break;
    case SC_CLIP_DRAW:
        err = graphic_process_clip_draw(gctx, in);
        no_push = 1;
        break;
    default:
        dzlog_debug("not supported cmd %d", in->type);
        err = -1;
    }
    if (err == 0 && !no_push) {
        if (outq_push(gctx->outq, out.type, out.data, out.size) != 0) {
            dzlog_debug("push outqueue error");
            out_cmd_cleanup(&out);
            return -1;
        }
    }
    return err;
}

/****************************************************************************************/

static int calc_bitmap_size(struct graphic_context *gctx, struct svp_bitmap *s_bitmap)
{
    if (!gctx)
        return -1;
    switch (s_bitmap->format) {
    case SPF_RGB32:
        return sizeof(struct pac_bitmap) + s_bitmap->w * s_bitmap->h * 4;
    case SPF_RGB24:
        return sizeof(struct pac_bitmap) + s_bitmap->w * s_bitmap->h * 3;
    case SPF_RGB16:
        return sizeof(struct pac_bitmap) + s_bitmap->w * s_bitmap->h * 2;
    case SPF_RGB8:
        return sizeof(struct pac_bitmap) + s_bitmap->w * s_bitmap->h;
    case SPF_MONO:
        return sizeof(struct pac_bitmap) + padded_length(s_bitmap->w, s_bitmap->h);
    default:
        return -1;
    }
}

static int calc_glyph_size(struct graphic_context *gctx, struct svp_glyph *s_glyph)
{
    return sizeof(struct pac_glyph) + s_glyph->bitmap.size;
}

static int calc_text_size(struct graphic_context *gctx, struct svp_text *s_text)
{
    int size;
    struct svp_glyph *s_glyph;
    if (!gctx)
        return -1;
    size = sizeof(struct pac_text);
    list_for_each_entry(s_glyph, &s_text->glyph_list, link)
        size += calc_glyph_size(gctx, s_glyph);
    return size;
}

void graphic_reconfig(struct graphic_context *gctx)
{
    if (!gctx || !gctx->cdc)
        return;
    graphic_option_reload(&gctx->opt);
    gctx->cdc->configure(gctx->cdc, &gctx->opt);
}
