#include <gtest/gtest.h>
extern "C" {
#include "server/vrde.c"
#include "server/server.c"
#include "server/graphic.c"
#include "server/inqueue.c"
#include "server/outqueue.c"
#include "server/svp.c"
#include "server/svp_server.c"
#include "server/channel.c"
#include "server/main_channel.c"
#include "server/display_channel.c"
#include "server/display_worker.c"
#include "server/cursor_channel.c"
#include "server/input_channel.c"
#include "server/usb_channel.c"
#include "server/usb.c"
#include "server/cmd.c"
#include "server/codec.c"
#include "server/encoder.c"
}

/*
 * set option
 * error: bad option value
 * dummy
 * SC_DIRTY_RECT RGB32 Raw
 * SC_DIRTY_RECT RGB24 Raw
 * SC_DIRTY_RECT RGB16 Raw
 * SC_DIRTY_RECT RGB8 Raw
 * SC_SOLID_RECT
 * SC_SOLID_BLT
 * SC_DST_BLT
 * SC_SCREEN_BLT
 * SC_PAT_BLT_BRUSH
 * SC_MEM_BLT
 * SC_CACHED_BITMAP
 * SC_DELETED_BITMAP
 * SC_LINE
 * SC_POLYLINE
 * SC_ELLIPSE
 * SC_SAVE_SCREEN
 * SC_TEXT, empty
 * SC_TEXT, one character
 * SC_TEXT, two character
 * SC_CLIP_DRAW, one rect one cmd
 * multiple input
 * error: malformed input
 */

bool operator==(const pac_hdr &left, const pac_hdr &right)
{
    return left.c_magic == right.c_magic
            && left.c_cmd == right.c_cmd
            && left.c_size == right.c_size;
}

bool operator==(const svp_bitmap &s_bitmap, const pac_bitmap &o_bitmap)
{
    bool eq = (s_bitmap.x == o_bitmap.x
               && s_bitmap.y == o_bitmap.y
               && s_bitmap.w == o_bitmap.w
               && s_bitmap.h == o_bitmap.h
               && s_bitmap.size == o_bitmap.size
               && (int)s_bitmap.format == o_bitmap.format
               && o_bitmap.codec == SX_RAW);
    if (!eq)
        return false;
    uint8_t *o_bits = (uint8_t *)&o_bitmap + sizeof(o_bitmap);
    return memcmp(s_bitmap.bits, o_bits, o_bitmap.size) == 0;
}

bool operator==(const svp_solid_rect &s_rect, const pac_solid_rect &o_rect)
{
    return s_rect.x == o_rect.x
            && s_rect.y == o_rect.y
            && s_rect.w == o_rect.w
            && s_rect.h == o_rect.h
            && s_rect.rgb == o_rect.rgb;
}

bool operator==(const svp_solid_blt &s_blt, const pac_solid_blt &o_blt)
{
    return s_blt.x == o_blt.x
            && s_blt.y == o_blt.y
            && s_blt.w == o_blt.w
            && s_blt.h == o_blt.h
            && s_blt.rgb == o_blt.rgb
            && s_blt.rop == o_blt.rop;
}

bool operator==(const svp_dst_blt &s_blt, const pac_dst_blt &o_blt)
{
    return s_blt.x == o_blt.x
            && s_blt.y == o_blt.y
            && s_blt.w == o_blt.w
            && s_blt.h == o_blt.h
            && s_blt.rop == o_blt.rop;
}

bool operator==(const svp_screen_blt &s_blt, const pac_screen_blt &o_blt)
{
    return s_blt.x == o_blt.x
            && s_blt.y == o_blt.y
            && s_blt.w == o_blt.w
            && s_blt.h == o_blt.h
            && s_blt.x_src == o_blt.x_src
            && s_blt.y_src == o_blt.y_src
            && s_blt.rop == o_blt.rop;
}

