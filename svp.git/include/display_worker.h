#ifndef DISPLAY_WORKER_H
#define DISPLAY_WORKER_H

#include "server.h"

struct in_cmd;
struct inqueue;
struct outqueue;
struct graphic_context;
struct display_worker
{
    struct server_base base;

    // interfaces
    int (*start)(struct display_worker *);
    void (*stop)(struct display_worker *);
    void (*graphic_update)(struct display_worker *, struct in_cmd *cmd);
    void (*graphic_reconfig)(struct display_worker *);

    // callbacks
    void (*output_update)(struct display_worker *);
    void *output_update_arg;

    struct outqueue *outq;
    struct inqueue *inq;
    struct graphic_context *gctx;
};

struct display_worker *display_worker_create(struct outqueue *outq);

void display_worker_destroy(struct display_worker *w);

#endif // DISPLAY_WORKER_H
