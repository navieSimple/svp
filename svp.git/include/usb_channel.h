#ifndef USB_CHANNEL_H
#define USB_CHANNEL_H

#include "channel.h"

struct usb_channel
{
    struct channel_base base;
    // interfaces
    void (*usb_update)(struct usb_channel *, struct in_cmd *in);
    //callbacks
    void (*usb_input)(int client_id, int req, const void *data, int size);

//    struct inqueue *inq;
    struct outqueue *outq;
};

struct svp_server;

struct usb_channel *usb_channel_create(struct svp_server *srv);

void usb_channel_destroy(struct svp_server *srv, struct usb_channel *channel);

#endif // USB_CHANNEL_H
