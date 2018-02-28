#include <gtest/gtest.h>
extern "C" {
#include "server/vrde.c"
#include "server/server.c"
#include "server/graphic.c"
#include "server/inqueue.c"
#include "server/outqueue.c"
#include "server/cmd.c"
#include "server/codec.c"
#include "server/encoder.c"
}

/*
 * indirect
 * VRDE_DIRTY_RECT
 * VRDE_SOLID_RECT
 * VRDE_SOLID_BLT
 * VRDE_DST_BLT
 * VRDE_SCREEN_BLT
 * VRDE_PAT_BLT_BRUSH
 * VRDE_MEM_BLT
 * VRDE_CACHED_BITMAP
 * VRDE_DELETED_BITMAP
 * VRDE_LINE
 * VRDE_POLYLINE
 * VRDE_ELLIPSE
 * VRDE_SAVE_SCREEN
 * VRDE_TEXT
 * VRDE_BOUNDS
 * VRDE_BOUNDS + VRDE_REPEAT
 * error: VRDE_REPEAT only
 * multiple VRDE command
 * error: unknown command
 * error: malformed VRDE header
 */

bool operator==(const vrde_bounds &bounds, const svp_rect &clip_rect)
{
    return bounds.pt1.x == clip_rect.x
            && bounds.pt1.y == clip_rect.y
            && bounds.pt2.x == clip_rect.x + clip_rect.w
            && bounds.pt2.y == clip_rect.y + clip_rect.h;
}

bool operator==(const vrde_solid_rect &v_rect, const svp_solid_rect &s_rect)
{
    return v_rect.x == s_rect.x
            && v_rect.y == s_rect.y
            && v_rect.w == s_rect.w
            && v_rect.h == s_rect.h
            && v_rect.rgb == s_rect.rgb;
}

static inline int get_Bpp(SVP_PIXEL_FORMAT format)
{
    switch (format) {
    case SPF_RGB32:
        return 4;
    case SPF_RGB24:
        return 3;
    case SPF_RGB16:
        return 2;
    case SPF_RGB8:
        return 1;
    case SPF_INVALID:
    case SPF_MONO:
    default:
        return -1;
    }
}

bool operator==(const vrde_databits &v_databits, const svp_bitmap &s_bitmap)
{
    return v_databits.x == s_bitmap.x
            && v_databits.y == s_bitmap.y
            && v_databits.w == s_bitmap.w
            && v_databits.h == s_bitmap.h
            && v_databits.Bpp == get_Bpp(s_bitmap.format)
            && v_databits.size == s_bitmap.size
            && memcmp((uint8_t *)&v_databits + sizeof(v_databits),
                      s_bitmap.bits, s_bitmap.size) == 0;
}

class VRDETest : public ::testing::Test
{
public:
    void SetUp()
    {
        const int len = 800 * 600 * 4;
        baddr = (unsigned char *)malloc(len);
        memset(baddr, 0, len);
        inq = inq_create();
    }

    void TearDown()
    {
        free(baddr);
        inq_destroy(inq);
    }

    void fill_hdr(struct vrde_framebuffer_info *info, struct vrde_hdr *hdr, uint8_t ch)
    {
        for (int i = hdr->y; i < hdr->y + hdr->h; i++) {
            memset(info->bits + (i * frameInfo.width + hdr->x) * frameInfo.bpp / 8,
                   ch, hdr->w * frameInfo.bpp / 8);
        }
    }

    struct vrde_framebuffer_info frameInfo;
    unsigned char *baddr;
    struct inqueue *inq;
};

TEST_F(VRDETest, SetFrameBuffer)
{
    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);
    ASSERT_EQ(800, frameInfo.width);
    ASSERT_EQ(600, frameInfo.height);
    ASSERT_EQ(32, frameInfo.bpp);
    ASSERT_EQ(VRDE_RGB32, frameInfo.format);
}

TEST_F(VRDETest, Indirect_RGB32)
{
    struct vrde_hdr hdr;
    struct in_cmd cmd;
    struct svp_bitmap *rect;
    int i;
    int j;

    memset(&cmd, 0, sizeof(cmd));

    hdr = {10, 20, 2, 2};
    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);
    fill_hdr(&frameInfo, &hdr, 0x80);

    ASSERT_EQ(0, vrde_process(&frameInfo, (uint8_t *)&hdr, sizeof(hdr), inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_DIRTY_RECT, cmd.type);
    rect = (struct svp_bitmap *)cmd.data;
    ASSERT_EQ(sizeof(*rect), cmd.size);
    ASSERT_EQ(hdr.w * hdr.h * frameInfo.bpp / 8, rect->size);
    ASSERT_EQ(hdr.w, rect->w);
    ASSERT_EQ(hdr.h, rect->h);
    ASSERT_EQ(SPF_RGB32, rect->format);
    for (i = 0; i < hdr.h; i++)
        for (j = 0; j < hdr.w; j++) {
            ASSERT_EQ(0x80808080, *(uint32_t *)&rect->bits[(i * hdr.w + j) * frameInfo.bpp / 8]);
        }
}

