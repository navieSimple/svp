#include "svp.h"
#include "server.h"
#include "channel.h"
#include "inqueue.h"
#include "outqueue.h"
#include <stdlib.h>
#include <string.h>

static uint32_t g_cid = 0;

int channel_start_listen(struct channel_base *channel, struct server_base *srv, const char *addr, uint16_t port);

void channel_stop_listen(struct channel_base *channel);

int client_send_cmd(struct client *c, struct out_cmd *cmd);

struct client *channel_create_client(struct channel_base *channel);

void channel_destroy_client(struct channel_base *channel, struct client *client);

struct client *channel_find_client(struct channel_base *channel, int id);

void channel_client_error(struct channel_base *channel, struct client *client);

static void on_client_cmd(struct fd_context *ctx, int status, void *priv);

static void on_cmd_write_done(struct fd_context *ctx, int status, void *priv);

int channel_base_init(struct channel_base *channel, struct server_base *srv)
{
    memset(channel, 0, sizeof(*channel));
    channel->srv = srv;
    channel->start_listen = channel_start_listen;
    channel->stop_listen = channel_stop_listen;
    channel->create_client = channel_create_client;
    channel->destroy_client = channel_destroy_client;
    channel->find_client = channel_find_client;
    INIT_LIST_HEAD(&channel->client_list);
    return 0;
}

void channel_base_fini(struct channel_base *channel)
{
    return;
}

void channel_client_error(struct channel_base *channel, struct client *c)
{
    if (channel->disconnect)
        channel->disconnect(channel, c);
}

static void on_client_hdr(struct fd_context *ctx, int status, void *priv)
{
    struct client *c = (struct client *)priv;
    struct pac_hdr *hdr;
    if (!c || !c->channel || c->fd != ctx)
        return;
    if (status != 0) {
        channel_client_error(c->channel, c);
    } else {
        hdr = (struct pac_hdr *)c->recvbuf;
        if (hdr->c_magic == SVP_MAGIC && hdr->c_size <= SVP_RECV_BUFSIZE) {
            client_ref(c);
            c->fd->async_read(c->fd, c->recvbuf + sizeof(struct pac_hdr), hdr->c_size, on_client_cmd, c);
        } else {
            dzlog_debug("bad packet");
            channel_client_error(c->channel, c);
        }
    }
    client_deref(c);
}

static void on_client_cmd(struct fd_context *ctx, int status, void *priv)
{
    struct client *c = (struct client *)priv;
    struct pac_hdr *hdr;
    struct out_cmd cmd;
    if (!c || !c->channel || c->fd != ctx)
        return;
    hdr = (struct pac_hdr *)c->recvbuf;
    if (status != 0) {
        channel_client_error(c->channel, c);
    } else {
        if (c->recv_cmd) {
            cmd.type = hdr->c_cmd;
            cmd.size = sizeof(*hdr) + hdr->c_size;
            cmd.data = c->recvbuf;
            c->recv_cmd(c, &cmd);
        }
        client_ref(c);
        c->fd->async_read(c->fd, c->recvbuf, sizeof(struct pac_hdr), on_client_hdr, c);
    }
    client_deref(c);
}

static void on_new_connection(struct server_base *srv, struct fd_context *listenfd, int status, void *priv)

{
    struct channel_base *channel = (struct channel_base *)priv;
    struct client *c;
    if (status != 0)
        return;
    c = channel->create_client(channel);
    if (!c)
        return;
    c->fd = srv->tcp_accept(srv, listenfd);
    if (!c->fd) {
        channel->destroy_client(channel, c);
        return;
    }
    c->fd->set_nodelay(c->fd);
    if (channel->connect)
        channel->connect(channel, c);
    client_ref(c);
    c->fd->async_read(c->fd, c->recvbuf, sizeof(struct pac_hdr), on_client_hdr, c);
}

int channel_start_listen(struct channel_base *channel, struct server_base *srv, const char *addr, uint16_t port)
{
    channel->srv = srv;
    channel->listenfd = srv->tcp_listen(srv, addr, port, on_new_connection, channel);
    return 0;
}

void channel_stop_listen(struct channel_base *channel)
{
    if (!channel->srv || !channel->listenfd)
        return;
    channel->srv->del_fd(channel->srv, channel->listenfd);
    channel->srv->fd_destroy(channel->srv, channel->listenfd);
    channel->listenfd = 0;
    return;
}

int client_send_cmd(struct client *c, struct out_cmd *cmd)
{
    int rc;
    rc = outq_push(c->outq, cmd->type, cmd->data, cmd->size);
    if (rc)
        return -1;
    c->outq_bytes += cmd->size;
    if (!c->fd->write_info.active && outq_size(c->outq) == 1) {
        client_ref(c);
        c->fd->async_write(c->fd, cmd->data, cmd->size, on_cmd_write_done, c);
    }
    return 0;
}

static void on_cmd_write_done(struct fd_context *ctx, int status, void *priv)
{
    struct client *c = (struct client *)priv;
    struct out_cmd cmd;
    int n;
    if (!c)
        return;
    if (status) {
        dzlog_debug("send error %d", status);
        goto out_error;
    }
    n = outq_pop(c->outq, &cmd);
    if (n != 1) {
        dzlog_debug("error out queue empty");
        goto out_error;
    }
    c->outq_bytes -= cmd.size;
    out_cmd_cleanup(&cmd);
    if (outq_peek(c->outq, &cmd) == 0) {
        client_ref(c);
        c->fd->async_write(c->fd, cmd.data, cmd.size, on_cmd_write_done, c);
    }
    client_deref(c);
    return;
out_error:
    channel_client_error(c->channel, c);
    client_deref(c);
}

struct client *channel_create_client(struct channel_base *channel)
{
    struct client *c;
    if (!channel)
        return 0;
    c = (struct client *)malloc(sizeof(*c));
    if (!c)
        return 0;
    memset(c, 0, sizeof(*c));
    c->channel = channel;
    c->fd = 0;
    c->ref = 0;
    c->id = 0;

    c->outq = outq_create();
    c->send_cmd = client_send_cmd;

    list_add_tail(&c->link, &channel->client_list);

    return c;
}

void channel_destroy_client(struct channel_base *channel, struct client *client)
{
    if (!channel || !channel->srv || !client)
        return;
    list_del(&client->link);
    if (client->fd)
        channel->srv->fd_destroy(channel->srv, client->fd);
    free(client);
}

struct client *channel_find_client(struct channel_base *channel, int id)
{
    struct client *c;
    list_for_each_entry(c, &channel->client_list, link) {
        if (c->id == id)
            return c;
    }
    return 0;
}

void client_ref(struct client *c)
{
    ++c->ref;
}

void client_deref(struct client *c)
{
    --c->ref;
    if (c->ref == 0)
        c->channel->destroy_client(c->channel, c);
}
