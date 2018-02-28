#include "svp_server.h"
#include "main_channel.h"
#include "display_channel.h"
#include "cursor_channel.h"
#include "input_channel.h"
#include "usb_channel.h"
#include "zlog/zlog.h"
#include <stdlib.h>
#include <string.h>

int svp_server_start(struct svp_server *srv);
void svp_server_stop(struct svp_server *srv);

__attribute__((constructor))
void log_init()
{
    int rc = dzlog_init("/etc/zlog.conf", "svp");
    if (rc) {
        fprintf(stderr, "init logger error, please check /etc/zlog.conf\n");
        exit(-1);
    }
    dzlog_debug("log init ok");
}

struct svp_server *svp_server_create(const char *addr, uint16_t port)
{
    struct svp_server *srv = (struct svp_server *)malloc(sizeof(*srv));
    if (!srv)
        return 0;
    memset(srv, 0, sizeof(*srv));
    if (server_base_init(&srv->base) != 0)
        goto out_error;

    srv->start = svp_server_start;
    srv->stop = svp_server_stop;

    srv->main_channel_create = main_channel_create;
    srv->main_channel_destroy = main_channel_destroy;
    srv->display_channel_create = display_channel_create;
    srv->display_channel_destroy = display_channel_destroy;
    srv->cursor_channel_create = cursor_channel_create;
    srv->cursor_channel_destroy = cursor_channel_destroy;
    srv->input_channel_create = input_channel_create;
    srv->input_channel_destroy = input_channel_destroy;
    srv->usb_channel_create = usb_channel_create;
    srv->usb_channel_destroy = usb_channel_destroy;

    strcpy(srv->addr, addr);
    srv->port = port;

    srv->main_channel = main_channel_create(srv);
    srv->display_channel = display_channel_create(srv);
    srv->cursor_channel = cursor_channel_create(srv);
    srv->input_channel = input_channel_create(srv);
    srv->usb_channel = usb_channel_create(srv);

    return srv;
out_error:
    free(srv);
    return 0;
}

void svp_server_destroy(struct svp_server *srv)
{
    if (!srv)
        return;
    srv->input_channel_destroy(srv, srv->input_channel);
    srv->cursor_channel_destroy(srv, srv->cursor_channel);
    srv->display_channel_destroy(srv, srv->display_channel);
    srv->main_channel_destroy(srv, srv->main_channel);
    srv->usb_channel_destroy(srv, srv->usb_channel);
    server_base_fini(&srv->base);
    free(srv);
}

int svp_server_start(struct svp_server *srv)
{
    return srv->base.start(&srv->base);
}

void svp_server_stop(struct svp_server *srv)
{
    srv->base.stop(&srv->base);
}