TEST_F(VRDETest, Indirect_RGB16)
{
    struct vrde_hdr hdr;
    struct in_cmd cmd;
    struct svp_bitmap *rect;
    int i;
    int j;

    memset(&cmd, 0, sizeof(cmd));

    hdr = {10, 20, 2, 2};
    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB16, baddr);
    fill_hdr(&frameInfo, &hdr, 0x80);

    ASSERT_EQ(0, vrde_process(&frameInfo, (uint8_t *)&hdr, sizeof(hdr), inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_DIRTY_RECT, cmd.type);
    rect = (struct svp_bitmap *)cmd.data;
    ASSERT_EQ(sizeof(*rect), cmd.size);
    ASSERT_EQ(hdr.w * hdr.h * frameInfo.bpp / 8, rect->size);
    ASSERT_EQ(hdr.w, rect->w);
    ASSERT_EQ(hdr.h, rect->h);
    ASSERT_EQ(SPF_RGB16, rect->format);
    for (i = 0; i < hdr.h; i++)
        for (j = 0; j < hdr.w; j++) {
            ASSERT_EQ(0x8080, *(uint16_t *)&rect->bits[(i * hdr.w + j) * frameInfo.bpp / 8]);
        }
}

TEST_F(VRDETest, Indirect_RGB8)
{
    struct vrde_hdr hdr;
    struct in_cmd cmd;
    struct svp_bitmap *rect;
    int i;
    int j;

    memset(&cmd, 0, sizeof(cmd));

    hdr = {10, 20, 2, 2};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB8, baddr);
    fill_hdr(&frameInfo, &hdr, 0x80);

    ASSERT_EQ(0, vrde_process(&frameInfo, (uint8_t *)&hdr, sizeof(hdr), inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_DIRTY_RECT, cmd.type);
    rect = (struct svp_bitmap *)cmd.data;
    ASSERT_EQ(sizeof(*rect), cmd.size);
    ASSERT_EQ(hdr.w * hdr.h * frameInfo.bpp / 8, rect->size);
    ASSERT_EQ(hdr.w, rect->w);
    ASSERT_EQ(hdr.h, rect->h);
    ASSERT_EQ(SPF_RGB8, rect->format);
    for (i = 0; i < hdr.h; i++)
        for (j = 0; j < hdr.w; j++) {
            ASSERT_EQ(0x80, rect->bits[(i * hdr.w + j) * frameInfo.bpp / 8]);
        }
}

TEST_F(VRDETest, DIRTY_RECT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_databits) + 2 * 2 * 4;
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_databits *v_bits = (struct vrde_databits *)(data + sizeof(*hdr) + sizeof(*order));
    uint8_t *v_data = (uint8_t *)v_bits + sizeof(*v_bits);
    struct svp_bitmap *s_bitmap;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_DIRTY_RECT;
    *v_bits = {2 * 2 * 4, 10, 20, 2, 2, 4};
    memset(v_data, 0x80, v_bits->size);

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB8, baddr);
    fill_hdr(&frameInfo, hdr, 0x80);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_DIRTY_RECT, cmd.type);
    s_bitmap = (struct svp_bitmap *)cmd.data;
    ASSERT_EQ(sizeof(*s_bitmap), cmd.size);
    ASSERT_EQ(v_bits->size, s_bitmap->size);
    ASSERT_EQ(v_bits->w, s_bitmap->w);
    ASSERT_EQ(v_bits->h, s_bitmap->h);
    ASSERT_EQ(SPF_RGB32, s_bitmap->format);
    ASSERT_TRUE(memcmp(v_data, s_bitmap->bits, s_bitmap->size) == 0);
}

TEST_F(VRDETest, SOLID_RECT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_solid_rect);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_solid_rect *vrde_rect = (struct vrde_solid_rect *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_solid_rect *svp_rect;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_SOLID_RECT;
    *vrde_rect = {10, 20, 2, 2, 0xff0000ff};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_SOLID_RECT, cmd.type);
    svp_rect = (struct svp_solid_rect *)cmd.data;
    ASSERT_EQ(sizeof(*svp_rect), cmd.size);
    ASSERT_EQ(*vrde_rect, *svp_rect);
}

TEST_F(VRDETest, SOLID_BLT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_solid_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_solid_blt *vrde_blt = (struct vrde_solid_blt *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_solid_blt *svp_blt;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_SOLID_BLT;
    *vrde_blt = {10, 20, 2, 2, 0xff0000ff};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_SOLID_BLT, cmd.type);
    svp_blt = (struct svp_solid_blt *)cmd.data;
    ASSERT_EQ(sizeof(*svp_blt), cmd.size);
    ASSERT_EQ(svp_blt->x, vrde_blt->x);
    ASSERT_EQ(svp_blt->y, vrde_blt->y);
    ASSERT_EQ(svp_blt->w, vrde_blt->w);
    ASSERT_EQ(svp_blt->h, vrde_blt->h);
    ASSERT_EQ(svp_blt->rgb, vrde_blt->rgb);
}

