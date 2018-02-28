#include "usb_channel.h"
#include "svp_server.h"
#include "outqueue.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>

void channel_usb_update(struct usb_channel *channel, struct in_cmd *in);
void on_usb_update(struct server_base *srv, int event, void *arg, void *priv);
void usb_channel_client_connect(struct channel_base *channel, struct client *c);
void usb_channel_client_disconnect(struct channel_base *channel, struct client *c);
void usb_channel_recv_cmd(struct client *c, struct out_cmd *cmd);
int usb_process(struct in_cmd *in, struct outqueue *outq);

struct usb_channel *usb_channel_create(struct svp_server *srv)
{
    struct usb_channel *channel = (struct usb_channel *)malloc(sizeof(*channel));
    if (!channel)
        return 0;
    memset(channel, 0, sizeof(*channel));
    if (channel_base_init(&channel->base, &srv->base) != 0)
        goto out_error;
    channel->base.connect = usb_channel_client_connect;
    channel->base.disconnect = usb_channel_client_disconnect;
    channel->usb_update = channel_usb_update;
    channel->outq = outq_create();
    srv->base.register_event(&srv->base, SE_USB_UPDATE, on_usb_update, channel);
    channel->base.start_listen(&channel->base, &srv->base, srv->addr, srv->port + SH_USB);
    return channel;
out_error:
    free(channel);
    return 0;
}

void usb_channel_destroy(struct svp_server *srv, struct usb_channel *channel)
{
    if (!channel)
        return;
    channel->base.stop_listen(&channel->base);
    srv->base.unregister_event(&srv->base, SE_USB_UPDATE);
    outq_destroy(channel->outq);
    channel_base_fini(&channel->base);
    free(channel);
    srv->usb_channel = 0;
}

void channel_usb_update(struct usb_channel *channel, struct in_cmd *in)
{
    struct server_base *srv = channel->base.srv;
    if (usb_process(in, channel->outq) == 0)
        srv->trigger_event(srv, SE_USB_UPDATE, 0);
    in_cmd_cleanup(in);
}

void usb_channel_client_connect(struct channel_base *channel, struct client *c)
{
    dzlog_debug("id %d", c->id);
    if (!c)
        return;
    c->recv_cmd = usb_channel_recv_cmd;
}

void usb_channel_client_disconnect(struct channel_base *channel, struct client *c)
{
    c->recv_cmd = 0;
}

static int process_usb_channel_init(struct client *c, struct out_cmd *in)
{
    struct out_cmd out;
    struct usb_channel *channel = (struct usb_channel *)c->channel;
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
    rep->ready.features = 1;
    c->send_cmd(c, &out);
    return 0;
}

void process_usb_reply(struct client *c, struct out_cmd *cmd)
{
    struct usb_channel *channel = (struct usb_channel *)c->channel;
    uint8_t *data = cmd->data + sizeof(struct pac_hdr);
    int size = cmd->size - sizeof(struct pac_hdr);
    if (size < 0) {
        dzlog_error("usb reply size %d too small", cmd->size);
        return;
    }
    if (channel->usb_input)
        channel->usb_input(c->id, cmd->type, data, size);
}

void usb_channel_recv_cmd(struct client *c, struct out_cmd *cmd)
{
    if (!c || !cmd)
        return;
    switch (cmd->type) {
    case SC_CHANNEL_INIT:
        process_usb_channel_init(c, cmd);
        break;
    case SC_USB_DEVICE_LIST_RESULT:
    case SC_USB_OPEN_STATUS:
    case SC_USB_CLEAR_HALTED_EP_STATUS:
    case SC_USB_URB_DATA:
        process_usb_reply(c, cmd);
        break;
    default:
        dzlog_debug("unknown cmd %d", cmd->type);
    }
}

static struct client *get_usb_client(struct usb_channel *channel, struct out_cmd *cmd)
{
    uint32_t client_id;
    struct client *c = 0;

    if (!channel || !cmd)
        return 0;
    if (cmd->type >= SC_USB_NEGOTIATE && cmd->type <= SC_USB_END
            && cmd->data && cmd->size >= sizeof(struct pac_hdr) + sizeof(client_id)) {
        client_id = *(uint32_t *)(cmd->data + sizeof(struct pac_hdr));
        c = channel->base.find_client(&channel->base, client_id);
    } else {
        dzlog_error("client not found for req type %d data %p size %d", cmd->type, cmd->data, cmd->size);
        c = 0;
    }
    return c;
}

void on_usb_update(struct server_base *srv, int event, void *arg, void *priv)
{
    struct usb_channel *channel = (struct usb_channel *)priv;
    struct out_cmd cmd;
    struct client *c = 0;

    while (outq_pop(channel->outq, &cmd) == 1) {
        c = get_usb_client(channel, &cmd);
        if (!c) {
            dzlog_debug("client not found for req type %d", cmd.type);
            out_cmd_cleanup(&cmd);
            continue;
        }
        if (cmd.type == SC_USB_NEGOTIATE) {
            if (channel->usb_input)
                channel->usb_input(c->id, SC_USB_NEGOTIATE, 0, 0);
            out_cmd_cleanup(&cmd);
        } else {
//            dzlog_debug("sending usb req %d", cmd.type);
            // no need to copy or free cmd, let client manage it
            c->send_cmd(c, &cmd);
        }
    }
}

