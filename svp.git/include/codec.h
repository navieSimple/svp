#ifndef CODEC_H
#define CODEC_H

#include "svp.h"
#include "net_packet.h"

struct graphic_option;
struct encoder_base;
struct codec
{
    // interfaces
    int (*init)(struct codec *);
    void (*fini)(struct codec *);
    void (*configure)(struct codec *, struct graphic_option *opt);
    void (*encode)(struct codec *, struct pac_bitmap *bitmap);
    struct encoder_base *(*encoder_for_id)(struct codec *, int id);

    struct encoder_base *best_encoder;
    struct list_head encoder_list;
};

struct codec *codec_create();

void codec_destroy(struct codec *c);

#endif // CODEC_H