TEST_F(VRDETest, DST_BLT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_dst_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_dst_blt *vrde_blt = (struct vrde_dst_blt *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_dst_blt *svp_blt;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_DST_BLT;
    *vrde_blt = {10, 20, 2, 2, 7};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_DST_BLT, cmd.type);
    svp_blt = (struct svp_dst_blt *)cmd.data;
    ASSERT_EQ(sizeof(*svp_blt), cmd.size);
    ASSERT_EQ(svp_blt->x, vrde_blt->x);
    ASSERT_EQ(svp_blt->y, vrde_blt->y);
    ASSERT_EQ(svp_blt->w, vrde_blt->w);
    ASSERT_EQ(svp_blt->h, vrde_blt->h);
    ASSERT_EQ(svp_blt->rop, vrde_blt->rop);
}

TEST_F(VRDETest, SCREEN_BLT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_screen_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_screen_blt *vrde_blt = (struct vrde_screen_blt *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_screen_blt *svp_blt;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_SCREEN_BLT;
    *vrde_blt = {10, 20, 2, 2, 1, 1, 7};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_SCREEN_BLT, cmd.type);
    svp_blt = (struct svp_screen_blt *)cmd.data;
    ASSERT_EQ(sizeof(*svp_blt), cmd.size);
    ASSERT_EQ(svp_blt->x, vrde_blt->x);
    ASSERT_EQ(svp_blt->y, vrde_blt->y);
    ASSERT_EQ(svp_blt->w, vrde_blt->w);
    ASSERT_EQ(svp_blt->h, vrde_blt->h);
    ASSERT_EQ(svp_blt->x_src, vrde_blt->x_src);
    ASSERT_EQ(svp_blt->y_src, vrde_blt->y_src);
    ASSERT_EQ(svp_blt->rop, vrde_blt->rop);
}

TEST_F(VRDETest, PAT_BLT_BRUSH)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_pat_blt_brush);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_pat_blt_brush *vrde_blt = (struct vrde_pat_blt_brush *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_pat_blt_brush *svp_blt;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_PAT_BLT_BRUSH;
    *vrde_blt = {10, 20, 2, 2, 1, 1, 0xff0000ff, 0xffff, 7};
    vrde_blt->pattern = {0, 1, 2, 3, 4, 5, 6, 7};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_PATTERN_BLT_BRUSH, cmd.type);
    svp_blt = (struct svp_pat_blt_brush *)cmd.data;
    ASSERT_EQ(sizeof(*svp_blt), cmd.size);
    ASSERT_EQ(svp_blt->x, vrde_blt->x);
    ASSERT_EQ(svp_blt->y, vrde_blt->y);
    ASSERT_EQ(svp_blt->w, vrde_blt->w);
    ASSERT_EQ(svp_blt->h, vrde_blt->h);
    ASSERT_EQ(svp_blt->rgb_fg, vrde_blt->rgb_fg);
    ASSERT_EQ(svp_blt->rgb_bg, vrde_blt->rgb_bg);
    ASSERT_EQ(svp_blt->rop, vrde_blt->rop);
    ASSERT_EQ(sizeof(svp_blt->pattern), sizeof(vrde_blt->pattern));
    ASSERT_TRUE(memcmp(svp_blt->pattern, vrde_blt->pattern, sizeof(vrde_blt->pattern)) == 0);
}

TEST_F(VRDETest, MEM_BLT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_mem_blt);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_mem_blt *vrde_blt = (struct vrde_mem_blt *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_mem_blt *svp_blt;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_MEM_BLT;
    *vrde_blt = {10, 20, 2, 2, 1, 1, 7};
    vrde_blt->hash = {0, 1, 2, 3, 4, 5, 6, 7,
                     8, 9, 10, 11, 12, 13, 14, 15};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_MEM_BLT, cmd.type);
    svp_blt = (struct svp_mem_blt *)cmd.data;
    ASSERT_EQ(sizeof(*svp_blt), cmd.size);
    ASSERT_EQ(svp_blt->x, vrde_blt->x);
    ASSERT_EQ(svp_blt->y, vrde_blt->y);
    ASSERT_EQ(svp_blt->w, vrde_blt->w);
    ASSERT_EQ(svp_blt->h, vrde_blt->h);
    ASSERT_EQ(svp_blt->x_src, vrde_blt->x_src);
    ASSERT_EQ(svp_blt->y_src, vrde_blt->y_src);
    ASSERT_EQ(svp_blt->rop, vrde_blt->rop);
    ASSERT_EQ(sizeof(svp_blt->hash), sizeof(vrde_blt->hash));
    ASSERT_TRUE(memcmp(svp_blt->hash, vrde_blt->hash, sizeof(vrde_blt->hash)) == 0);
}