bool operator==(const svp_pat_blt_brush &s_blt, const pac_pattern_blt_brush &o_blt)
{
    return s_blt.x == o_blt.x
            && s_blt.y == o_blt.y
            && s_blt.w == o_blt.w
            && s_blt.h == o_blt.h
            && s_blt.x_src == o_blt.x_src
            && s_blt.y_src == o_blt.y_src
            && s_blt.rgb_fg == o_blt.rgb_fg
            && s_blt.rgb_bg == o_blt.rgb_bg
            && s_blt.rop == o_blt.rop
            && sizeof(s_blt.pattern) == sizeof(o_blt.pattern)
            && memcmp(s_blt.pattern, o_blt.pattern, sizeof(o_blt.pattern)) == 0;
}

bool operator==(const svp_mem_blt &s_blt, const pac_mem_blt &o_blt)
{
    return s_blt.x == o_blt.x
            && s_blt.y == o_blt.y
            && s_blt.w == o_blt.w
            && s_blt.h == o_blt.h
            && s_blt.x_src == o_blt.x_src
            && s_blt.y_src == o_blt.y_src
            && s_blt.rop == o_blt.rop
            && sizeof(s_blt.hash) == sizeof(o_blt.hash)
            && memcmp(s_blt.hash, o_blt.hash, sizeof(o_blt.hash)) == 0;
}

bool operator==(const svp_cached_bitmap &s_cache, const pac_cached_bitmap &o_cache)
{
    bool eq = (sizeof(s_cache.hash) == sizeof(o_cache.hash)
               && memcmp(s_cache.hash, o_cache.hash, sizeof(o_cache.hash)) == 0);
    if (!eq)
        return false;
    pac_bitmap *o_bitmap = (pac_bitmap *)((uint8_t *)&o_cache + sizeof(o_cache));
    return s_cache.bitmap == *o_bitmap;
}

bool operator==(const svp_deleted_bitmap &s_cache, const pac_deleted_bitmap &o_cache)
{
    return sizeof(s_cache.hash) == sizeof(o_cache.hash);
}

bool operator==(const svp_line &s_line, const pac_line &o_line)
{
    return s_line.x1 == o_line.x1
            && s_line.y1 == o_line.y1
            && s_line.x2 == o_line.x2
            && s_line.y2 == o_line.y2
            && s_line.x_bounds1 == o_line.x_bounds1
            && s_line.y_bounds1 == o_line.y_bounds1
            && s_line.x_bounds2 == o_line.x_bounds2
            && s_line.y_bounds2 == o_line.y_bounds2
            && s_line.mix == o_line.mix
            && s_line.rgb == o_line.rgb;
}

bool operator==(const svp_polyline &s_polyline, const pac_polyline &o_polyline)
{
    int i;
    bool eq = (s_polyline.x_start == o_polyline.pt_start.x
               && s_polyline.y_start == o_polyline.pt_start.y
               && s_polyline.mix == o_polyline.mix
               && s_polyline.rgb == o_polyline.rgb
               && s_polyline.count == o_polyline.points.c);
    if (!eq)
        return false;
    if (s_polyline.count > 16)
        return false;
    for (i = 0; i < s_polyline.count; i++)
        if (s_polyline.points[i].x != o_polyline.points.a[i].x)
            return false;
    return true;
}

bool operator==(const svp_ellipse &s_el, const pac_ellipse &o_el)
{
    return s_el.x == o_el.pt1.x
            && s_el.y == o_el.pt1.y
            && s_el.w == o_el.pt2.x - o_el.pt1.x
            && s_el.h == o_el.pt2.y - o_el.pt1.y
            && s_el.mix == o_el.mix
            && s_el.fill_mode == o_el.fill_mode
            && s_el.rgb == o_el.rgb;
}

bool operator==(const svp_save_screen &s_save, const pac_save_screen &o_save)
{
    return s_save.x == o_save.pt1.x
            && s_save.y == o_save.pt1.y
            && s_save.w == o_save.pt2.x - o_save.pt1.x
            && s_save.h == o_save.pt2.y - o_save.pt1.y
            && s_save.ident == o_save.ident
            && o_save.restore == 0;
}

