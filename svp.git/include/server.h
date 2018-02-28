#ifndef SERVER_H
#define SERVER_H

#include "svp.h"
#include "net_packet.h"
#include <pthread.h>
#include <unistd.h>

struct server_base;
struct async_read_info;
struct async_write_info;
struct fd_context;
typedef int (*fd_event_function)(struct server_base *srv, struct fd_context *fd, int event);
typedef void (*server_event_function)(struct server_base *srv, int event, void *arg, void *priv);
typedef void (*new_tcp_connection_function)(struct server_base *srv, struct fd_context *listenfd,
                                            int status, void *priv);
typedef void (*async_done_function)(struct fd_context *ctx, int status, void *priv);

struct async_info
{
    int active;
    uint8_t *now;
    uint8_t *end;
    void *priv;

    async_done_function done;
};

struct server_base;
struct fd_context
{
    // callbacks
    ssize_t (*read)(struct fd_context *fd, void *buf, size_t count);
    ssize_t (*write)(struct fd_context *fd, const void *buf, size_t count);
    int (*async_read)(struct fd_context *fd, void *data, int size,
                      async_done_function func, void *priv);
    int (*async_write)(struct fd_context *fd, const void *data, int size,
                       async_done_function func, void *priv);
    int (*set_nodelay)(struct fd_context *fd);

    struct server_base *srv;
    int fd;
    int events;
    int closing;
    fd_event_function event;
    void *priv;
    struct list_head link;

    union
    {
        struct
        {
            new_tcp_connection_function new_conn;
        } tcp_info;
    };
    struct async_info read_info;
    struct async_info write_info;
};

struct server_event {
    struct fd_context *fd;
    int event;
    server_event_function func;
    void *arg;
    void *priv;
    struct list_head link;
};

struct server_base
{
    int (*start)(struct server_base *srv);
    int (*stop)(struct server_base *srv);
    int (*is_running)(struct server_base *srv);

    int (*register_event)(struct server_base *srv, int event, server_event_function func, void *priv);
    int (*trigger_event)(struct server_base *srv, int event, void *arg);
    int (*unregister_event)(struct server_base *srv, int event);

    struct fd_context *(*tcp_listen)(struct server_base *srv, const char *addr, uint16_t port,
                                     new_tcp_connection_function func, void *priv);
    struct fd_context *(*tcp_accept)(struct server_base *srv, struct fd_context *listenfd);

    int (*add_fd)(struct server_base *srv, int events, struct fd_context *ctx);
    int (*mod_fd)(struct server_base *srv, int events, struct fd_context *ctx);
    void (*del_fd)(struct server_base *srv, struct fd_context *ctx);

    struct fd_context *(*fd_from_raw)(struct server_base *srv, int fd, fd_event_function func, void *priv);
    struct fd_context *(*fd_create_socket)(struct server_base *srv, fd_event_function func, void *priv);
    struct fd_context *(*fd_create_eventfd)(struct server_base *srv, fd_event_function func, void *priv);
    void (*fd_destroy)(struct server_base *srv, struct fd_context *fd);

    int running;
    int req_stop;
    pthread_t tid;
    int epfd;
    struct list_head xlist;
    pthread_spinlock_t evlock;
    struct list_head evlist;
};

void set_nonblocking(int fd);

void set_reuseaddr(int fd);

int server_base_init(struct server_base *srv);

void server_base_fini(struct server_base *srv);

int server_base_start(struct server_base *srv);

int server_base_stop(struct server_base *srv);

#endif // SERVER_H