TEST_F(VRDETest, CACHED_BITMAP)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_cached_bitmap)
            + sizeof(struct vrde_databits) + 2 * 2 * 4;
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)((uint8_t *)hdr + sizeof(*hdr));
    struct vrde_cached_bitmap *vrde_cache = (struct vrde_cached_bitmap *)((uint8_t *)order + sizeof(*order));
    struct vrde_databits *vrde_bits = (struct vrde_databits *)((uint8_t *)vrde_cache + sizeof(*vrde_cache));
    uint8_t *bits = (uint8_t *)vrde_bits + sizeof(*vrde_bits);
    struct svp_cached_bitmap *svp_cache;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_CACHED_BITMAP;
    vrde_cache->hash = {0, 1, 2, 3, 4, 5, 6, 7,
                     8, 9, 10, 11, 12, 13, 14, 15};
    *vrde_bits = {2 * 2 * 4, 10, 20, 2, 2, 4};
    memset(bits, 0x80, 2 * 2 * 4);

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_CACHED_BITMAP, cmd.type);
    svp_cache = (struct svp_cached_bitmap *)cmd.data;
    ASSERT_EQ(sizeof(*svp_cache), cmd.size);
    ASSERT_EQ(sizeof(svp_cache->hash), sizeof(vrde_cache->hash));
    ASSERT_TRUE(memcmp(svp_cache->hash, vrde_cache->hash, sizeof(vrde_cache->hash)) == 0);
    ASSERT_EQ(*vrde_bits, svp_cache->bitmap);
}

TEST_F(VRDETest, DELETED_BITMAP)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_deleted_bitmap);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_deleted_bitmap *vrde_cache = (struct vrde_deleted_bitmap *)((uint8_t *)order + sizeof(*order));
    struct svp_deleted_bitmap *svp_cache;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 2, 2};
    *order = VRDE_DELETED_BITMAP;
    vrde_cache->hash = {0, 1, 2, 3, 4, 5, 6, 7,
                     8, 9, 10, 11, 12, 13, 14, 15};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_DELETED_BITMAP, cmd.type);
    svp_cache = (struct svp_deleted_bitmap *)cmd.data;
    ASSERT_EQ(sizeof(*svp_cache), cmd.size);
    ASSERT_EQ(sizeof(svp_cache->hash), sizeof(vrde_cache->hash));
    ASSERT_TRUE(memcmp(svp_cache->hash, vrde_cache->hash, sizeof(vrde_cache->hash)) == 0);
}

TEST_F(VRDETest, LINE)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_line);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)((uint8_t *)hdr + sizeof(*hdr));
    struct vrde_line *v_line = (struct vrde_line *)((uint8_t *)order + sizeof(*order));
    struct svp_line *s_line;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_LINE;
    *v_line = {10, 20, 110, 220, 10, 20, 110, 220, 7, 0xff0000ff};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_LINE, cmd.type);
    s_line = (struct svp_line *)cmd.data;
    ASSERT_EQ(sizeof(*s_line), cmd.size);
    ASSERT_EQ(s_line->x1, v_line->x1);
    ASSERT_EQ(s_line->y1, v_line->y1);
    ASSERT_EQ(s_line->x2, v_line->x2);
    ASSERT_EQ(s_line->y2, v_line->y2);
    ASSERT_EQ(s_line->x_bounds1, v_line->x_bounds1);
    ASSERT_EQ(s_line->y_bounds1, v_line->y_bounds1);
    ASSERT_EQ(s_line->x_bounds2, v_line->x_bounds2);
    ASSERT_EQ(s_line->y_bounds2, v_line->y_bounds2);
    ASSERT_EQ(s_line->mix, v_line->mix);
    ASSERT_EQ(s_line->rgb, v_line->rgb);
}

TEST_F(VRDETest, POLYLINE)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_polyline);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_polyline *v_polyline = (struct vrde_polyline *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_polyline *s_polyline;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_POLYLINE;
    v_polyline->pt_start = {10, 20};
    v_polyline->mix = 3;
    v_polyline->rgb = 0xff0000ff;
    v_polyline->points.c = 8;
    for (int i = 0; i < 8; i++)
        v_polyline->points.a[i] = {(int16_t)(10 + i * 10), (int16_t)(20 + i * 10)};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_POLYLINE, cmd.type);
    s_polyline = (struct svp_polyline *)cmd.data;
    ASSERT_EQ(sizeof(*s_polyline), cmd.size);
    ASSERT_EQ(v_polyline->pt_start.x, s_polyline->x_start);
    ASSERT_EQ(v_polyline->pt_start.y, s_polyline->y_start);
    ASSERT_EQ(v_polyline->mix, s_polyline->mix);
    ASSERT_EQ(v_polyline->rgb, s_polyline->rgb);
    ASSERT_EQ(v_polyline->points.c, s_polyline->count);
    for (int i = 0; i < v_polyline->points.c; i++) {
        ASSERT_EQ(v_polyline->points.a[i].x, s_polyline->points[i].x) << "at " << i;
        ASSERT_EQ(v_polyline->points.a[i].y, s_polyline->points[i].y) << "at " << i;
    }
}

