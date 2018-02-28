#ifndef GRAPHIC_H
#define GRAPHIC_H

#include "channel.h"

struct graphic_option
{
    int force_codec; // enum SVP_CODEC, or -1 means auto choose
    int lz4_hc; // whether to choose high compression method
};

void graphic_option_reload(struct graphic_option *opt);

struct inqueue;
struct outqueue;
struct codec;
struct graphic_context
{
    void (*reconfig)(struct graphic_context *);

    struct graphic_option opt;
    struct codec *cdc;
    struct outqueue *outq;
};

struct graphic_context *graphic_context_create();

void graphic_context_destroy(struct graphic_context *gctx);

int graphic_get_option(struct graphic_context *gctx, struct graphic_option *opt);

int graphic_set_option(struct graphic_context *gctx, struct graphic_option *opt);

int graphic_input(struct graphic_context *gctx, struct inqueue *inq);

int graphic_output(struct graphic_context *gctx, struct outqueue *outq);

#endif // GRAPHIC_H
