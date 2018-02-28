#include "display_channel.h"
#include "svp_server.h"
#include "display_worker.h"
#include "inqueue.h"
#include "outqueue.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>

void channel_screen_resize(struct display_channel *channel, int w, int h);
void channel_graphic_update(struct display_channel *channel, struct in_cmd *in);
void on_output_update(struct server_base *srv, int event, void *arg, void *priv);
void display_channel_client_connect(struct channel_base *channel, struct client *c);
void display_channel_client_disconnect(struct channel_base *channel, struct client *c);
void display_channel_recv_cmd(struct client *c, struct out_cmd *cmd);
void channel_graphic_reconfig(struct display_channel *channel);
void on_worker_output_update(struct display_worker *worker);

struct display_channel *display_channel_create(struct svp_server *srv)
{
    struct display_channel *channel = (struct display_channel *)malloc(sizeof(*channel));
    if (!channel)
        return 0;
    memset(channel, 0, sizeof(*channel));
    if (channel_base_init(&channel->base, &srv->base) != 0)
        goto out_error;
    channel->base.connect = display_channel_client_connect;
    channel->base.disconnect = display_channel_client_disconnect;
    channel->screen_resize = channel_screen_resize;
    channel->graphic_update = channel_graphic_update;
    channel->graphic_reconfig = channel_graphic_reconfig;
    channel->outq = outq_create();
    channel->worker = display_worker_create(channel->outq);
    channel->worker->output_update = on_worker_output_update;
    channel->worker->output_update_arg = channel;
    srv->base.register_event(&srv->base, SE_OUTPUT_UPDATE, on_output_update, channel);
    channel->worker->start(channel->worker);
    channel->base.start_listen(&channel->base, &srv->base, srv->addr, srv->port + SH_DISPLAY);
    return channel;
out_error:
    free(channel);
    return 0;
}

void display_channel_destroy(struct svp_server *srv, struct display_channel *channel)
{
    if (!channel)
        return;
    channel->base.stop_listen(&channel->base);
    channel->worker->stop(channel->worker);
    srv->base.unregister_event(&srv->base, SE_OUTPUT_UPDATE);
    display_worker_destroy(channel->worker);
    outq_destroy(channel->outq);
    channel_base_fini(&channel->base);
    free(channel);
    srv->display_channel = 0;
}

void channel_graphic_update(struct display_channel *channel, struct in_cmd *in)
{
    channel->worker->graphic_update(channel->worker, in);
}

void display_channel_client_connect(struct channel_base *channel, struct client *c)
{
    if (!c)
        return;
    c->recv_cmd = display_channel_recv_cmd;
}

void display_channel_client_disconnect(struct channel_base *channel, struct client *c)
{
    c->recv_cmd = 0;
}

static int process_display_channel_init(struct client *c, struct out_cmd *in)
{
    struct out_cmd out;
    struct display_channel *channel = (struct display_channel *)c->channel;
    struct
    {
        struct pac_hdr hdr;
        struct pac_channel_init init;
    } __attribute__((packed)) *req;
    struct
    {
        struct pac_hdr hdr;
        struct pac_channel_ready ready;
    } __attribute__((packed)) *rep;
    req = (typeof(req))in->data;
    out.type = SC_CHANNEL_READY;
    out.data = (uint8_t *)malloc(sizeof(*rep));
    if (!out.data)
        return -1;
    out.size = sizeof(*rep);
    rep = (typeof(rep))out.data;
    rep->hdr.c_magic = SVP_MAGIC;
    rep->hdr.c_cmd = SC_CHANNEL_READY;
    rep->hdr.c_size = sizeof(rep->ready);
    channel->features = req->init.features;
    rep->ready.features = channel->features;
    c->send_cmd(c, &out);
    return 0;
}

int process_video_modeset(struct client *c, struct out_cmd *in)
{
    struct display_channel *channel = (struct display_channel *)c->channel;
    struct
    {
        struct pac_hdr hdr;
        struct pac_video_modeset mode;
    } __attribute__((packed)) *req;
    req = (typeof(req))in->data;
    if (in->size != sizeof(*req)) {
        dzlog_debug("bad SC_VIDEO_MODESET size, expect %d got %d", sizeof(*req), in->size);
        return -1;
    }
    if (channel->video_modeset)
        channel->video_modeset(req->mode.w, req->mode.h, req->mode.depth);
    return 0;
}

int process_graphic_config(struct client *c, struct out_cmd *in)
{
    struct display_channel *channel = (struct display_channel *)c->channel;
    struct
    {
        struct pac_hdr hdr;
        struct pac_graphic_config conf;
        char data[];
    } __attribute__((packed)) *req;
    req = (typeof(req))in->data;
    if (in->size < sizeof(*req) + sizeof(req->conf.size)) {
        dzlog_debug("bad SC_GRAPHIC_CONFIG size, expect at least %d got %d",
                sizeof(*req) + sizeof(req->conf.size), in->size);
        return -1;
    }
    settings_update(GS, req->data, req->conf.size);
    channel->graphic_reconfig(channel);
    return 0;
}

void display_channel_recv_cmd(struct client *c, struct out_cmd *cmd)
{
    if (!c || !cmd)
        return;
    switch (cmd->type) {
    case SC_CHANNEL_INIT:
        process_display_channel_init(c, cmd);
        break;
    case SC_VIDEO_MODESET:
        process_video_modeset(c, cmd);
        break;
    case SC_GRAPHIC_CONFIG:
        process_graphic_config(c, cmd);
        break;
    default:
        dzlog_debug("unknown cmd %d", cmd->type);
    }
}

void on_output_update(struct server_base *srv, int event, void *arg, void *priv)
{
    struct display_channel *channel = (struct display_channel *)priv;
    struct out_cmd cmd, copy;
    struct client *c;
    while (outq_pop(channel->outq, &cmd) == 1) {
        list_for_each_entry(c, &channel->base.client_list, link) {
            out_cmd_copy(&copy, &cmd);
            c->send_cmd(c, &copy);
        }
        out_cmd_cleanup(&cmd);
    }
}

void channel_screen_resize(struct display_channel *channel, int w, int h)
{
    struct client *c;
    struct
    {
        struct pac_hdr hdr;
        struct pac_screen_resize resize;
    } __attribute__((packed)) *p = (typeof(p))malloc(sizeof(*p));
    p->hdr.c_magic = SVP_MAGIC;
    p->hdr.c_cmd = SC_SCREEN_RESIZE;
    p->hdr.c_size = sizeof(p->resize);
    p->resize.w = w;
    p->resize.h = h;
    struct out_cmd cmd = {SC_SCREEN_RESIZE, (uint8_t *)p, sizeof(*p)};
    list_for_each_entry(c, &channel->base.client_list, link) {
        c->send_cmd(c, &cmd);
    }
}

void channel_graphic_reconfig(struct display_channel *channel)
{
    if (!channel->worker)
        return;
    channel->worker->graphic_reconfig(channel->worker);
}

void on_worker_output_update(struct display_worker *worker)
{
    struct display_channel *channel = (struct display_channel *)worker->output_update_arg;
    if (!channel || !channel->base.srv)
        return;
    struct server_base *srv = (struct server_base *)channel->base.srv;
    srv->trigger_event(srv, SE_OUTPUT_UPDATE, 0);
}