TEST_F(VRDETest, ELLIPSE)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_ellipse);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_ellipse *v_ellipse = (struct vrde_ellipse *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_ellipse *s_ellipse;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_ELLIPSE;
    v_ellipse->pt1 = {10, 20};
    v_ellipse->pt2 = {110, 220};
    v_ellipse->mix = 3;
    v_ellipse->fill_mode = 1;
    v_ellipse->rgb = 0xff0000ff;

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_ELLIPSE, cmd.type);
    s_ellipse = (struct svp_ellipse *)cmd.data;
    ASSERT_EQ(sizeof(*s_ellipse), cmd.size);
    ASSERT_EQ(v_ellipse->pt1.x, s_ellipse->x);
    ASSERT_EQ(v_ellipse->pt1.y, s_ellipse->y);
    ASSERT_EQ(v_ellipse->pt2.x, s_ellipse->x + s_ellipse->w);
    ASSERT_EQ(v_ellipse->pt2.y, s_ellipse->y + s_ellipse->h);
    ASSERT_EQ(v_ellipse->mix, s_ellipse->mix);
    ASSERT_EQ(v_ellipse->fill_mode, s_ellipse->fill_mode);
    ASSERT_EQ(v_ellipse->rgb, s_ellipse->rgb);
}

TEST_F(VRDETest, SAVE_SCREEN)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_save_screen);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_save_screen *v_save_screen = (struct vrde_save_screen *)(data + sizeof(*hdr) + sizeof(*order));
    struct svp_save_screen *s_save_screen;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_SAVE_SCREEN;
    v_save_screen->pt1 = {10, 20};
    v_save_screen->pt2 = {110, 220};
    v_save_screen->ident = 1;
    v_save_screen->restore = 0;

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_SAVE_SCREEN, cmd.type);
    s_save_screen = (struct svp_save_screen *)cmd.data;
    ASSERT_EQ(sizeof(*s_save_screen), cmd.size);
    ASSERT_EQ(v_save_screen->pt1.x, s_save_screen->x);
    ASSERT_EQ(v_save_screen->pt1.y, s_save_screen->y);
    ASSERT_EQ(v_save_screen->pt2.x, s_save_screen->x + s_save_screen->w);
    ASSERT_EQ(v_save_screen->pt2.y, s_save_screen->y + s_save_screen->h);
    ASSERT_EQ(v_save_screen->ident, s_save_screen->ident);
}

TEST_F(VRDETest, RESTORE_SCREEN)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_save_screen)
            + sizeof(struct vrde_databits) + 2 * 2 * 4;
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_save_screen *v_save_screen = (struct vrde_save_screen *)((uint8_t *)order + sizeof(*order));
    struct vrde_databits *v_databits = (struct vrde_databits *)((uint8_t *)v_save_screen + sizeof(*v_save_screen));
    uint8_t *v_bits = (uint8_t *)v_databits + sizeof(*v_databits);
    struct svp_restore_screen *s_restore_screen;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_SAVE_SCREEN;
    v_save_screen->pt1 = {10, 20};
    v_save_screen->pt2 = {12, 22};
    v_save_screen->ident = 1;
    v_save_screen->restore = 1;

    *v_databits = {2 * 2 * 4, 10, 20, 2, 2, 4};

    memset(v_bits, 0x80, 2 * 2 * 4);

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_RESTORE_SCREEN, cmd.type);
    s_restore_screen = (struct svp_restore_screen *)cmd.data;
    ASSERT_EQ(sizeof(*s_restore_screen), cmd.size);
    ASSERT_EQ(v_save_screen->pt1.x, s_restore_screen->x);
    ASSERT_EQ(v_save_screen->pt1.y, s_restore_screen->y);
    ASSERT_EQ(v_save_screen->pt2.x, s_restore_screen->x + s_restore_screen->w);
    ASSERT_EQ(v_save_screen->pt2.y, s_restore_screen->y + s_restore_screen->h);
    ASSERT_EQ(v_save_screen->ident, s_restore_screen->ident);

    ASSERT_EQ(*v_databits, s_restore_screen->bitmap);
}

TEST_F(VRDETest, TEXT_Empty)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_text);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_text *v_text = (struct vrde_text *)((uint8_t *)order + sizeof(*order));
    struct svp_text *s_text;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_TEXT;
    *v_text = {sizeof(*v_text), 10, 20, 100, 20, 10, 20, 100, 20, 20,
               0, 1, 2, 0xff0000ff, 0xffff};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_TEXT, cmd.type);
    s_text = (struct svp_text *)cmd.data;
    ASSERT_EQ(sizeof(*s_text), cmd.size);
    ASSERT_EQ(v_text->x_bg, s_text->x_bg);
    ASSERT_EQ(v_text->y_bg, s_text->y_bg);
    ASSERT_EQ(v_text->w_bg, s_text->w_bg);
    ASSERT_EQ(v_text->h_bg, s_text->h_bg);

    ASSERT_EQ(v_text->x_opaque, s_text->x_opaque);
    ASSERT_EQ(v_text->y_opaque, s_text->y_opaque);
    ASSERT_EQ(v_text->w_opaque, s_text->w_opaque);
    ASSERT_EQ(v_text->h_opaque, s_text->h_opaque);

    ASSERT_EQ(v_text->max_glyph, s_text->max_glyph);
    ASSERT_EQ(v_text->flags, s_text->flags);
    ASSERT_EQ(v_text->char_inc, s_text->char_inc);
    ASSERT_EQ(v_text->rgb_fg, s_text->rgb_fg);
    ASSERT_EQ(v_text->rgb_bg, s_text->rgb_bg);

    ASSERT_EQ(v_text->glyphs, s_text->glyph_count);
    ASSERT_TRUE(list_empty(&s_text->glyph_list));
}

