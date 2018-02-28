#include "svp.h"
#include "svp_server.h"
#include "main_channel.h"
#include "cursor_channel.h"
#include "display_channel.h"
#include "input_channel.h"
#include "usb_channel.h"
#include "inqueue.h"

void svp_init(struct svp_frontend_operations *op, const char *addr, uint16_t port);
void svp_fini(struct svp_frontend_operations *op);
void svp_screen_resize(struct svp_frontend_operations *op, int w, int h);
void svp_graphic_update(struct svp_frontend_operations *op, struct in_cmd *cmd);
void svp_cursor_update(struct svp_frontend_operations *op, struct svp_cursor *cursor);
void svp_usb_update(struct svp_frontend_operations *op, struct in_cmd *cmd);

void on_client_logon(int id);
void on_client_logout(int id);
void on_video_modeset(int w, int h, int depth);
void on_keyboard_input(int down, uint32_t scancode);
void on_mouse_input(int x, int y, int buttons);
void on_usb_input(int client_id, int req, const void *data, int size);

struct svp_backend_operations *g_bops = 0;

int svp_get_interface(struct svp_backend_operations *bops, struct svp_frontend_operations *fops)
{
    if (!bops || !fops)
        return -1;
    g_bops = bops;

    fops->init = svp_init;
    fops->fini = svp_fini;
    fops->screen_resize = svp_screen_resize;
    fops->graphic_update = svp_graphic_update;
    fops->cursor_update = svp_cursor_update;
    fops->usb_update = svp_usb_update;
    fops->priv = 0;
    return 0;
}

void svp_init(struct svp_frontend_operations *op, const char *addr, uint16_t port)
{
    struct svp_server *srv = svp_server_create(addr, port);
    op->priv = srv;
    if (srv->main_channel) {
        srv->main_channel->client_logon = on_client_logon;
        srv->main_channel->client_logout = on_client_logout;
    }
    if (srv->display_channel)
        srv->display_channel->video_modeset = on_video_modeset;
    if (srv->input_channel) {
        srv->input_channel->keyboard_input = on_keyboard_input;
        srv->input_channel->mouse_input = on_mouse_input;
    }
    if (srv->usb_channel)
        srv->usb_channel->usb_input = on_usb_input;
    srv->start(srv);
}

void svp_fini(struct svp_frontend_operations *op)
{
    struct svp_server *srv = (struct svp_server *)op->priv;
    srv->stop(srv);
    svp_server_destroy(srv);
    op->priv = 0;
    g_bops = 0;
}

void svp_screen_resize(struct svp_frontend_operations *op, int w, int h)
{
    struct svp_server *srv = (struct svp_server *)op->priv;
    if (!srv || !srv->display_channel)
        return;
    srv->display_channel->screen_resize(srv->display_channel, w, h);
}

void svp_graphic_update(struct svp_frontend_operations *op, struct in_cmd *cmd)
{
    struct svp_server *srv = (struct svp_server *)op->priv;
    if (!srv || !srv->display_channel)
        return;
    srv->display_channel->graphic_update(srv->display_channel, cmd);
}

void svp_cursor_update(struct svp_frontend_operations *op, struct svp_cursor *cursor)
{
    struct svp_server *srv = (struct svp_server *)op->priv;
    if (!srv || !srv->cursor_channel)
        return;
    srv->cursor_channel->cursor_update(srv->cursor_channel, cursor);
}

void on_client_logon(int id)
{
    if (g_bops && g_bops->client_logon)
        g_bops->client_logon(g_bops, id);
}

void on_client_logout(int id)
{
    if (g_bops && g_bops->client_logout)
        g_bops->client_logout(g_bops, id);
}

void on_video_modeset(int w, int h, int depth)
{
    if (g_bops && g_bops->video_modeset)
        g_bops->video_modeset(g_bops, w, h, depth);
}

void on_keyboard_input(int down, uint32_t scancode)
{
    if (g_bops && g_bops->keyboard_input)
        g_bops->keyboard_input(g_bops, down, scancode);
}

void on_mouse_input(int x, int y, int buttons)
{
    if (g_bops && g_bops->mouse_input)
        g_bops->mouse_input(g_bops, x, y, buttons);
}

void svp_usb_update(struct svp_frontend_operations *op, struct in_cmd *cmd)
{
    struct svp_server *srv = (struct svp_server *)op->priv;
    if (!srv || !srv->usb_channel)
        return;
    srv->usb_channel->usb_update(srv->usb_channel, cmd);
}

void on_usb_input(int client_id, int req, const void *data, int size)
{
    if (g_bops && g_bops->usb_input)
        g_bops->usb_input(g_bops, client_id, req, data, size);
}
