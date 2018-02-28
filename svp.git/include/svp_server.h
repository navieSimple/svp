#ifndef SVP_SERVER_H
#define SVP_SERVER_H

#include "server.h"

struct main_channel;
struct display_channel;
struct cursor_channel;
struct input_channel;
struct usb_channel;
struct svp_server
{
    struct server_base base;

    int (*start)(struct svp_server *srv);
    void (*stop)(struct svp_server *srv);

    struct main_channel *(*main_channel_create)(struct svp_server *srv);
    void (*main_channel_destroy)(struct svp_server *srv, struct main_channel *channel);
    struct display_channel *(*display_channel_create)(struct svp_server *srv);
    void (*display_channel_destroy)(struct svp_server *srv, struct display_channel *channel);
    struct cursor_channel *(*cursor_channel_create)(struct svp_server *srv);
    void (*cursor_channel_destroy)(struct svp_server *srv, struct cursor_channel *channel);
    struct input_channel *(*input_channel_create)(struct svp_server *srv);
    void (*input_channel_destroy)(struct svp_server *srv, struct input_channel *channel);
    struct usb_channel *(*usb_channel_create)(struct svp_server *srv);
    void (*usb_channel_destroy)(struct svp_server *srv, struct usb_channel *channel);

    struct main_channel *main_channel;
    struct display_channel *display_channel;
    struct cursor_channel *cursor_channel;
    struct input_channel *input_channel;
    struct usb_channel *usb_channel;

    char addr[SVP_ADDRESSSIZE];
    uint16_t port;
    int num_clients;

    struct svp_frontend_operations *fops;
    void *bpriv;
    struct svp_backend_operations *bops;
    /*
    struct svp_channel **channels;
    int refresh;
    */
};

/*
struct svp_server *server_create(const char *addr, uint16_t port,
                                 struct svp_backend_operations *bops, void *bpriv,
                                 struct svp_frontend_operations **pfops);

void server_destroy(struct svp_server *srv);
*/

struct svp_server *svp_server_create(const char *addr, uint16_t port);

void svp_server_destroy(struct svp_server *srv);

#endif // SVP_SERVER_H
