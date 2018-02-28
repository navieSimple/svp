#include "cursor_channel.h"
#include "svp_server.h"
#include "inqueue.h"
#include "outqueue.h"
#include <stdlib.h>
#include <string.h>

void channel_cursor_update(struct cursor_channel *channel, struct svp_cursor *cursor);
void on_cursor_update(struct server_base *srv, int event, void *arg, void *priv);
void cursor_channel_client_connect(struct channel_base *channel, struct client *c);
void cursor_channel_client_disconnect(struct channel_base *channel, struct client *c);
void cursor_channel_recv_cmd(struct client *c, struct out_cmd *cmd);

struct cursor_channel *cursor_channel_create(struct svp_server *srv)
{
    struct cursor_channel *channel = (struct cursor_channel *)malloc(sizeof(*channel));
    if (!channel)
        return 0;
    memset(channel, 0, sizeof(*channel));
    if (channel_base_init(&channel->base, &srv->base) != 0)
        goto out_error;
    channel->cursor_update = channel_cursor_update;
    pthread_spin_init(&channel->lock, PTHREAD_PROCESS_PRIVATE);
    srv->base.register_event(&srv->base, SE_CURSOR_UPDATE, on_cursor_update, channel);
    channel->base.connect = cursor_channel_client_connect;
    channel->base.disconnect = cursor_channel_client_disconnect;
    channel->base.start_listen(&channel->base, &srv->base, srv->addr, srv->port + SH_CURSOR);
    return channel;
out_error:
    free(channel);
    return 0;
}

void cursor_channel_destroy(struct svp_server *srv, struct cursor_channel *channel)
{
    if (!channel)
        return;
    srv->base.unregister_event(&srv->base, SE_CURSOR_UPDATE);
    channel->base.stop_listen(&channel->base);
    channel_base_fini(&channel->base);
    free(channel);
    srv->cursor_channel = 0;
}

void channel_cursor_update(struct cursor_channel *channel, struct svp_cursor *cursor)
{
    struct pac_hdr *hdr;
    struct pac_cursor *cur;
    uint8_t *data;
    uint8_t *mask;
    uint8_t *pixmap;

    data = (uint8_t *)malloc(sizeof(*hdr) + sizeof(*cur) + cursor->mask.size + cursor->pixmap.size);
    hdr = (struct pac_hdr *)data;
    cur = (struct pac_cursor *)(data + sizeof(*hdr));
    mask = data + sizeof(*hdr) + sizeof(*cur);
    pixmap = mask + cursor->mask.size;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = SC_CURSOR;
    hdr->c_size = sizeof(*cur) + cursor->mask.size + cursor->pixmap.size;
    cur->x_hot = cursor->x_hot;
    cur->y_hot = cursor->y_hot;
    cur->w = cursor->w;
    cur->h = cursor->h;
    cur->mask_len = cursor->mask.size;
    cur->data_len = cursor->pixmap.size;
    memcpy(mask, cursor->mask.bits, cursor->mask.size);
    memcpy(pixmap, cursor->pixmap.bits, cursor->pixmap.size);

    // TODO: refactor it
    free(cursor->mask.bits);
    free(cursor->pixmap.bits);
    free(cursor);

    pthread_spin_lock(&channel->lock);
    if (channel->out.data) {
        dzlog_debug("cursor shape is changing rapidly");
        free(channel->out.data);
    }
    channel->out.type = SC_CURSOR;
    channel->out.data = (uint8_t *)hdr;
    channel->out.size = sizeof(*hdr) + hdr->c_size;
    pthread_spin_unlock(&channel->lock);

    channel->base.srv->trigger_event(channel->base.srv, SE_CURSOR_UPDATE, 0);
}

void on_cursor_update(struct server_base *srv, int event, void *arg, void *priv)
{
    struct cursor_channel *channel = (struct cursor_channel *)priv;
    struct client *c;
    struct out_cmd out;
    pthread_spin_lock(&channel->lock);
    out.type = channel->out.type;
    out.data = channel->out.data;
    out.size = channel->out.size;
    channel->out.type = SC_NOP;
    channel->out.data = 0;
    channel->out.size = 0;
    pthread_spin_unlock(&channel->lock);
    list_for_each_entry(c, &channel->base.client_list, link) {
        c->send_cmd(c, &out);
    }
}

void cursor_channel_client_connect(struct channel_base *channel, struct client *c)
{
    if (!c)
        return;
    c->recv_cmd = cursor_channel_recv_cmd;
}

void cursor_channel_client_disconnect(struct channel_base *channel, struct client *c)
{
    c->recv_cmd = 0;
}

static int process_cursor_channel_init(struct client *c, struct out_cmd *in)
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

void cursor_channel_recv_cmd(struct client *c, struct out_cmd *cmd)
{
    if (!c || !cmd)
        return;
    switch (cmd->type) {
    case SC_CHANNEL_INIT:
        process_cursor_channel_init(c, cmd);
        break;
    default:
        dzlog_debug("unknown cmd %d", cmd->type);
    }
}
