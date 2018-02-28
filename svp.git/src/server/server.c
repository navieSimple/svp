#include "svp.h"
#include "server.h"
#include "channel.h"
#include "display_worker.h"
#include "list.h"
#include "inqueue.h"
#include "outqueue.h"
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#include <assert.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#define POLL_TIMEOUT 20

__thread uint8_t g_sndbuf[SVP_SEND_BUFSIZE];
__thread int g_sndlen = 0;

int server_add_fd(struct server_base *srv, int events, struct fd_context *ctx);

int server_mod_fd(struct server_base *srv, int events, struct fd_context *ctx);

void server_del_fd(struct server_base *srv, struct fd_context *ctx);

int server_register_event(struct server_base *srv, int event, server_event_function func, void *priv);

int server_trigger_event(struct server_base *srv, int event, void *arg);

int server_unregister_event(struct server_base *srv, int event);

struct fd_context *server_tcp_listen(struct server_base *srv, const char *addr, uint16_t port,
                                     new_tcp_connection_function func, void *priv);

struct fd_context *server_tcp_accept(struct server_base *srv, struct fd_context *listenfd);

struct fd_context *fd_from_raw(struct server_base *srv, int fd, fd_event_function handler, void *priv);

struct fd_context *fd_create_eventfd(struct server_base *srv, fd_event_function handler, void *priv);

struct fd_context *fd_create_socket(struct server_base *srv, fd_event_function handler, void *priv);

ssize_t fd_read(struct fd_context *fd, void *buf, size_t count);

ssize_t fd_write(struct fd_context *fd, const void *buf, size_t count);

int fd_async_read(struct fd_context *fd, void *data, int size,
                  async_done_function func, void *priv);

int fd_async_write(struct fd_context *fd, const void *data, int size,
                   async_done_function func, void *priv);

int fd_set_nodelay(struct fd_context *fd);

void fd_destroy(struct server_base *srv, struct fd_context *ctx);

ssize_t fd_read(struct fd_context *fd, void *buf, size_t count);

ssize_t fd_write(struct fd_context *fd, const void *buf, size_t count);

__attribute__((constructor))
void ignore_sigpipe()
{
    signal(SIGPIPE, SIG_IGN);
}

static int gettid()
{
    return syscall(SYS_gettid);
}

void set_nonblocking(int fd)
{
    int flag;
    flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flag, sizeof(flag));
}