int usb_process_simple(struct outqueue *outq, struct in_cmd *cmd)
{
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_ids);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct svp_usb_ids *s_ids = (struct svp_usb_ids *)cmd->data;
    struct pac_usb_ids *p_ids = (struct pac_usb_ids *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_ids || !p_ids)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_ids->client_id = s_ids->client_id;
    p_ids->dev_id = s_ids->dev_id;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_set_config(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_set_config *s_set_conf = (struct svp_usb_set_config *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_set_config);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_set_config *p_set_conf = (struct pac_usb_set_config *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_set_conf || !p_set_conf)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_set_conf->client_id = s_set_conf->client_id;
    p_set_conf->dev_id = s_set_conf->dev_id;
    p_set_conf->config = s_set_conf->config;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_claim_iface(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_claim_interface *s_claim_iface = (struct svp_usb_claim_interface *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_claim_interface);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_claim_interface *p_claim_iface = (struct pac_usb_claim_interface *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_claim_iface || !p_claim_iface)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_claim_iface->client_id = s_claim_iface->client_id;
    p_claim_iface->dev_id = s_claim_iface->dev_id;
    p_claim_iface->iface = s_claim_iface->iface;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_relse_iface(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_release_interface *s_relse_iface = (struct svp_usb_release_interface *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_release_interface);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_release_interface *p_relse_iface = (struct pac_usb_release_interface *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_relse_iface || !p_relse_iface)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_relse_iface->client_id = s_relse_iface->client_id;
    p_relse_iface->dev_id = s_relse_iface->dev_id;
    p_relse_iface->iface = s_relse_iface->iface;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_set_iface(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_set_interface *s_set_iface = (struct svp_usb_set_interface *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_set_config);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_set_interface *p_set_iface = (struct pac_usb_set_interface *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_set_iface || !p_set_iface)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_set_iface->client_id = s_set_iface->client_id;
    p_set_iface->dev_id = s_set_iface->dev_id;
    p_set_iface->iface = s_set_iface->iface;
    p_set_iface->setting = s_set_iface->setting;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_queue_urb(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_queue_urb *s_queue_urb = (struct svp_usb_queue_urb *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_queue_urb) + s_queue_urb->datalen;
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_queue_urb *p_queue_urb = (struct pac_usb_queue_urb *)((uint8_t *)hdr + sizeof(*hdr));
    uint8_t *p_urbdata = (uint8_t *)p_queue_urb + sizeof(*p_queue_urb);
    if (!hdr || !s_queue_urb || !p_queue_urb)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_queue_urb->client_id = s_queue_urb->client_id;
    p_queue_urb->dev_id = s_queue_urb->dev_id;
    p_queue_urb->handle = s_queue_urb->handle;
    p_queue_urb->type = s_queue_urb->type;
    p_queue_urb->ep = s_queue_urb->ep;
    p_queue_urb->direction = s_queue_urb->direction;
    p_queue_urb->urblen = s_queue_urb->urblen;
    p_queue_urb->datalen = s_queue_urb->datalen;
    memcpy(p_urbdata, s_queue_urb->data, s_queue_urb->datalen);

//    dzlog_debug("queue urb ep %d urblen %d datalen %d", p_queue_urb->ep, p_queue_urb->urblen, p_queue_urb->datalen);

    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_clr_ep(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_clear_halted_ep *s_clr_ep = (struct svp_usb_clear_halted_ep *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_clear_halted_ep);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_clear_halted_ep *p_clr_ep = (struct pac_usb_clear_halted_ep *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_clr_ep || !p_clr_ep)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_clr_ep->client_id = s_clr_ep->client_id;
    p_clr_ep->dev_id = s_clr_ep->dev_id;
    p_clr_ep->ep = s_clr_ep->ep;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process_cancel_urb(struct outqueue *outq, struct in_cmd *cmd)
{
    struct svp_usb_cancel_urb *s_cancel_urb = (struct svp_usb_cancel_urb *)cmd->data;
    int size = sizeof(struct pac_hdr) + sizeof(struct pac_usb_cancel_urb);
    struct pac_hdr *hdr = (struct pac_hdr *)malloc(size);
    struct pac_usb_cancel_urb *p_cancel_urb = (struct pac_usb_cancel_urb *)((uint8_t *)hdr + sizeof(*hdr));
    if (!hdr || !s_cancel_urb || !p_cancel_urb)
        return -1;
    hdr->c_magic = SVP_MAGIC;
    hdr->c_cmd = cmd->type;
    hdr->c_size = size - sizeof(struct pac_hdr);
    p_cancel_urb->client_id = s_cancel_urb->client_id;
    p_cancel_urb->dev_id = s_cancel_urb->dev_id;
    p_cancel_urb->handle = s_cancel_urb->handle;
    if (outq_push(outq, cmd->type, hdr, size)) {
        free(hdr);
        return -1;
    }
    return 0;
}

int usb_process(struct in_cmd *in, struct outqueue *outq)
{
    switch (in->type) {
    case SC_USB_NEGOTIATE:
    case SC_USB_DEVICE_LIST_QUERY:
    case SC_USB_OPEN:
    case SC_USB_CLOSE:
    case SC_USB_RESET:
    case SC_USB_REAP_URB:
        return usb_process_simple(outq, in);
    case SC_USB_SET_CONFIG:
        return usb_process_set_config(outq, in);
    case SC_USB_CLAIM_INTERFACE:
        return usb_process_claim_iface(outq, in);
    case SC_USB_RELEASE_INTERFACE:
        return usb_process_relse_iface(outq, in);
    case SC_USB_SET_INTERFACE:
        return usb_process_set_iface(outq, in);
    case SC_USB_QUEUE_URB:
        return usb_process_queue_urb(outq, in);
    case SC_USB_CLEAR_HALTED_EP:
        return usb_process_clr_ep(outq, in);
    case SC_USB_CANCEL_URB:
        return usb_process_cancel_urb(outq, in);
    default:
        dzlog_error("not handled cmd %d", in->type);
    }
    return 0;
}
