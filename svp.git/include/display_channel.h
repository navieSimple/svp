#ifndef DISPLAY_CHANNEL_H
#define DISPLAY_CHANNEL_H

#include "channel.h"

struct display_worker;
struct display_channel
{
    struct channel_base base;
    // interfaces
    void (*screen_resize)(struct display_channel *, int w, int h);
    void (*graphic_update)(struct display_channel *, struct in_cmd *in);
    void (*graphic_reconfig)(struct display_channel *);
    //callbacks
    void (*video_modeset)(int w, int h, int depth);

    int features;
    struct outqueue *outq;
    struct display_worker *worker;
};

struct svp_server;

struct display_channel *display_channel_create(struct svp_server *srv);

void display_channel_destroy(struct svp_server *srv, struct display_channel *channel);

#endif // DISPLAY_CHANNEL_H