bool operator==(const svp_restore_screen &s_restore, const pac_save_screen &o_save)
{
    bool eq = (s_restore.x == o_save.pt1.x
               && s_restore.y == o_save.pt1.y
               && s_restore.w == o_save.pt2.x - o_save.pt1.x
               && s_restore.h == o_save.pt2.y - o_save.pt1.y
               && s_restore.ident == o_save.ident
               && o_save.restore == 1);
    if (!eq)
        return false;
    pac_bitmap *o_bitmap = (pac_bitmap *)((uint8_t *)&o_save + sizeof(o_save));
    return s_restore.bitmap == *o_bitmap;
}

bool operator==(const svp_glyph &s_glyph, const pac_glyph &o_glyph)
{
    uint8_t *o_bits = (uint8_t *)((uint8_t *)&o_glyph + sizeof(o_glyph));
    return s_glyph.handle == o_glyph.handle
            && s_glyph.x_origin == o_glyph.x_origin
            && s_glyph.y_origin == o_glyph.y_origin
            && s_glyph.bitmap.x == o_glyph.x
            && s_glyph.bitmap.y == o_glyph.y
            && s_glyph.bitmap.w == o_glyph.w
            && s_glyph.bitmap.h == o_glyph.h
            && s_glyph.bitmap.size == o_glyph.size - sizeof(o_glyph)
            && memcmp(s_glyph.bitmap.bits, o_bits, s_glyph.bitmap.size) == 0;
}

bool operator==(const svp_text &s_text, const pac_text &o_text)
{
    int i;
    struct svp_glyph *s_glyph;
    struct pac_glyph *o_glyph;
    uint8_t *ptr;
    bool eq = (s_text.x_bg == o_text.x_bg
               && s_text.y_bg == o_text.y_bg
               && s_text.w_bg == o_text.w_bg
               && s_text.h_bg == o_text.h_bg
               && s_text.x_opaque == o_text.x_opaque
               && s_text.y_opaque == o_text.y_opaque
               && s_text.w_opaque == o_text.w_opaque
               && s_text.h_opaque == o_text.h_opaque
               && s_text.max_glyph == o_text.max_glyph
               && s_text.flags == o_text.flags
               && s_text.rgb_fg == o_text.rgb_fg
               && s_text.rgb_bg == o_text.rgb_bg
               && s_text.glyph_count == o_text.glyphs);
    if (!eq)
        return false;
    i = 0;
    ptr = (uint8_t *)&o_text + sizeof(o_text);
    list_for_each_entry(s_glyph, &s_text.glyph_list, link) {
        o_glyph = (struct pac_glyph *)ptr;
        if (!(*s_glyph == *o_glyph))
            return false;
        i++;
        ptr += o_glyph->size;
    }
    return true;
}

bool operator==(const svp_rect &s_rect, const pac_rect &o_rect)
{
    return s_rect.x == o_rect.x
            && s_rect.y == o_rect.y
            && s_rect.w == o_rect.w
            && s_rect.h == o_rect.h;
}

bool operator==(const svp_clip_draw &s_clip, const pac_clip_draw &o_clip)
{
    if (!(s_clip.cmd_count == o_clip.cmd_count
          && s_clip.rect_count == o_clip.rect_count))
        return false;
    for (int i = 0; i < s_clip.rect_count; i++)
        if (!(s_clip.rect[i] == o_clip.rect[i]))
            return false;
    return true;
}

class TestGraphic : public ::testing::Test
{
public:
    void SetUp()
    {
        const int len = 800 * 600 * 4;
        baddr = (unsigned char *)malloc(len);
        memset(baddr, 0, len);
        inq = inq_create();
        gctx = graphic_context_create();
        outq = outq_create();
    }

    void TearDown()
    {
        free(baddr);
        inq_destroy(inq);
        graphic_context_destroy(gctx);
        outq_destroy(outq);
    }

    void fill_hdr(struct vrde_framebuffer_info *info, struct vrde_hdr *hdr, uint8_t ch)
    {
        for (int i = hdr->y; i < hdr->y + hdr->h; i++) {
            memset(info->bits + (i * frameInfo.width + hdr->x) * frameInfo.bpp / 8,
                   ch, hdr->w * frameInfo.bpp / 8);
        }
    }

