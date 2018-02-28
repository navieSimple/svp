#include "input_channel.h"
#include "svp_server.h"
#include "inqueue.h"
#include "outqueue.h"
#include <stdlib.h>
#include <string.h>

void input_channel_client_connect(struct channel_base *channel, struct client *c);

void input_channel_client_disconnect(struct channel_base *channel, struct client *c);

void input_channel_recv_cmd(struct client *c, struct out_cmd *cmd);

struct input_channel *input_channel_create(struct svp_server *srv)
{
    struct input_channel *channel = (struct input_channel *)malloc(sizeof(*channel));
    if (!channel)
        return 0;
    memset(channel, 0, sizeof(*channel));
    if (channel_base_init(&channel->base, &srv->base) != 0)
        goto out_error;
    channel->base.connect = input_channel_client_connect;
    channel->base.disconnect = input_channel_client_disconnect;
    channel->base.start_listen(&channel->base, &srv->base, srv->addr, srv->port + SH_INPUT);
    return channel;
out_error:
    free(channel);
    return 0;
}

void input_channel_destroy(struct svp_server *srv, struct input_channel *channel)
{
    if (!channel)
        return;
    channel->base.stop_listen(&channel->base);
    channel_base_fini(&channel->base);
    free(channel);
    srv->input_channel = 0;
}

void input_channel_client_connect(struct channel_base *channel, struct client *c)
{
    if (!c)
        return;
    c->recv_cmd = input_channel_recv_cmd;
}

void input_channel_client_disconnect(struct channel_base *channel, struct client *c)
{
    c->recv_cmd = 0;
}

static int process_input_channel_init(struct client *c, struct out_cmd *in)
{
    struct out_cmd out;
    struct svp_server *srv = (struct svp_server *)c->channel->srv;
    struct
    {
        struct pac_hdr hdr;
        struct pac_channel_ready ready;
    } __attribute__((packed)) *rep;
    out.type = SC_CHANNEL_READY;
    out.data = (uint8_t *)malloc(sizeof(*rep));
    if (!out.data)
        return -1;
    out.size = sizeof(*rep);
    rep = (typeof(rep))out.data;
    rep->hdr.c_magic = SVP_MAGIC;
    rep->hdr.c_cmd = SC_CHANNEL_READY;
    rep->hdr.c_size = sizeof(rep->ready);
    rep->ready.features = 1;
    c->send_cmd(c, &out);
    return 0;
}

int process_keyboard_event(struct client *c, struct out_cmd *in)
{
    struct input_channel *channel = (struct input_channel *)c->channel;
    struct
    {
        struct pac_hdr hdr;
        struct pac_keyboard_event event;
    } __attribute__((packed)) *req;
    req = (typeof(req))in->data;
    if (in->size != sizeof(*req)) {
        dzlog_debug("bad SC_KEYBOARD_EVENT size, expect %d got %d", sizeof(*req), in->size);
        return -1;
    }
    if (channel->keyboard_input)
        channel->keyboard_input(req->event.down, req->event.scancode);
    return 0;
}

int process_mouse_event(struct client *c, struct out_cmd *in)
{
    struct input_channel *channel = (struct input_channel *)c->channel;
    struct
    {
        struct pac_hdr hdr;
        struct pac_mouse_event event;
    } __attribute__((packed)) *req;
    req = (typeof(req))in->data;
    if (in->size != sizeof(*req)) {
        dzlog_debug("bad SC_MOUSE_EVENT size, expect %d got %d", sizeof(*req), in->size);
        return -1;
    }
    if (channel->mouse_input)
        channel->mouse_input(req->event.x, req->event.y, req->event.buttons);
    return 0;
}

void input_channel_recv_cmd(struct client *c, struct out_cmd *cmd)
{
    if (!c || !cmd)
        return;
    switch (cmd->type) {
    case SC_CHANNEL_INIT:
        process_input_channel_init(c, cmd);
        break;
    case SC_KEYBOARD_EVENT:
        process_keyboard_event(c, cmd);
        break;
    case SC_MOUSE_EVENT:
        process_mouse_event(c, cmd);
        break;
    default:
        dzlog_debug("unknown cmd %d", cmd->type);
    }
}
