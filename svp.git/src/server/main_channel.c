#include "main_channel.h"
#include "svp_server.h"
#include "outqueue.h"
#include <stdlib.h>
#include <string.h>

void main_channel_client_connect(struct channel_base *channel, struct client *c);

void main_channel_client_disconnect(struct channel_base *channel, struct client *c);

void main_channel_recv_cmd(struct client *c, struct out_cmd *cmd);

struct main_channel *main_channel_create(struct svp_server *srv)
{
    struct main_channel *channel = (struct main_channel *)malloc(sizeof(*channel));
    if (!channel)
        return 0;
    memset(channel, 0, sizeof(*channel));
    if (channel_base_init(&channel->base, &srv->base) != 0)
        goto out_error;
    channel->base.connect = main_channel_client_connect;
    channel->base.disconnect = main_channel_client_disconnect;
    channel->base.start_listen(&channel->base, &srv->base, srv->addr, srv->port + SH_MAIN);
    return channel;
out_error:
    free(channel);
    return 0;
}

void main_channel_destroy(struct svp_server *srv, struct main_channel *channel)
{
    if (!channel)
        return;
    channel->base.stop_listen(&channel->base);

    channel_base_fini(&channel->base);
    free(channel);
    srv->main_channel = 0;
}

void main_channel_client_connect(struct channel_base *channel, struct client *c)
{
    struct main_channel *mc = (struct main_channel *)channel;
    dzlog_debug("");
    if (!c)
        return;
    c->recv_cmd = main_channel_recv_cmd;
    if (mc->client_logon)
        mc->client_logon(c->id);
}

void main_channel_client_disconnect(struct channel_base *channel, struct client *c)
{
    struct main_channel *mc = (struct main_channel *)channel;
    dzlog_debug("");
    c->recv_cmd = 0;
    if (mc->client_logout)
        mc->client_logout(c->id);
}

int process_main_channel_init(struct client *c, struct out_cmd *in)
{
    struct out_cmd out;
    struct svp_server *srv = (struct svp_server *)c->channel->srv;
    struct
    {
        struct pac_hdr hdr;
        struct pac_channel_list list;
    } __attribute__((packed)) *rep;
    out.type = SC_CHANNEL_LIST;
    out.data = (uint8_t *)malloc(sizeof(*rep));
    if (!out.data)
        return -1;
    out.size = sizeof(*rep);
    rep = (typeof(rep))out.data;
    rep->hdr.c_magic = SVP_MAGIC;
    rep->hdr.c_cmd = SC_CHANNEL_LIST;
    rep->hdr.c_size = sizeof(rep->list);
    rep->list.server_version = 1;
    rep->list.count = 5;
    rep->list.channel[0].type = SH_MAIN;
    rep->list.channel[0].port = srv->port;
    rep->list.channel[1].type = SH_DISPLAY;
    rep->list.channel[1].port = srv->port + 1;
    rep->list.channel[2].type = SH_CURSOR;
    rep->list.channel[2].port = srv->port + 2;
    rep->list.channel[3].type = SH_INPUT;
    rep->list.channel[3].port = srv->port + 3;
    rep->list.channel[4].type = SH_USB;
    rep->list.channel[4].port = srv->port + 4;
    c->send_cmd(c, &out);
    return 0;
}

void main_channel_recv_cmd(struct client *c, struct out_cmd *cmd)
{
    if (!c || !cmd)
        return;
    switch (cmd->type) {
    case SC_MAIN_CHANNEL_INIT:
        process_main_channel_init(c, cmd);
        break;
    default:
        dzlog_debug("unknown cmd %d", cmd->type);
    }
}