// one 8x16 character
TEST_F(VRDETest, TEXT_OneChar)
{
    struct in_cmd cmd = {0};
    const int bitmap_size = 16;  // 1bpp 8x16 glyph bitmap size: 16
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_text)
            + sizeof(struct vrde_glyph) + bitmap_size;
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_text *v_text = (struct vrde_text *)((uint8_t *)order + sizeof(*order));
    struct svp_text *s_text;
    struct vrde_glyph *v_glyph = (struct vrde_glyph *)((uint8_t *)v_text + sizeof(*v_text));
    struct svp_glyph *s_glyph;
    uint8_t *v_bits = (uint8_t *)v_glyph + sizeof(*v_glyph);

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_TEXT;
    *v_text = {sizeof(*v_text) + sizeof(*v_glyph) + bitmap_size, 10, 20, 100, 20, 10, 20, 100, 20, 20,
               1, 1, 2, 0xff0000ff, 0xffff};
    *v_glyph = {sizeof(*v_glyph) + bitmap_size, 0x8765432112345678, 10, 40, 8, 16, 10, 40};
    memset(v_bits, 0x80, bitmap_size);

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_TEXT, cmd.type);
    s_text = (struct svp_text *)cmd.data;
    ASSERT_EQ(sizeof(*s_text), cmd.size);
    ASSERT_EQ(v_text->x_bg, s_text->x_bg);
    ASSERT_EQ(v_text->y_bg, s_text->y_bg);
    ASSERT_EQ(v_text->w_bg, s_text->w_bg);
    ASSERT_EQ(v_text->h_bg, s_text->h_bg);

    ASSERT_EQ(v_text->x_opaque, s_text->x_opaque);
    ASSERT_EQ(v_text->y_opaque, s_text->y_opaque);
    ASSERT_EQ(v_text->w_opaque, s_text->w_opaque);
    ASSERT_EQ(v_text->h_opaque, s_text->h_opaque);

    ASSERT_EQ(v_text->max_glyph, s_text->max_glyph);
    ASSERT_EQ(v_text->flags, s_text->flags);
    ASSERT_EQ(v_text->char_inc, s_text->char_inc);
    ASSERT_EQ(v_text->rgb_fg, s_text->rgb_fg);
    ASSERT_EQ(v_text->rgb_bg, s_text->rgb_bg);

    ASSERT_EQ(v_text->glyphs, s_text->glyph_count);
    ASSERT_FALSE(list_empty(&s_text->glyph_list));

    s_glyph = list_first_entry(&s_text->glyph_list, struct svp_glyph, link);
    ASSERT_EQ(v_glyph->handle, s_glyph->handle);
    ASSERT_EQ(v_glyph->x_origin, s_glyph->x_origin);
    ASSERT_EQ(v_glyph->y_origin, s_glyph->y_origin);
    ASSERT_EQ(SPF_MONO, s_glyph->bitmap.format);
    ASSERT_EQ(v_glyph->x, s_glyph->bitmap.x);
    ASSERT_EQ(v_glyph->y, s_glyph->bitmap.y);
    ASSERT_EQ(v_glyph->w, s_glyph->bitmap.w);
    ASSERT_EQ(v_glyph->h, s_glyph->bitmap.h);
    ASSERT_EQ(v_glyph->size - sizeof(*v_glyph), s_glyph->bitmap.size);
    ASSERT_TRUE(memcmp(v_bits, s_glyph->bitmap.bits, s_glyph->bitmap.size) == 0);
}