void set_reuseaddr(int fd)
{
    int flag = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

int server_add_fd(struct server_base *srv, int events, struct fd_context *ctx)
{
    struct epoll_event ev;

    list_add(&ctx->link, &srv->xlist);
    ev.events = ctx->events = events;
    ev.data.ptr = ctx;
    return epoll_ctl(srv->epfd, EPOLL_CTL_ADD, ctx->fd, &ev);
}

int server_mod_fd(struct server_base *srv, int events, struct fd_context *ctx)
{
    struct epoll_event ev;

    ev.events = ctx->events = events;
    ev.data.ptr = ctx;
    return epoll_ctl(srv->epfd, EPOLL_CTL_MOD, ctx->fd, &ev);
}

void server_del_fd(struct server_base *srv, struct fd_context *ctx)
{
    list_del_init(&ctx->link);
    epoll_ctl(srv->epfd, EPOLL_CTL_DEL, ctx->fd, 0);
}

int on_user_event(struct server_base *srv, struct fd_context *fd, int event)
{
    struct server_event *ev = (struct server_event *)fd->priv;
    uint64_t val;
    if (!srv || !ev)
        return -1;
    if (!(event & EPOLLIN))
        return 0;
    int n = read(fd->fd, &val, sizeof(val));
    if (n != sizeof(val))
        return 0;
    if (ev->func)
        ev->func(srv, ev->event, ev->arg, ev->priv);

    return 0;
}

int server_register_event(struct server_base *srv, int event, server_event_function func, void *priv)
{
    struct server_event *ev;
    if (!srv || !srv->fd_create_eventfd)
        return -1;
    pthread_spin_lock(&srv->evlock);
    list_for_each_entry(ev, &srv->evlist, link) {
        if (event == ev->event) {
            dzlog_debug("error: event %d already registered", event);
            pthread_spin_unlock(&srv->evlock);
            return -1;
        }
    }
    ev = (struct server_event *)malloc(sizeof(*ev));
    ev->event = event;
    ev->func = func;
    ev->arg = 0;
    ev->priv = priv;
    ev->fd = srv->fd_create_eventfd(srv, on_user_event, ev);
    list_add_tail(&ev->link, &srv->evlist);
    pthread_spin_unlock(&srv->evlock);
    srv->add_fd(srv, EPOLLIN, ev->fd);
    return 0;
}

int server_trigger_event(struct server_base *srv, int event, void *arg)
{
    struct server_event *ev;
    uint64_t val = 1;
    pthread_spin_lock(&srv->evlock);
    list_for_each_entry(ev, &srv->evlist, link) {
        if (event == ev->event && ev->fd) {
            ev->arg = arg;
            write(ev->fd->fd, &val, sizeof(val));
            pthread_spin_unlock(&srv->evlock);
            return 0;
        }
    }
    pthread_spin_unlock(&srv->evlock);
    return -1;
}

int server_unregister_event(struct server_base *srv, int event)
{
    struct server_event *ev;
    pthread_spin_lock(&srv->evlock);
    list_for_each_entry(ev, &srv->evlist, link) {
        if (event == ev->event) {
            list_del(&ev->link);
            pthread_spin_unlock(&srv->evlock);
            srv->del_fd(srv, ev->fd);
            srv->fd_destroy(srv, ev->fd);
            free(ev);
            return 0;
        }
    }
    pthread_spin_unlock(&srv->evlock);
    return -1;
}

static int on_listenfd_event(struct server_base *srv, struct fd_context *listenfd, int event)
{
    if (!srv)
        return -1;
    if (event & EPOLLERR)
        listenfd->tcp_info.new_conn(srv, listenfd, -1, listenfd->priv);
    else if (event & EPOLLIN)
        listenfd->tcp_info.new_conn(srv, listenfd, 0, listenfd->priv);

    return 0;
}

struct fd_context *server_tcp_listen(struct server_base *srv, const char *addr, uint16_t port,
                                     new_tcp_connection_function func, void *priv)
{
    struct fd_context *ctx = srv->fd_create_socket(srv, on_listenfd_event, priv);
    struct sockaddr_in saddr;
    if (!ctx)
        return 0;
    ctx->tcp_info.new_conn = func;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(addr);
    saddr.sin_port = htons(port);
    if (bind(ctx->fd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) != 0)
        goto out_error;
    if (listen(ctx->fd, 5) == -1)
        goto out_error;
    srv->add_fd(srv, EPOLLIN, ctx);
    return ctx;
out_error:
    srv->fd_destroy(srv, ctx);
    return 0;
}

void async_done(struct async_info *ai, struct fd_context *fd, int status)
{
    ai->active = 0;
    ai->done(fd, status, ai->priv);
}

static int on_clientfd_event(struct server_base *srv, struct fd_context *clientfd, int event)
{
    struct async_info *ri = &clientfd->read_info;
    struct async_info *wi = &clientfd->write_info;
    int n;
    if (!srv)
        return -1;
    if ((event & EPOLLERR) || (event & EPOLLRDHUP)) {
        srv->del_fd(srv, clientfd);
        if (ri->active)
            async_done(ri, clientfd, -1);
        if (wi->active)
            async_done(wi, clientfd, -1);
        return 0;
    }
    if ((event & EPOLLIN) && ri->active) {
        n = clientfd->read(clientfd, ri->now, ri->end - ri->now);
        if (n < 0) {
            if (errno != EAGAIN) {
                srv->del_fd(srv, clientfd);
                async_done(ri, clientfd, -1);
            }
        } else {
            ri->now += n;
            if (ri->now == ri->end)
                async_done(ri, clientfd, 0);
        }
    }
    if ((event & EPOLLOUT) && wi->active) {
        n = clientfd->write(clientfd, wi->now, wi->end - wi->now);
        if (n < 0) {
            if (errno != EAGAIN) {
                dzlog_debug("wrote error %s", strerror(errno));
                srv->del_fd(srv, clientfd);
                async_done(wi, clientfd, -1);
            }
        } else {
            wi->now += n;
            if (wi->now == wi->end) {
                srv->mod_fd(clientfd->srv, EPOLLIN | EPOLLRDHUP, clientfd);
                async_done(wi, clientfd, 0);
            }
        }
    }
    return 0;
}

struct fd_context *server_tcp_accept(struct server_base *srv, struct fd_context *listenfd)
{
    struct fd_context *clientfd = srv->fd_create_socket(srv, on_clientfd_event, 0);
    if (!listenfd || !clientfd)
        return 0;
    clientfd->fd = accept(listenfd->fd, 0, 0);
    if (clientfd->fd == -1)
        goto out_error;
    srv->add_fd(srv, EPOLLIN | EPOLLRDHUP, clientfd);
    return clientfd;
out_error:
    srv->fd_destroy(srv, clientfd);
    return 0;
}

struct fd_context *fd_from_raw(struct server_base *srv, int fd, fd_event_function handler, void *priv)
{
    struct fd_context *ctx;
    ctx = (struct fd_context *)malloc(sizeof(*ctx));
    if (!ctx)
        return 0;
    memset(ctx, 0, sizeof(*ctx));
    ctx->srv = srv;
    ctx->fd = fd;
    ctx->events = 0;
    ctx->event = handler;
    ctx->priv = priv;
    ctx->read = fd_read;
    ctx->write = fd_write;
    ctx->async_read = fd_async_read;
    ctx->async_write = fd_async_write;
    ctx->set_nodelay = fd_set_nodelay;
    return ctx;
}

struct fd_context *fd_create_socket(struct server_base *srv, fd_event_function handler, void *priv)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    set_nonblocking(fd);
    set_reuseaddr(fd);
    return fd_from_raw(srv, fd, handler, priv);
}