    void set_sample_bitmap(struct svp_bitmap *bitmap, SVP_PIXEL_FORMAT format = SPF_RGB32)
    {
        int size;

        if (!bitmap)
            return;
        switch (format) {
        case SPF_RGB32:
            size = 10 * 10 * 4;
            break;
        case SPF_RGB24:
            size = 10 * 10 * 3;
            break;
        case SPF_RGB16:
            size = 10 * 10 * 2;
            break;
        case SPF_RGB8:
            size = 10 * 10;
            break;
        case SPF_MONO:
            size = padded_length(10, 10);
            break;
        default:
            size = 0;
        }
        *bitmap = {format, 0, 0, 10, 10, size};
        bitmap->bits = (uint8_t *)malloc(bitmap->size);
        if (!bitmap->bits)
            return;
        memset(bitmap->bits, 0x80, bitmap->size);
    }

    struct vrde_framebuffer_info frameInfo;
    unsigned char *baddr;
    struct inqueue *inq;
    struct outqueue *outq;
    struct graphic_context *gctx;
    struct graphic_option opt;
};

TEST_F(TestGraphic, SetOption)
{
    struct graphic_option set_opt = {SX_LZ4, 1};
    struct graphic_option new_opt;

    ASSERT_EQ(0, graphic_set_option(gctx, &set_opt));
    ASSERT_EQ(0, graphic_get_option(gctx, &new_opt));
    ASSERT_EQ(set_opt.force_codec, new_opt.force_codec);
}

TEST_F(TestGraphic, SetBadOption)
{
    struct graphic_option old_opt;
    struct graphic_option set_opt = {123, 1};
    struct graphic_option new_opt;

    graphic_get_option(gctx, &old_opt);
    ASSERT_EQ(-1, graphic_set_option(gctx, &set_opt));
    graphic_get_option(gctx, &new_opt);
    ASSERT_EQ(old_opt.force_codec, new_opt.force_codec);
}

TEST_F(TestGraphic, Dummy)
{
    ASSERT_EQ(0, graphic_output(gctx, outq));
}

