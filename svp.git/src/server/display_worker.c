#include "display_worker.h"
#include "svp.h"
#include "server.h"
#include "channel.h"
#include "graphic.h"
#include "list.h"
#include "inqueue.h"
#include "outqueue.h"
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#include <assert.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <errno.h>

void worker_graphic_update(struct display_worker *worker, struct in_cmd *cmd);
int worker_start(struct display_worker *worker);
void worker_stop(struct display_worker *worker);
void on_graphic_update(struct server_base *srv, int event, void *arg, void *priv);
void on_graphic_reconfig(struct server_base *srv, int event, void *arg, void *priv);
void worker_graphic_reconfig(struct display_worker *worker);

struct display_worker *display_worker_create(struct outqueue *outq)
{
    struct display_worker *worker = (struct display_worker *)malloc(sizeof(*worker));
    if (!worker)
        return 0;
    server_base_init(&worker->base);
    worker->start = worker_start;
    worker->stop = worker_stop;
    worker->graphic_update = worker_graphic_update;
    worker->graphic_reconfig = worker_graphic_reconfig;
    worker->inq = inq_create();
    worker->outq = outq;
    worker->gctx = graphic_context_create();
    return worker;
}

void display_worker_destroy(struct display_worker *worker)
{
    if (!worker)
        return;
    graphic_context_destroy(worker->gctx);
    inq_destroy(worker->inq);
    server_base_fini(&worker->base);
}

int worker_start(struct display_worker *worker)
{
    worker->base.register_event(&worker->base, SE_GRAPHIC_UPDATE, on_graphic_update, worker);
    worker->base.register_event(&worker->base, SE_GRAPHIC_RECONFIG, on_graphic_reconfig, worker);
    return worker->base.start(&worker->base);
}

void worker_stop(struct display_worker *worker)
{
    worker->base.unregister_event(&worker->base, SE_GRAPHIC_RECONFIG);
    worker->base.unregister_event(&worker->base, SE_GRAPHIC_UPDATE);
    worker->base.stop(&worker->base);
}

void worker_graphic_update(struct display_worker *worker, struct in_cmd *cmd)
{
    if (!worker || !cmd)
        return;
    inq_push(worker->inq, cmd->type, cmd->data, cmd->size);
    worker->base.trigger_event(&worker->base, SE_GRAPHIC_UPDATE, 0);
}

void on_graphic_update(struct server_base *srv, int event, void *arg, void *priv)
{
    struct display_worker *worker = (struct display_worker *)priv;
    graphic_input(worker->gctx, worker->inq);
    graphic_output(worker->gctx, worker->outq);
    if (outq_size(worker->outq) > 0) {
        if (worker->output_update)
            worker->output_update(worker);
    }
}

void on_graphic_reconfig(struct server_base *srv, int event, void *arg, void *priv)
{
    struct display_worker *worker = (struct display_worker *)priv;
    if (!worker || !worker->gctx)
        return;
    worker->gctx->reconfig(worker->gctx);
}

void worker_graphic_reconfig(struct display_worker *worker)
{
    worker->base.trigger_event(&worker->base, SE_GRAPHIC_RECONFIG, 0);
}