struct fd_context *fd_create_eventfd(struct server_base *srv, fd_event_function handler, void *priv)
{
    return fd_from_raw(srv, eventfd(0, EFD_NONBLOCK), handler, priv);
}

ssize_t fd_read(struct fd_context *fd, void *buf, size_t count)
{
    if (!fd)
        return -1;
    return read(fd->fd, buf, count);
}

ssize_t fd_write(struct fd_context *fd, const void *buf, size_t count)
{
    if (!fd)
        return -1;
    return write(fd->fd, buf, count);
}

int fd_async_read(struct fd_context *fd, void *data, int size,
                  async_done_function func, void *priv)
{
    struct async_info *info = &fd->read_info;
    if (info->active)
        return -1;
    info->now = (uint8_t *)data;
    info->end = info->now + size;
    info->done = func;
    info->priv = priv;
    info->active = 1;
    return 0;
}

int fd_async_write(struct fd_context *fd, const void *data, int size,
                   async_done_function func, void *priv)
{
    struct async_info *info = &fd->write_info;
    if (info->active)
        return -1;
    info->now = (uint8_t *)data;
    info->end = info->now + size;
    info->done = func;
    info->priv = priv;
    info->active = 1;
    fd->srv->mod_fd(fd->srv, EPOLLIN | EPOLLRDHUP | EPOLLOUT, fd);
    return 0;
}

int fd_set_nodelay(struct fd_context *fd)
{
    char flag = 1;
    return setsockopt(fd->fd, IPPROTO_TCP, TCP_NODELAY, &flag, (socklen_t)sizeof(flag));
}

void fd_close(struct fd_context *ctx)
{
    if (!ctx)
        return;
    ctx->closing = 1;
}

void fd_destroy(struct server_base *srv, struct fd_context *ctx)
{
    if (!ctx)
        return;
    close(ctx->fd);
    free(ctx);
}

static void *server_run(void *arg)
{
    struct server_base *srv = (struct server_base *)arg;
    struct epoll_event evs[SVP_MAX_CLIENTS];
    struct epoll_event *ev;
    struct fd_context *fdctx, *tmp;
    int n;

    while (!srv->req_stop) {
        n = epoll_wait(srv->epfd, evs, SVP_MAX_CLIENTS, POLL_TIMEOUT);
        if (n <= 0)
            continue;
        for (ev = evs; ev < evs + n; ev++) {
            fdctx = (struct fd_context *)ev->data.ptr;
            if (fdctx->closing)
                continue;
            if (fdctx->event)
                fdctx->event(srv, fdctx, ev->events);
        }
        list_for_each_entry_safe(fdctx, tmp, &srv->xlist, link) {
            if (fdctx->closing) {
                server_del_fd(srv, fdctx);
                fd_destroy(srv, fdctx);
            }
        }
    }
    srv->running = 0;
    return 0;
}

int server_start(struct server_base *srv)
{
    srv->running = 1;
    if (pthread_create(&srv->tid, NULL, server_run, srv)) {
        dzlog_debug("create server thread error");
        srv->running = 0;
    }
}

int server_stop(struct server_base *srv)
{
    if (!srv->running)
        return 0;
    srv->req_stop = 1;
    while (srv->running)
        ;
    srv->req_stop = 0;
    return 0;
}

int server_running(struct server_base *srv)
{
    return srv->running;
}

int server_base_init(struct server_base *srv)
{
    memset(srv, 0, sizeof(*srv));
    srv->req_stop = 0;
    srv->running = 0;
    srv->epfd = epoll_create(10);
    INIT_LIST_HEAD(&srv->xlist);
    pthread_spin_init(&srv->evlock, PTHREAD_PROCESS_PRIVATE);
    INIT_LIST_HEAD(&srv->evlist);

    srv->start = server_start;
    srv->stop = server_stop;
    srv->is_running = server_running;

    srv->register_event = server_register_event;
    srv->trigger_event = server_trigger_event;
    srv->unregister_event = server_unregister_event;

    srv->tcp_listen = server_tcp_listen;
    srv->tcp_accept = server_tcp_accept;

    srv->add_fd = server_add_fd;
    srv->mod_fd = server_mod_fd;
    srv->del_fd = server_del_fd;

    srv->fd_from_raw = fd_from_raw;
    srv->fd_create_eventfd = fd_create_eventfd;
    srv->fd_create_socket = fd_create_socket;
    srv->fd_destroy = fd_destroy;
    return 0;
}

void server_base_fini(struct server_base *srv)
{
    // cleanup
    struct fd_context *fdctx, *tmp;
    list_for_each_entry_safe(fdctx, tmp, &srv->xlist, link) {
        srv->del_fd(srv, fdctx);
        fd_destroy(srv, fdctx);
    }
    close(srv->epfd);
    srv->epfd = -1;
}