TEST_F(TestGraphic, DIRTY_RECT_RGB32_Raw)
{
    struct svp_bitmap bitmap, *bitmap2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_DIRTY_RECT};

    set_sample_bitmap(&bitmap, SPF_RGB32);
    bitmap2 = (struct svp_bitmap *)malloc(sizeof(*bitmap2));
    set_sample_bitmap(bitmap2, SPF_RGB32);

    hdr.c_size = sizeof(struct pac_bitmap) + bitmap2->size;

    inq_push(inq, SC_DIRTY_RECT, bitmap2, sizeof(*bitmap2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_DIRTY_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_bitmap) + bitmap.size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(bitmap, *(struct pac_bitmap *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, DIRTY_RECT_RGB24_Raw)
{
    struct svp_bitmap bitmap, *bitmap2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_DIRTY_RECT};

    set_sample_bitmap(&bitmap, SPF_RGB24);
    bitmap2 = (struct svp_bitmap *)malloc(sizeof(*bitmap2));
    set_sample_bitmap(bitmap2, SPF_RGB24);

    hdr.c_size = sizeof(struct pac_bitmap) + bitmap2->size;

    inq_push(inq, SC_DIRTY_RECT, bitmap2, sizeof(*bitmap2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_DIRTY_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_bitmap) + bitmap.size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(bitmap, *(struct pac_bitmap *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, DIRTY_RECT_RGB16_Raw)
{
    struct svp_bitmap bitmap, *bitmap2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_DIRTY_RECT};

    set_sample_bitmap(&bitmap, SPF_RGB16);
    bitmap2 = (struct svp_bitmap *)malloc(sizeof(*bitmap2));
    set_sample_bitmap(bitmap2, SPF_RGB16);

    hdr.c_size = sizeof(struct pac_bitmap) + bitmap2->size;

    inq_push(inq, SC_DIRTY_RECT, bitmap2, sizeof(*bitmap2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_DIRTY_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_bitmap) + bitmap.size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(bitmap, *(struct pac_bitmap *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, DIRTY_RECT_RGB8_Raw)
{
    struct svp_bitmap bitmap, *bitmap2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_DIRTY_RECT};

    set_sample_bitmap(&bitmap, SPF_RGB8);
    bitmap2 = (struct svp_bitmap *)malloc(sizeof(*bitmap2));
    set_sample_bitmap(bitmap2, SPF_RGB8);

    hdr.c_size = sizeof(struct pac_bitmap) + bitmap2->size;

    inq_push(inq, SC_DIRTY_RECT, bitmap2, sizeof(*bitmap2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_DIRTY_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_bitmap) + bitmap.size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(bitmap, *(struct pac_bitmap *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, SOLID_RECT)
{
    struct svp_solid_rect rect = {0, 0, 10, 10, 0xffff0000}, *rect2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_SOLID_RECT, sizeof(struct pac_solid_rect)};

    rect2 = (struct svp_solid_rect *)malloc(sizeof(*rect2));
    *rect2 = rect;
    inq_push(inq, SC_SOLID_RECT, rect2, sizeof(*rect2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_SOLID_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_solid_rect), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(rect, *(struct pac_solid_rect *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, SOLID_BLT)
{
    struct svp_solid_blt blt = {0, 0, 10, 10, 0xffff0000, 3}, *blt2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_SOLID_BLT, sizeof(struct pac_solid_blt)};

    blt2 = (struct svp_solid_blt *)malloc(sizeof(*blt2));
    *blt2 = blt;
    inq_push(inq, SC_SOLID_BLT, blt2, sizeof(*blt2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_SOLID_BLT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_solid_blt), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(blt, *(struct pac_solid_blt *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, DST_BLT)
{
    struct svp_dst_blt blt = {0, 0, 10, 10, 3}, *blt2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_DST_BLT, sizeof(struct pac_dst_blt)};

    blt2 = (struct svp_dst_blt *)malloc(sizeof(*blt2));
    *blt2 = blt;

    inq_push(inq, SC_DST_BLT, blt2, sizeof(*blt2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_DST_BLT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_dst_blt), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(blt, *(struct pac_dst_blt *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, SCREEN_BLT)
{
    struct svp_screen_blt blt = {0, 0, 10, 10, 100, 200, 3}, *blt2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_SCREEN_BLT, sizeof(struct pac_screen_blt)};

    blt2 = (struct svp_screen_blt *)malloc(sizeof(*blt2));
    *blt2 = blt;

    inq_push(inq, SC_SCREEN_BLT, blt2, sizeof(*blt2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_SCREEN_BLT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_screen_blt), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(blt, *(struct pac_screen_blt *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, PAT_BLT_BRUSH)
{
    struct svp_pat_blt_brush blt = {0, 0, 10, 10, 3, 3, 0xffff0000, 0xff0000ff, 3}, *blt2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_PATTERN_BLT_BRUSH, sizeof(struct pac_pattern_blt_brush)};

    memset(blt.pattern, 0x80, sizeof(blt.pattern));

    blt2 = (struct svp_pat_blt_brush *)malloc(sizeof(*blt2));
    *blt2 = blt;

    inq_push(inq, SC_PATTERN_BLT_BRUSH, blt2, sizeof(*blt2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_PATTERN_BLT_BRUSH, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_pattern_blt_brush), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(blt, *(struct pac_pattern_blt_brush *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, MEM_BLT)
{
    struct svp_mem_blt blt = {0, 0, 10, 10, 100, 200, 3}, *blt2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_MEM_BLT, sizeof(struct pac_mem_blt)};

    memset(blt.hash, 0x80, sizeof(blt.hash));

    blt2 = (struct svp_mem_blt *)malloc(sizeof(*blt2));
    *blt2 = blt;

    inq_push(inq, SC_MEM_BLT, blt2, sizeof(*blt2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_MEM_BLT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_mem_blt), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(blt, *(struct pac_mem_blt *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, CACHED_BITMAP)
{
    struct svp_cached_bitmap cache = {0}, *cache2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_CACHED_BITMAP};

    memset(cache.hash, 0x80, sizeof(cache.hash));
    set_sample_bitmap(&cache.bitmap);
    hdr.c_size = sizeof(struct pac_cached_bitmap) + sizeof(struct pac_bitmap) + cache.bitmap.size;

    cache2 = (struct svp_cached_bitmap *)malloc(sizeof(*cache2));
    *cache2 = cache;
    set_sample_bitmap(&cache2->bitmap);

    inq_push(inq, SC_CACHED_BITMAP, cache2, sizeof(*cache2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_CACHED_BITMAP, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_cached_bitmap)
              + sizeof(struct pac_bitmap) + cache.bitmap.size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(cache, *(struct pac_cached_bitmap *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, DELETED_BITMAP)
{
    struct svp_deleted_bitmap cache = {0}, *cache2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_DELETED_BITMAP, sizeof(struct pac_deleted_bitmap)};

    memset(cache.hash, 0x80, sizeof(cache.hash));

    cache2 = (struct svp_deleted_bitmap *)malloc(sizeof(*cache2));
    *cache2 = cache;

    inq_push(inq, SC_DELETED_BITMAP, cache2, sizeof(*cache2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_DELETED_BITMAP, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_deleted_bitmap), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(cache, *(struct pac_deleted_bitmap *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, LINE)
{
    struct svp_line line = {0, 0, 10, 10, 0, 0, 10, 10, 3, 0xffff0000}, *line2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_LINE, sizeof(struct pac_line)};

    line2 = (struct svp_line *)malloc(sizeof(*line2));
    *line2 = line;

    inq_push(inq, SC_LINE, line2, sizeof(*line2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_LINE, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_line), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(line, *(struct pac_line *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, POLYLINE)
{
    struct svp_polyline polyline = {0, 0, 3, 0xffff0000, 2}, *polyline2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_POLYLINE, sizeof(struct pac_polyline)};

    polyline.points[0] = {0, 0};
    polyline.points[1] = {10, 10};

    polyline2 = (struct svp_polyline *)malloc(sizeof(*polyline2));
    *polyline2 = polyline;

    inq_push(inq, SC_POLYLINE, polyline2, sizeof(*polyline2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_POLYLINE, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_polyline), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(polyline, *(struct pac_polyline *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, ELLIPSE)
{
    struct svp_ellipse el = {0, 0, 10, 10, 3, 5, 0xffff0000}, *el2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_ELLIPSE, sizeof(struct pac_ellipse)};

    el2 = (struct svp_ellipse *)malloc(sizeof(*el2));
    *el2 = el;

    inq_push(inq, SC_ELLIPSE, el2, sizeof(*el2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_ELLIPSE, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_ellipse), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(el, *(struct pac_ellipse *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, SAVE_SCREEN)
{
    struct svp_save_screen save = {0, 0, 10, 10, 1}, *save2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_SAVE_SCREEN, sizeof(struct pac_save_screen)};

    save2 = (struct svp_save_screen *)malloc(sizeof(*save2));
    *save2 = save;

    inq_push(inq, SC_SAVE_SCREEN, save2, sizeof(*save2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_SAVE_SCREEN, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_save_screen), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(save, *(struct pac_save_screen *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, RESTORE_SCREEN)
{
    struct svp_restore_screen restore = {0, 0, 10, 10, 1}, *restore2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_RESTORE_SCREEN,
                sizeof(struct pac_save_screen) + sizeof(struct pac_bitmap) + 10 * 10 * 4};

    set_sample_bitmap(&restore.bitmap);

    restore2 = (struct svp_restore_screen *)malloc(sizeof(*restore2));
    *restore2 = restore;
    set_sample_bitmap(&restore2->bitmap);

    inq_push(inq, SC_RESTORE_SCREEN, restore2, sizeof(*restore2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_RESTORE_SCREEN, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_save_screen)
              + sizeof(struct pac_bitmap) + 10 * 10 * 4, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(restore, *(struct pac_save_screen *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, TEXT_Empty)
{
    struct svp_text text = {0, 5, 10, 20, 0, 5, 10, 20, 20, 1, 2, 0xffff0000, 0xff0000ff, 0}, *text2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_TEXT, sizeof(struct pac_text)};

    INIT_LIST_HEAD(&text.glyph_list);

    text2 = (struct svp_text *)malloc(sizeof(*text2));
    *text2 = text;
    INIT_LIST_HEAD(&text2->glyph_list);

    inq_push(inq, SC_TEXT, text2, sizeof(*text2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_TEXT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_text), cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(text, *(struct pac_text *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, TEXT_OneChar)
{
    struct svp_text text = {0, 5, 10, 20, 0, 5, 10, 20, 20, 1, 2, 0xffff0000, 0xff0000ff, 1}, *text2;
    struct svp_glyph glyph = {0x1234567887654321, 0, 0}, *glyph2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_TEXT};

    INIT_LIST_HEAD(&text.glyph_list);
    INIT_LIST_HEAD(&glyph.link);
    set_sample_bitmap(&glyph.bitmap, SPF_MONO);
    list_add_tail(&glyph.link, &text.glyph_list);

    text2 = (struct svp_text *)malloc(sizeof(*text2));
    *text2 = text;
    glyph2 = (struct svp_glyph *)malloc(sizeof(*glyph2));
    *glyph2 = glyph;
    INIT_LIST_HEAD(&text2->glyph_list);
    INIT_LIST_HEAD(&glyph2->link);
    set_sample_bitmap(&glyph2->bitmap, SPF_MONO);
    list_add_tail(&glyph2->link, &text2->glyph_list);

    hdr.c_size = sizeof(pac_text) + sizeof(pac_glyph) + glyph2->bitmap.size;
    inq_push(inq, SC_TEXT, text2, sizeof(*text2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_TEXT, cmd[0].type);
    ASSERT_EQ(sizeof(hdr) + hdr.c_size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(text, *(struct pac_text *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, TEXT_TwoChar)
{
    struct svp_text text = {0, 5, 10, 20, 0, 5, 10, 20, 20, 1, 2, 0xffff0000, 0xff0000ff, 2}, *text2;
    struct svp_glyph glyph = {0x1234567887654321, 0, 0}, *glyph2;
    struct out_cmd cmd[2];
    struct pac_hdr hdr = {SVP_MAGIC, SC_TEXT};

    INIT_LIST_HEAD(&text.glyph_list);
    INIT_LIST_HEAD(&glyph.link);
    set_sample_bitmap(&glyph.bitmap, SPF_MONO);
    list_add_tail(&glyph.link, &text.glyph_list);

    text2 = (struct svp_text *)malloc(sizeof(*text2));
    *text2 = text;
    INIT_LIST_HEAD(&text2->glyph_list);

    for (int i = 0; i < 2; i++) {
        glyph2 = (struct svp_glyph *)malloc(sizeof(*glyph2));
        *glyph2 = glyph;
        INIT_LIST_HEAD(&glyph2->link);
        set_sample_bitmap(&glyph2->bitmap, SPF_MONO);
        list_add_tail(&glyph2->link, &text2->glyph_list);
    }

    hdr.c_size = sizeof(pac_text) + sizeof(pac_glyph) + glyph2->bitmap.size
                 + sizeof(pac_glyph) + glyph2->bitmap.size;
    inq_push(inq, SC_TEXT, text2, sizeof(*text2));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(1, outq_popn(outq, cmd, 2));

    ASSERT_EQ(SC_TEXT, cmd[0].type);
    ASSERT_EQ(sizeof(hdr) + hdr.c_size, cmd[0].size);
    ASSERT_EQ(hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(text, *(struct pac_text *)(cmd[0].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, CLIP_DRAW)
{
    struct svp_clip_draw clip, *clip2;
    struct svp_solid_rect rect = {0, 0, 100, 100, 0xff0000ff}, *rect2;
    struct pac_hdr clip_hdr = {SVP_MAGIC, SC_CLIP_DRAW, sizeof(pac_clip_draw) + sizeof(pac_rect)};
    struct pac_hdr rect_hdr = {SVP_MAGIC, SC_SOLID_RECT, sizeof(pac_solid_rect)};
    struct out_cmd cmd[5];

    clip.rect_count = 1;
    clip.rect[0] = {0, 0, 10, 10};
    clip.cmd_count = 1;
    clip.cmd[0].type = SC_SOLID_RECT;
    clip.cmd[0].data = (uint8_t *)&rect;
    clip.cmd[0].size = sizeof(rect);

    clip2 = (struct svp_clip_draw *)malloc(sizeof(*clip2));
    *clip2 = clip;
    rect2 = (struct svp_solid_rect *)malloc(sizeof(*rect2));
    *rect2 = rect;
    clip2->cmd[0].data = (uint8_t *)rect2;

    inq_push(inq, SC_CLIP_DRAW, clip2, sizeof(*clip2));
    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(2, outq_popn(outq, cmd, 5));

    ASSERT_EQ(SC_CLIP_DRAW, cmd[0].type);
    ASSERT_EQ(sizeof(clip_hdr) + sizeof(pac_clip_draw) + sizeof(pac_rect), cmd[0].size);
    ASSERT_EQ(clip_hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(clip, *(struct pac_clip_draw *)(cmd[0].data + sizeof(clip_hdr)));

    ASSERT_EQ(SC_SOLID_RECT, cmd[1].type);
    ASSERT_EQ(sizeof(rect_hdr) + sizeof(pac_solid_rect), cmd[1].size);
    ASSERT_EQ(rect_hdr, *(struct pac_hdr *)cmd[1].data);
    ASSERT_EQ(rect, *(struct pac_solid_rect *)(cmd[1].data + sizeof(clip_hdr)));
}

TEST_F(TestGraphic, Multiple)
{
    struct svp_solid_rect rect1 = {0, 0, 10, 10, 0xffff0000}, *rect1_copy;
    struct svp_solid_rect rect2 = {10, 20, 100, 20, 0xff0000ff}, *rect2_copy;
    struct out_cmd cmd[5];
    struct pac_hdr rect_hdr = {SVP_MAGIC, SC_SOLID_RECT, sizeof(struct pac_solid_rect)};

    rect1_copy = (struct svp_solid_rect *)malloc(sizeof(*rect1_copy));
    *rect1_copy = rect1;
    rect2_copy = (struct svp_solid_rect *)malloc(sizeof(*rect2_copy));
    *rect2_copy = rect2;

    inq_push(inq, SC_SOLID_RECT, rect1_copy, sizeof(*rect1_copy));
    inq_push(inq, SC_SOLID_RECT, rect2_copy, sizeof(*rect2_copy));

    ASSERT_EQ(0, graphic_input(gctx, inq));
    ASSERT_EQ(0, graphic_output(gctx, outq));
    ASSERT_EQ(2, outq_popn(outq, cmd, 5));

    ASSERT_EQ(SC_SOLID_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_solid_rect), cmd[0].size);
    ASSERT_EQ(rect_hdr, *(struct pac_hdr *)cmd[0].data);
    ASSERT_EQ(rect1, *(struct pac_solid_rect *)(cmd[0].data + sizeof(pac_hdr)));

    ASSERT_EQ(SC_SOLID_RECT, cmd[1].type);
    ASSERT_EQ(sizeof(struct pac_hdr) + sizeof(struct pac_solid_rect), cmd[1].size);
    ASSERT_EQ(rect_hdr, *(struct pac_hdr *)cmd[1].data);
    ASSERT_EQ(rect2, *(struct pac_solid_rect *)(cmd[1].data + sizeof(pac_hdr)));
}

TEST_F(TestGraphic, BadInput)
{
    int *i;

    i = (int *)malloc(sizeof(*i));
    *i = 2;

    inq_push(inq, 123, 0, 0);
    ASSERT_NE(0, graphic_input(gctx, inq));

    inq_clear(inq, 0);
    inq_push(inq, SC_SOLID_RECT, i, sizeof(*i));
    ASSERT_NE(0, graphic_input(gctx, inq));
}
