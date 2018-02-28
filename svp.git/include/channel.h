#ifndef CHANNEL_H
#define CHANNEL_H

#include "svp.h"
#include "server.h"
#include "list.h"
#include <pthread.h>

struct in_cmd;
struct inqueue;
struct out_cmd;
struct outqueue;
/*
struct svp_channel;

struct outqueue_control
{
    struct svp_channel *channel;
    struct async_write_info *async;
    struct outqueue *outq;
    struct out_cmd sending_cmd;
};

struct svp_channel
{
    struct fd_context *listenfd;
    struct fd_context *clientfd;

    connect_function clientfd_connect;
    fd_event_function clientfd_event;
    disconnect_function clientfd_disconnect;

    struct async_read_info read_info;
    struct async_write_info write_info;
    void *priv;
};

struct main_info
{
    struct pac_hdr hdr;
    struct pac_main_channel_init init;
    struct pac_channel_list channel_list;
};

struct display_worker;
struct display_info
{
    struct pac_hdr hdr;
    struct pac_channel_init init;
    struct pac_channel_ready ready;

    pthread_spinlock_t fblock;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint8_t *fb;

    struct display_worker *worker;
    struct fd_context *workerfd;
    struct async_read_info worker_read_info;
    struct pac_hdr worker_pac_hdr;

    struct inqueue *inq;
    struct outqueue *outq;
    struct outqueue_control outctl;
};

struct cursor_info
{
    struct pac_hdr hdr;
    struct pac_channel_init init;
    struct pac_channel_ready ready;

    struct fd_context *crsfd;
    pthread_spinlock_t crslock;
    int crslen;
    uint8_t crsdata[SVP_MAX_CURSOR_DATA];

    struct outqueue *outq;
    struct outqueue_control outctl;
};

struct input_info
{
    struct pac_hdr hdr;
    struct pac_channel_init init;
    struct pac_channel_ready ready;
    uint8_t *rcvbuf;
};
*/

struct client
{
    // interfaces
    int (*send_cmd)(struct client *c, struct out_cmd *cmd);

    // callbacks
    void (*recv_cmd)(struct client *c, struct out_cmd *cmd);

    struct channel_base *channel;
    void *priv;
    struct fd_context *fd;
    uint8_t recvbuf[SVP_RECV_BUFSIZE];
    int ref;
    int id;

    struct outqueue *outq;
    uint64_t outq_bytes;

    struct list_head link;
};

struct channel_base
{
    // interfaces
    int (*start_listen)(struct channel_base *channel,
                        struct server_base *srv, const char *addr, uint16_t port);
    void (*stop_listen)(struct channel_base *channel);
    struct client *(*create_client)(struct channel_base *channel);
    void (*destroy_client)(struct channel_base *channel, struct client *client);
    struct client *(*find_client)(struct channel_base *channel, int id);

    // callbacks
    void (*connect)(struct channel_base *channel, struct client *c);
    void (*disconnect)(struct channel_base *channel, struct client *c);

    // private data
    struct server_base *srv;
    struct fd_context *listenfd;
    struct list_head client_list;
};

/*
int display_attach_framebuffer(struct svp_server *srv, unsigned char *frameBuffer,
                           int w, int h, int bpp);

void display_dirty_region(struct svp_server *srv, int x, int y, int w, int h);

void display_draw(struct svp_server *srv, int cmd, void *data, int size);

void cursor_update(struct svp_server *srv, struct pac_cursor *cursor,
                   uint8_t *mask, uint8_t *bitmap);
*/

int channel_base_init(struct channel_base *channel, struct server_base *srv);

void channel_base_fini(struct channel_base *channel);

void client_ref(struct client *c);

void client_deref(struct client *c);

#endif // CHANNEL_H