// two 8x16 character
TEST_F(VRDETest, TEXT_TwoChar)
{
    struct in_cmd cmd = {0};
    const int bitmap_size = 16;  // 1bpp 8x16 glyph bitmap size: 16
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_text)
               + sizeof(struct vrde_glyph) + bitmap_size
               + sizeof(struct vrde_glyph) + bitmap_size;
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_text *v_text = (struct vrde_text *)((uint8_t *)order + sizeof(*order));
    struct svp_text *s_text;
    struct vrde_glyph *v_glyph = (struct vrde_glyph *)((uint8_t *)v_text + sizeof(*v_text));
    struct svp_glyph *s_glyph;
    uint8_t *v_bits = (uint8_t *)v_glyph + sizeof(*v_glyph);

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_TEXT;
    *v_text = {sizeof(*v_text) + sizeof(*v_glyph) + bitmap_size, 10, 20, 100, 20, 10, 20, 100, 20, 20,
               2, 1, 2, 0xff0000ff, 0xffff};
    *v_glyph = {sizeof(*v_glyph) + bitmap_size, 0x8765432112345678, 10, 40, 8, 16, 10, 40};
    memset(v_bits, 0x80, bitmap_size);

    v_glyph = (struct vrde_glyph *)(v_bits + bitmap_size);
    v_bits = (uint8_t *)v_glyph + sizeof(*v_glyph);
    *v_glyph = {sizeof(*v_glyph) + bitmap_size, 0x8765432112345678, 10, 40, 8, 16, 10, 40};
    memset(v_bits, 0x80, bitmap_size);

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_TEXT, cmd.type);
    s_text = (struct svp_text *)cmd.data;
    ASSERT_EQ(sizeof(*s_text), cmd.size);
    ASSERT_EQ(v_text->x_bg, s_text->x_bg);
    ASSERT_EQ(v_text->y_bg, s_text->y_bg);
    ASSERT_EQ(v_text->w_bg, s_text->w_bg);
    ASSERT_EQ(v_text->h_bg, s_text->h_bg);

    ASSERT_EQ(v_text->x_opaque, s_text->x_opaque);
    ASSERT_EQ(v_text->y_opaque, s_text->y_opaque);
    ASSERT_EQ(v_text->w_opaque, s_text->w_opaque);
    ASSERT_EQ(v_text->h_opaque, s_text->h_opaque);

    ASSERT_EQ(v_text->max_glyph, s_text->max_glyph);
    ASSERT_EQ(v_text->flags, s_text->flags);
    ASSERT_EQ(v_text->char_inc, s_text->char_inc);
    ASSERT_EQ(v_text->rgb_fg, s_text->rgb_fg);
    ASSERT_EQ(v_text->rgb_bg, s_text->rgb_bg);

    ASSERT_EQ(v_text->glyphs, s_text->glyph_count);
    ASSERT_FALSE(list_empty(&s_text->glyph_list));

    list_for_each_entry(s_glyph, &s_text->glyph_list, link) {
        ASSERT_EQ(v_glyph->handle, s_glyph->handle);
        ASSERT_EQ(v_glyph->x_origin, s_glyph->x_origin);
        ASSERT_EQ(v_glyph->y_origin, s_glyph->y_origin);
        ASSERT_EQ(SPF_MONO, s_glyph->bitmap.format);
        ASSERT_EQ(v_glyph->x, s_glyph->bitmap.x);
        ASSERT_EQ(v_glyph->y, s_glyph->bitmap.y);
        ASSERT_EQ(v_glyph->w, s_glyph->bitmap.w);
        ASSERT_EQ(v_glyph->h, s_glyph->bitmap.h);
        ASSERT_EQ(v_glyph->size - sizeof(*v_glyph), s_glyph->bitmap.size);
        ASSERT_TRUE(memcmp(v_bits, s_glyph->bitmap.bits, s_glyph->bitmap.size) == 0);
    }
}

// one VRDE_BOUNDS and one sub command
TEST_F(VRDETest, BOUNDS_Simple)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_bounds)
            + sizeof(uint32_t) + sizeof(struct vrde_solid_rect);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_bounds *v_bounds = (struct vrde_bounds *)(data + sizeof(*hdr) + sizeof(*order));
    uint32_t *sub_order = (uint32_t *)((uint8_t *)v_bounds + sizeof(*v_bounds));
    struct vrde_solid_rect *v_sub_rect = (struct vrde_solid_rect *)((uint8_t *)sub_order + sizeof(*sub_order));
    struct svp_clip_draw *s_clip_draw;
    struct svp_solid_rect *s_rect;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_BOUNDS;
    v_bounds->pt1 = {50, 100};
    v_bounds->pt2 = {80, 150};
    *sub_order = VRDE_SOLID_RECT;
    *v_sub_rect = {50, 100, 30, 50, 0xff0000ff};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_CLIP_DRAW, cmd.type);
    s_clip_draw = (struct svp_clip_draw *)cmd.data;
    ASSERT_EQ(sizeof(*s_clip_draw), cmd.size);
    ASSERT_EQ(1, s_clip_draw->rect_count);
    ASSERT_EQ(*v_bounds, s_clip_draw->rect[0]);
    ASSERT_EQ(1, s_clip_draw->cmd_count);
    ASSERT_EQ(SC_SOLID_RECT, s_clip_draw->cmd[0].type);
    ASSERT_EQ(sizeof(*s_rect), s_clip_draw->cmd[0].size);
    s_rect = (struct svp_solid_rect *)s_clip_draw->cmd[0].data;
    ASSERT_EQ(v_sub_rect->x, s_rect->x);
    ASSERT_EQ(v_sub_rect->y, s_rect->y);
    ASSERT_EQ(v_sub_rect->w, s_rect->w);
    ASSERT_EQ(v_sub_rect->h, s_rect->h);
    ASSERT_EQ(v_sub_rect->rgb, s_rect->rgb);
}

