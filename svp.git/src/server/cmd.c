#include "cmd.h"
#include "svp.h"
#include <stdlib.h>
#include <string.h>

void bitmap_cleanup(struct svp_bitmap *bitmap)
{
    if (bitmap)
        free(bitmap->bits);
}

void text_cleanup(struct svp_text *text)
{
    struct svp_glyph *glyph, *tmp;
    if (!text)
        return;
    list_for_each_entry_safe(glyph, tmp, &text->glyph_list, link) {
        bitmap_cleanup(&glyph->bitmap);
        free(glyph);
    }
    text->glyph_count = 0;
    INIT_LIST_HEAD(&text->glyph_list);
}

void clip_draw_cleanup(struct svp_clip_draw *clip)
{
    struct in_cmd cmd;
    int i;
    if (!clip)
        return;
    for (i = 0; i < clip->cmd_count; i++) {
        cmd.type = clip->cmd[i].type;
        cmd.data = clip->cmd[i].data;
        cmd.size = clip->cmd[i].size;
        in_cmd_cleanup(&cmd);
    }
    clip->cmd_count = 0;
}

void generic_cleanup(int type, void *data, int size)
{
    switch (type) {
    case SC_CACHED_BITMAP:
        if (data)
            bitmap_cleanup(&((struct svp_cached_bitmap *)data)->bitmap);
        break;
    case SC_RESTORE_SCREEN:
        if (data)
            bitmap_cleanup(&((struct svp_restore_screen *)data)->bitmap);
        break;
    case SC_TEXT:
        text_cleanup((struct svp_text *)data);
        break;
    case SC_CLIP_DRAW:
        clip_draw_cleanup((struct svp_clip_draw *)data);
        break;
    }
}

void in_cmd_cleanup(struct in_cmd *cmd)
{
    if (!cmd)
        return;
    generic_cleanup(cmd->type, cmd->data, cmd->size);
    free(cmd->data);
}

void out_cmd_cleanup(struct out_cmd *cmd)
{
    if (!cmd)
        return;
    free(cmd->data);
    cmd->data = 0;
}

void out_cmd_copy(struct out_cmd *dst, struct out_cmd *src)
{
    if (!dst || !src)
        return;
    dst->type = src->type;
    dst->size = src->size;
    dst->data = (uint8_t *)malloc(src->size);
    if (dst->data)
        memcpy(dst->data, src->data, src->size);
}
