#ifndef CURSOR_CHANNEL_H
#define CURSOR_CHANNEL_H

#include "channel.h"
#include "outqueue.h"
#include <pthread.h>

struct cursor_channel
{
    struct channel_base base;
    void (*cursor_update)(struct cursor_channel *channel, struct svp_cursor *cursor);

    pthread_spinlock_t lock;
    struct out_cmd out;
};

struct svp_server;

struct cursor_channel *cursor_channel_create(struct svp_server *srv);

void cursor_channel_destroy(struct svp_server *srv, struct cursor_channel *channel);

#endif // CURSOR_CHANNEL_H