TEST_F(VRDETest, BOUNDS_REPEAT)
{
    struct in_cmd cmd = {0};
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_bounds)
            + sizeof(uint32_t) + sizeof(struct vrde_solid_rect)
            + sizeof(uint32_t) + sizeof(struct vrde_repeat);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_bounds *v_bounds = (struct vrde_bounds *)(data + sizeof(*hdr) + sizeof(*order));
    uint32_t *sub_order = (uint32_t *)((uint8_t *)v_bounds + sizeof(*v_bounds));
    struct vrde_solid_rect *v_sub_rect = (struct vrde_solid_rect *)((uint8_t *)sub_order + sizeof(*sub_order));
    uint32_t *repeat_order = (uint32_t *)((uint8_t *)v_sub_rect + sizeof(*v_sub_rect));
    struct vrde_repeat *v_repeat = (struct vrde_repeat *)((uint8_t *)repeat_order + sizeof(*repeat_order));
    struct svp_clip_draw *s_clip_draw;
    struct svp_solid_rect *s_rect;

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_BOUNDS;
    v_bounds->pt1 = {50, 100};
    v_bounds->pt2 = {80, 150};
    *sub_order = VRDE_SOLID_RECT;
    *v_sub_rect = {50, 100, 50, 100, 0xff0000ff};
    *repeat_order = VRDE_REPEAT;
    v_repeat->bounds.pt1 = {80, 150};
    v_repeat->bounds.pt2 = {100, 200};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(1, inq_size(inq));
    inq_pop(inq, &cmd);

    ASSERT_EQ(SC_CLIP_DRAW, cmd.type);
    s_clip_draw = (struct svp_clip_draw *)cmd.data;
    ASSERT_EQ(sizeof(*s_clip_draw), cmd.size);

    ASSERT_EQ(2, s_clip_draw->rect_count);
    ASSERT_EQ(*v_bounds, s_clip_draw->rect[0]);
    ASSERT_EQ(v_repeat->bounds, s_clip_draw->rect[1]);

    ASSERT_EQ(1, s_clip_draw->cmd_count);
    ASSERT_EQ(SC_SOLID_RECT, s_clip_draw->cmd[0].type);
    ASSERT_EQ(sizeof(*s_rect), s_clip_draw->cmd[0].size);
    s_rect = (struct svp_solid_rect *)s_clip_draw->cmd[0].data;
    ASSERT_EQ(*v_sub_rect, *s_rect);
}

TEST_F(VRDETest, REPEAT_No_BOUNDS_Error)
{
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_repeat);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_repeat *v_repeat = (struct vrde_repeat *)(data + sizeof(*hdr) + sizeof(*order));

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = VRDE_REPEAT;
    v_repeat->bounds.pt1 = {50, 100};
    v_repeat->bounds.pt2 = {80, 150};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_NE(0, vrde_process(&frameInfo, data, size, inq));
    ASSERT_EQ(0, inq_size(inq));
}

TEST_F(VRDETest, MultipleCommand)
{
    struct in_cmd cmd[5];
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t) + sizeof(struct vrde_solid_rect)
             + sizeof(uint32_t) + sizeof(struct vrde_solid_rect);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order1 = (uint32_t *)(data + sizeof(*hdr));
    struct vrde_solid_rect *v_rect1 = (struct vrde_solid_rect *)((uint8_t *)order1 + sizeof(*order1));
    uint32_t *order2 = (uint32_t *)((uint8_t *)v_rect1 + sizeof(*v_rect1));
    struct vrde_solid_rect *v_rect2 = (struct vrde_solid_rect *)((uint8_t *)order2 + sizeof(*order2));

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order1 = VRDE_SOLID_RECT;
    *v_rect1 = {10, 20, 2, 2, 0xff0000ff};
    *order2 = VRDE_SOLID_RECT;
    *v_rect2 = {15, 25, 2, 2, 0xffff0000};

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(0, vrde_process(&frameInfo, data, size, inq));

    ASSERT_EQ(2, inq_popn(inq, cmd, 5));

    ASSERT_EQ(SC_SOLID_RECT, cmd[0].type);
    ASSERT_EQ(sizeof(struct svp_solid_rect), cmd[0].size);
    ASSERT_EQ(*v_rect1, *(struct svp_solid_rect *)cmd[0].data);

    ASSERT_EQ(SC_SOLID_RECT, cmd[1].type);
    ASSERT_EQ(sizeof(struct svp_solid_rect), cmd[1].size);
    ASSERT_EQ(*v_rect2, *(struct svp_solid_rect *)cmd[1].data);
}

TEST_F(VRDETest, BadCommandType)
{
    int size = sizeof(struct vrde_hdr) + sizeof(uint32_t);
    uint8_t *data = (uint8_t *)malloc(size);
    struct vrde_hdr *hdr = (struct vrde_hdr *)data;
    uint32_t *order = (uint32_t *)(data + sizeof(*hdr));

    ASSERT_TRUE(data);

    *hdr = {10, 20, 100, 200};
    *order = 253;

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(-1, vrde_process(&frameInfo, data, size, inq));
    ASSERT_EQ(0, inq_size(inq));
}

TEST_F(VRDETest, BadHeader)
{
    int size = sizeof(struct vrde_hdr) - 1;
    uint8_t *data = (uint8_t *)malloc(size);

    ASSERT_TRUE(data);

    vrde_set_framebuffer(&frameInfo, 800, 600, VRDE_RGB32, baddr);

    ASSERT_EQ(-1, vrde_process(&frameInfo, data, size, inq));
    ASSERT_EQ(0, inq_size(inq));

    ASSERT_EQ(-1, vrde_process(&frameInfo, data, -123, inq));
    ASSERT_EQ(0, inq_size(inq));
}
