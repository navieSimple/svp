#include <gtest/gtest.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
extern "C" {
#include "server/vrde.c"
#include "server/server.c"
#include "server/svp_server.c"
#include "server/channel.c"
#include "server/main_channel.c"
#include "server/display_channel.c"
#include "server/display_worker.c"
#include "server/cursor_channel.c"
#include "server/input_channel.c"
#include "server/usb_channel.c"
#include "server/usb.c"
#include "server/graphic.c"
#include "server/svp.c"
#include "server/inqueue.c"
#include "server/outqueue.c"
#include "server/cmd.c"
#include "server/codec.c"
#include "server/encoder.c"
}
using namespace std;
/*
 **** server base
 * dummy start and stop
 * event
 * multiple event trigger once
 * different event type
 **** tcp socket
 * listen
 * accept new connection
 * async read
 * multiple async read error
 * async write
 * multile async write error
 * async read and write simultaneously
 **** channel base:
 * client connect
 * client disconnect
 * recv cmd
 * send cmd
 **** svp server:
 * create channels
 * destroy channels
 **** channels:
 */
#define TEST_PORT 54321
#define TEST_EVENT1 1001
#define TEST_EVENT2 1002

struct mock_server
{
    struct server_base base;
    void *priv;
};

static bool bindablePort(unsigned short port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;
    set_reuseaddr(fd);
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("0.0.0.0");
    saddr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) == 0) {
        close(fd);
        return true;
    } else {
        close(fd);
        return false;
    }
}

class ServerBaseTest : public ::testing::Test
{
public:
    void SetUp()
    {
        val = 0;
        arg1 = 0;
        arg2 = 0;
        memset(buf, 0, sizeof(buf));
        len = 0;
        ASSERT_EQ(0, server_base_init(&server));
    }

    void TearDown()
    {
        clearHistory();
        server_base_fini(&server);
    }

    /*
    static int onLocalSocketEvent(struct server_base *srv, struct fd_context *fd, int event)
    {
        if (!(event & EPOLLIN))
            return 0;
        int n;
        ServerBaseTest *me = (ServerBaseTest *)fd->priv;
        n = fd->read(fd, me->buf + me->len, 1000 - me->len);
        me->len += n;
        return 0;
    }

    static int onTcpListenSocketEvent(struct server_base *srv, struct fd_context *fd, int event)
    {
        if (!(event & EPOLLIN))
            return 0;
        ServerBaseTest *me = (ServerBaseTest *)fd->priv;
        int clientfd = accept(fd->fd, 0, 0);
        if (clientfd == -1)
            return 0;
        struct fd_context *ctx = srv->fd_from_raw(srv, clientfd, onTcpDataSocketEvent, me);
        server_add_fd(&me->server, EPOLLIN | EPOLLRDHUP, ctx);
        return 0;
    }

    static int onTcpDataSocketEvent(struct server_base *srv, struct fd_context *fd, int event)
    {
        if (!(event & EPOLLIN))
            return 0;
        int n;
        ServerBaseTest *me = (ServerBaseTest *)fd->priv;
        n = fd->read(fd, me->buf + me->len, 1000 - me->len);
        me->len += n;
        return 0;
    }

    int tcpListen(const char *addr, unsigned short port)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1)
            return -1;
        set_nonblocking(fd);
        set_reuseaddr(fd);
        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = inet_addr(addr);
        saddr.sin_port = htons(port);
        if (bind(fd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) != 0)
            return -1;
        if (listen(fd, 5) == -1)
            return -1;
        return fd;
    }
    */

    int tcpConnect(const char *addr, unsigned short port)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1)
            return -1;
        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = inet_addr(addr);
        saddr.sin_port = htons(port);
        if (connect(fd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) != 0)
            return -1;
        return fd;
    }

    void clearHistory()
    {
        statusHistory.clear();
        fdHistory.clear();
    }

    static void onTestEvent1(struct server_base *srv, int event, void *arg, void *priv)
    {
        ServerBaseTest *me = (ServerBaseTest *)priv;
        uint64_t *i = (uint64_t *)arg;
        ASSERT_TRUE(me);
        ASSERT_TRUE(i);
        me->val += *i;
    }

    static void onTestEvent2(struct server_base *srv, int event, void *arg, void *priv)
    {
        ServerBaseTest *me = (ServerBaseTest *)priv;
        uint64_t *i = (uint64_t *)arg;
        ASSERT_TRUE(me);
        ASSERT_TRUE(i);
        me->val += *i;
    }

    static void onNewTcpConnection(struct server_base *srv, struct fd_context *listenfd, int status, void *priv)
    {
        ServerBaseTest *me = (ServerBaseTest *)priv;
        me->statusHistory.push_back(status);
        if (status != 0) {
            me->fdHistory.push_back(0);
            return;
        }
        struct fd_context *clientfd = srv->tcp_accept(srv, listenfd);
        me->fdHistory.push_back(clientfd);
    }

    static void onAsyncReadDone(struct fd_context *clientfd, int status, void *priv)
    {
        ServerBaseTest *me = (ServerBaseTest *)priv;
        me->statusHistory.push_back(status);
    }

    static void onAsyncWriteDone(struct fd_context *clientfd, int status, void *priv)
    {
        ServerBaseTest *me = (ServerBaseTest *)priv;
        me->statusHistory.push_back(status);
    }

    struct server_base server;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t val;
    char buf[1000];
    char len;
    vector<int> statusHistory;
    vector<struct fd_context *> fdHistory;
};

TEST_F(ServerBaseTest, StartStop)
{
    ASSERT_EQ(0, server.start(&server));
    ASSERT_TRUE(server.is_running(&server));
    ASSERT_EQ(0, server.stop(&server));
    ASSERT_FALSE(server.is_running(&server));
}

TEST_F(ServerBaseTest, SimpleEvent)
{
    ASSERT_EQ(0, server_register_event(&server, TEST_EVENT1, onTestEvent1, this));

    server.start(&server);

    arg1 = 3;
    server.trigger_event(&server, TEST_EVENT1, &arg1);
    usleep(50 * 1000);
    EXPECT_EQ(3, val);
    server.trigger_event(&server, TEST_EVENT1, &arg1);
    usleep(50 * 1000);
    EXPECT_EQ(6, val);

    ASSERT_EQ(0, server_unregister_event(&server, TEST_EVENT1));

    server.stop(&server);
}

TEST_F(ServerBaseTest, ManyEventTriggerOnce)
{
    ASSERT_EQ(0, server_register_event(&server, TEST_EVENT1, onTestEvent1, this));

    server.start(&server);

    arg1 = 3;
    server.trigger_event(&server, TEST_EVENT1, &arg1);
    server.trigger_event(&server, TEST_EVENT1, &arg1);
    usleep(50 * 1000);
    EXPECT_EQ(3, val);

    ASSERT_EQ(0, server_unregister_event(&server, TEST_EVENT1));

    server.stop(&server);
}

TEST_F(ServerBaseTest, DifferentEvent)
{
    ASSERT_EQ(0, server.register_event(&server, TEST_EVENT1, onTestEvent1, this));
    ASSERT_EQ(0, server.register_event(&server, TEST_EVENT2, onTestEvent2, this));

    server.start(&server);

    arg1 = 3;
    server.trigger_event(&server, TEST_EVENT1, &arg1);
    arg2 = 4;
    server.trigger_event(&server, TEST_EVENT2, &arg2);
    usleep(50 * 1000);
    EXPECT_EQ(7, val);

    ASSERT_EQ(0, server.unregister_event(&server, TEST_EVENT1));

    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpListen)
{
    server.start(&server);
    ASSERT_TRUE(server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this));
    ASSERT_FALSE(bindablePort(TEST_PORT));
    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpBindTwiceError)
{
    server.start(&server);
    ASSERT_TRUE(server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this));
    ASSERT_FALSE(bindablePort(TEST_PORT));
    ASSERT_FALSE(server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this));
    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpAccept)
{
    int fd;
    server.start(&server);
    server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this);
    fd = tcpConnect("127.0.0.1", TEST_PORT);
    ASSERT_NE(-1, fd);
    usleep(50 * 1000);
    ASSERT_EQ(1, statusHistory.size());
    ASSERT_EQ(0, statusHistory[0]);
    ASSERT_EQ(1, fdHistory.size());
    server.stop(&server);
    close(fd);
}

TEST_F(ServerBaseTest, TcpAsyncRead)
{
    int fd;
    int n;
    struct fd_context *ctx;
    char s[10] = "hello";
    server.start(&server);
    server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this);
    fd = tcpConnect("127.0.0.1", TEST_PORT);
    usleep(50 * 100);
    ASSERT_EQ(1, fdHistory.size());
    ctx = fdHistory[0];
    clearHistory();
    ASSERT_EQ(0, ctx->async_read(ctx, buf, 10, onAsyncReadDone, this));
    for (int i = 0; i < sizeof(s); i++) {
        n = write(fd, s + i, 1);
        usleep(10 * 100);
    }
    ASSERT_STREQ(s, buf);
    ASSERT_EQ(1, statusHistory.size());
    ASSERT_EQ(0, statusHistory[0]);
    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpAsyncWrite)
{
    int fd;
    struct fd_context *ctx;
    char s[10] = "hello";
    server.start(&server);
    server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this);
    fd = tcpConnect("127.0.0.1", TEST_PORT);
    usleep(50 * 100);
    ASSERT_EQ(1, fdHistory.size());
    ctx = fdHistory[0];
    clearHistory();
    ASSERT_EQ(0, ctx->async_write(ctx, s, 10, onAsyncWriteDone, this));
    for (int i = 0; i < sizeof(s); i++) {
        read(fd, buf + i, 1);
        usleep(10 * 100);
    }
    ASSERT_STREQ(s, buf);
    ASSERT_EQ(1, statusHistory.size());
    ASSERT_EQ(0, statusHistory[0]);
    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpMultipleAsyncReadError)
{
    int fd;
    struct fd_context *ctx;
    char s[10] = "hello";
    server.start(&server);
    server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this);
    fd = tcpConnect("127.0.0.1", TEST_PORT);
    usleep(50 * 100);
    ASSERT_EQ(1, fdHistory.size());
    ctx = fdHistory[0];
    clearHistory();
    ASSERT_EQ(0, ctx->async_read(ctx, buf, 10, onAsyncReadDone, this));
    ASSERT_EQ(-1, ctx->async_read(ctx, buf, 10, onAsyncReadDone, this));
    write(fd, s, 10);
    ASSERT_STREQ(s, buf);
    ASSERT_EQ(1, statusHistory.size());
    ASSERT_EQ(0, statusHistory[0]);
    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpMultipleAsyncWriteError)
{
    int fd;
    struct fd_context *ctx;
    char s[10] = "hello";
    server.start(&server);
    server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this);
    fd = tcpConnect("127.0.0.1", TEST_PORT);
    usleep(50 * 100);
    ASSERT_EQ(1, fdHistory.size());
    ctx = fdHistory[0];
    clearHistory();
    ASSERT_EQ(0, ctx->async_write(ctx, s, 10, onAsyncWriteDone, this));
    ASSERT_EQ(-1, ctx->async_write(ctx, s, 10, onAsyncWriteDone, this));
    read(fd, buf, 10);
    ASSERT_STREQ(s, buf);
    ASSERT_EQ(1, statusHistory.size());
    ASSERT_EQ(0, statusHistory[0]);
    server.stop(&server);
}

TEST_F(ServerBaseTest, TcpAsyncOneReadOneWrite)
{
    int fd;
    struct fd_context *ctx;
    char s[10] = "hello";
    char s2[10];
    server.start(&server);
    server.tcp_listen(&server, "0.0.0.0", TEST_PORT, onNewTcpConnection, this);
    fd = tcpConnect("127.0.0.1", TEST_PORT);
    usleep(50 * 100);
    ASSERT_EQ(1, fdHistory.size());
    ctx = fdHistory[0];
    clearHistory();
    ASSERT_EQ(0, ctx->async_write(ctx, s, 10, onAsyncWriteDone, this));
    ASSERT_EQ(0, ctx->async_read(ctx, s2, 10, onAsyncReadDone, this));
    write(fd, s, 10);
    read(fd, buf, 10);
    usleep(50 * 100);
    ASSERT_EQ(2, statusHistory.size());
    ASSERT_EQ(0, statusHistory[0]);
    ASSERT_STREQ(s, s2);
    server.stop(&server);
}

class ChannelBaseTest;
static ChannelBaseTest *g_channelTest;

class ChannelBaseTest : public ::testing::Test
{
public:
    void SetUp()
    {
        g_channelTest = this;

        server_base_init(&mserver.base);
        mserver.base.add_fd = stubAddFD;
        mserver.base.mod_fd = stubModFD;
        mserver.base.del_fd = stubDelFD;
        mserver.base.fd_destroy = stubDestroyFD;
        mserver.base.tcp_listen = stubTcpListen;
        mserver.base.tcp_accept = stubTcpAccept;
        mserver.priv = this;

        stubTcpListenCount = 0;
        stubTcpAcceptCount = 0;
        stubConnectCount = 0;
        stubDisconnectCount = 0;

        memset(&clientfd, 0, sizeof(clientfd));
        clientfd.srv = &mserver.base;
        clientfd.event = on_clientfd_event;
        clientfd.read = stubRead;
        clientfd.write = stubWrite;
        clientfd.async_read = fd_async_read;
        clientfd.async_write = fd_async_write;

        listenfd.srv = &mserver.base;
        listenfdStopped = false;

        channel_base_init(&channel, &mserver.base);
        channel.connect = stubConnect;
        channel.disconnect = stubDisconnect;

        nread = 0;
        nwritten = 0;
        cmd.hdr = {SVP_MAGIC, SC_SOLID_RECT, sizeof(cmd.rect)};
        cmd.rect = {0, 1, 10, 20, 0xff00ff00};
        memcpy(readBuf, &cmd, sizeof(cmd));

        slowClient = false;
        sendCommand = false;
        destroyClientCalled = false;
    }

    void TearDown()
    {
        channel_base_fini(&channel);
        server_base_fini(&mserver.base);
        clearHistory();
    }

    void clearHistory()
    {
        newConnFuncHistory.clear();
        privHistory.clear();
        clientConnectHistory.clear();
        clientDisconnectHistory.clear();
        sendHistory.clear();
    }

    static void stubDestroyFD(struct server_base *srv, struct fd_context *fd)
    {
        return;
    }

    static ssize_t stubRead(struct fd_context *fd, void *buf, size_t count)
    {
        if (!g_channelTest)
            return -1;
        int n = min(count, sizeof(cmd) - g_channelTest->nread);
        if (n <= 0) {
            errno = EAGAIN;
            return -1;
        }
        if (g_channelTest->slowClient) {
            if (rand() % 10 < 5)
                n = 1;
            else {
                errno = EAGAIN;
                return -1;
            }
        }
        memcpy(buf, g_channelTest->readBuf + g_channelTest->nread, n);
        g_channelTest->nread += n;
        return n;
    }

    static ssize_t stubWrite(struct fd_context *fd, const void *buf, size_t count)
    {
        if (!g_channelTest)
            return -1;
        int n = min(count, sizeof(cmd) - g_channelTest->nwritten);
        if (n <= 0) {
            errno = EAGAIN;
            return -1;
        }
        if (g_channelTest->slowClient) {
            if (rand() % 10 < 5)
                n = 1;
            else {
                errno = EAGAIN;
                return -1;
            }
        }
        memcpy(g_channelTest->writeBuf + g_channelTest->nwritten, buf, n);
        g_channelTest->nwritten += n;
        return n;
    }

    static int stubAddFD(struct server_base *srv, int events, struct fd_context *ctx)
    {
        return 0;
    }

    static int stubModFD(struct server_base *srv, int events, struct fd_context *ctx)
    {
        return 0;
    }

    static void stubDelFD(struct server_base *srv, struct fd_context *ctx)
    {
        if (ctx == &g_channelTest->listenfd)
            g_channelTest->listenfdStopped = true;
        return;
    }

    static struct fd_context *stubTcpListen(struct server_base *srv, const char *addr, uint16_t port,
                                            new_tcp_connection_function func, void *priv)
    {
        ChannelBaseTest *me = (ChannelBaseTest *)((struct mock_server *)srv)->priv;
        me->stubTcpListenCount++;
        me->newConnFuncHistory.push_back(func);
        me->privHistory.push_back(priv);
        return &me->listenfd;
    }

    static struct fd_context *stubTcpAccept(struct server_base *srv, struct fd_context *listenfd)
    {
        ChannelBaseTest *me = (ChannelBaseTest *)((struct mock_server *)srv)->priv;
        me->stubTcpAcceptCount++;
        return &me->clientfd;
    }

    static void stubConnect(struct channel_base *channel, struct client *c)
    {
        ChannelBaseTest *me = (ChannelBaseTest *)((struct mock_server *)channel->srv)->priv;
        struct out_cmd cmd;

        c->recv_cmd = stubRecvCommand;
        c->priv = me;
        me->stubConnectCount++;
        me->clientConnectHistory.push_back(c);

        cmd.type = me->cmd.hdr.c_cmd;
        cmd.size = sizeof(me->cmd);
        cmd.data = (uint8_t *)malloc(cmd.size);
        if (cmd.data)
            memcpy(cmd.data, &me->cmd, cmd.size);
        if (me->sendCommand) {
            int rc = c->send_cmd(c, &cmd);
            me->sendHistory.push_back(rc);
        }
    }

    static void stubDisconnect(struct channel_base *channel, struct client *c)
    {
        ChannelBaseTest *me = (ChannelBaseTest *)((struct mock_server *)channel->srv)->priv;
        me->stubDisconnectCount++;
        me->clientDisconnectHistory.push_back(c);
    }

    void mockClientConnect()
    {
        ASSERT_EQ(1, newConnFuncHistory.size());
        ASSERT_EQ(1, privHistory.size());
        newConnFuncHistory[0](&mserver.base, 0, 0, privHistory[0]);
    }

    void mockClientSend()
    {
        nread = 0;
        memcpy(readBuf, &cmd, sizeof(cmd));
        while (nread < sizeof(cmd)) {
            clientfd.event(&mserver.base, &clientfd, EPOLLIN);
            usleep(10 * 1000);
        }
    }

    void mockClientRecv()
    {
        nwritten = 0;
        memcpy(writeBuf, &cmd, sizeof(cmd));
        while (nwritten < sizeof(cmd)) {
            clientfd.event(&mserver.base, &clientfd, EPOLLOUT);
            usleep(10 * 1000);
        }
    }

    void mockClientDisconnect()
    {
        clientfd.event(&mserver.base, &clientfd, EPOLLRDHUP);
    }

    static void stubDestroyClient(struct channel_base *channel, struct client *client)
    {
        g_channelTest->destroyClientCalled = true;
        channel_destroy_client(channel, client);
    }

    void hookChannel(struct channel_base *channel)
    {
        channel->destroy_client = stubDestroyClient;
    }

    void mockClientError()
    {
        clientfd.event(&mserver.base, &clientfd, EPOLLERR);
    }

    static void stubRecvCommand(struct client *c, struct out_cmd *cmd)
    {
        ChannelBaseTest *me = (ChannelBaseTest *)c->priv;
        struct pac_hdr *hdr = (struct pac_hdr *)cmd->data;
        memcpy(me->readBuf, cmd->data, cmd->size);
    }

    /*
    static int stubSendCommand(struct client *c, struct pac_hdr *hdr, const void *data)
    {
        ChannelBaseTest *me = (ChannelBaseTest *)c->priv;
        return 0;
    }
    */

    struct mock_server mserver;
    struct channel_base channel;
    int stubTcpListenCount;
    int stubTcpAcceptCount;
    vector<new_tcp_connection_function> newConnFuncHistory;
    vector<void *> privHistory;
    struct fd_context clientfd;
    bool listenfdStopped;
    int stubConnectCount;
    int stubDisconnectCount;
    vector<struct client *> clientConnectHistory;
    vector<struct client *> clientDisconnectHistory;
    bool destroyClientCalled;

    struct {
        struct pac_hdr hdr;
        struct pac_solid_rect rect;
    } __attribute__((packed)) cmd;

    int nread;
    int nwritten;
    char readBuf[SVP_RECV_BUFSIZE];
    char writeBuf[SVP_SEND_BUFSIZE];

    bool slowClient;
    struct fd_context listenfd;

    bool sendCommand;
    vector<int> sendHistory;
};

TEST_F(ChannelBaseTest, StartListen)
{
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);

    ASSERT_EQ(1, stubTcpListenCount);
}

TEST_F(ChannelBaseTest, StopListen)
{
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    ASSERT_EQ(1, stubTcpListenCount);
    channel.stop_listen(&channel);
    ASSERT_TRUE(listenfdStopped);
}

TEST_F(ChannelBaseTest, ClientConnect)
{
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);

    ASSERT_EQ(1, stubTcpListenCount);
    mockClientConnect();
    ASSERT_EQ(1, stubTcpAcceptCount);

    ASSERT_EQ(1, stubConnectCount);
}

TEST_F(ChannelBaseTest, ClientDisconnect)
{
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);

    ASSERT_EQ(1, stubTcpListenCount);
    mockClientConnect();
    ASSERT_EQ(1, stubTcpAcceptCount);

    mockClientDisconnect();
    ASSERT_EQ(1, stubDisconnectCount);
}

TEST_F(ChannelBaseTest, ClientError)
{
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);

    hookChannel(&channel);
    mockClientConnect();
    mockClientError();

    ASSERT_EQ(1, stubDisconnectCount);
    ASSERT_TRUE(destroyClientCalled);
}

TEST_F(ChannelBaseTest, Recv)
{
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    mockClientConnect();
    usleep(50 * 1000);
    mockClientSend();
    ASSERT_EQ(sizeof(cmd), nread);
    mockClientDisconnect();
}

TEST_F(ChannelBaseTest, RecvSlow)
{
    slowClient = true;
    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    mockClientConnect();
    usleep(50 * 1000);
    mockClientSend();
    ASSERT_EQ(sizeof(cmd), nread);
    mockClientDisconnect();
}

TEST_F(ChannelBaseTest, RecvCommand)
{
    struct pac_hdr *hdr = (struct pac_hdr *)readBuf;

    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    mockClientConnect();
    usleep(50 * 1000);
    mockClientSend();
    ASSERT_EQ(cmd.hdr.c_magic, hdr->c_magic);
    ASSERT_EQ(cmd.hdr.c_cmd, hdr->c_cmd);
    ASSERT_EQ(cmd.hdr.c_size, hdr->c_size);
    ASSERT_TRUE(memcmp(&cmd.rect, readBuf + sizeof(*hdr), cmd.hdr.c_size) == 0);
    mockClientDisconnect();
}

TEST_F(ChannelBaseTest, RecvMultipleCommands)
{
    struct pac_hdr *hdr = (struct pac_hdr *)readBuf;

    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    mockClientConnect();
    for (int i = 0; i < 5; i++) {
        mockClientSend();
        ASSERT_EQ(cmd.hdr.c_magic, hdr->c_magic);
        ASSERT_EQ(cmd.hdr.c_cmd, hdr->c_cmd);
        ASSERT_EQ(cmd.hdr.c_size, hdr->c_size);
        ASSERT_TRUE(memcmp(&cmd.rect, readBuf + sizeof(*hdr), cmd.hdr.c_size) == 0);
        usleep(50 * 1000);
    }
    mockClientDisconnect();
}

TEST_F(ChannelBaseTest, SendCommand)
{
    struct pac_hdr *hdr = (struct pac_hdr *)writeBuf;

    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    sendCommand = true;
    mockClientConnect();
    usleep(50 * 1000);
    mockClientRecv();
    ASSERT_EQ(1, sendHistory.size());
    ASSERT_EQ(0, sendHistory[0]);
    ASSERT_EQ(cmd.hdr.c_magic, hdr->c_magic);
    ASSERT_EQ(cmd.hdr.c_cmd, hdr->c_cmd);
    ASSERT_EQ(cmd.hdr.c_size, hdr->c_size);
    ASSERT_TRUE(memcmp(&cmd.rect, writeBuf + sizeof(*hdr), cmd.hdr.c_size) == 0);
    mockClientDisconnect();
}

TEST_F(ChannelBaseTest, SendSlow)
{
    struct pac_hdr *hdr = (struct pac_hdr *)writeBuf;

    channel.start_listen(&channel, &mserver.base, "0.0.0.0", TEST_PORT);
    sendCommand = true;
    slowClient = true;
    mockClientConnect();
    usleep(50 * 1000);
    mockClientRecv();
    ASSERT_EQ(1, sendHistory.size());
    ASSERT_EQ(0, sendHistory[0]);
    ASSERT_EQ(cmd.hdr.c_magic, hdr->c_magic);
    ASSERT_EQ(cmd.hdr.c_cmd, hdr->c_cmd);
    ASSERT_EQ(cmd.hdr.c_size, hdr->c_size);
    ASSERT_TRUE(memcmp(&cmd.rect, writeBuf + sizeof(*hdr), cmd.hdr.c_size) == 0);
    mockClientDisconnect();
}

class SvpServerTest;
static SvpServerTest *g_svpServerTest = 0;
class SvpServerTest : public ::testing::Test
{
public:
    void SetUp()
    {
        g_svpServerTest = this;

        srv = 0;
        mainChannelDestroyCalled = false;
        displayChannelDestroyCalled = false;
        cursorChannelDestroyCalled = false;
        inputChannelDestroyCalled = false;

        saved_main_destroy = 0;
        saved_display_destroy = 0;
        saved_cursor_destroy = 0;
        saved_input_destroy = 0;
    }

    void TearDown()
    {

    }

    static void mockMainChannelDestroy(struct svp_server *srv, struct main_channel *channel)
    {
        g_svpServerTest->mainChannelDestroyCalled = true;
        if (g_svpServerTest->saved_main_destroy)
            return g_svpServerTest->saved_main_destroy(srv, channel);
    }

    static void mockDisplayChannelDestroy(struct svp_server *srv, struct display_channel *channel)
    {
        g_svpServerTest->displayChannelDestroyCalled = true;
        if (g_svpServerTest->saved_display_destroy)
            return g_svpServerTest->saved_display_destroy(srv, channel);
    }

    static void mockCursorChannelDestroy(struct svp_server *srv, struct cursor_channel *channel)
    {
        g_svpServerTest->cursorChannelDestroyCalled = true;
        if (g_svpServerTest->saved_cursor_destroy)
            return g_svpServerTest->saved_cursor_destroy(srv, channel);
    }

    static void mockInputChannelDestroy(struct svp_server *srv, struct input_channel *channel)
    {
        g_svpServerTest->inputChannelDestroyCalled = true;
        if (g_svpServerTest->saved_input_destroy)
            return g_svpServerTest->saved_input_destroy(srv, channel);
    }

    void hookChannelDestroy(struct svp_server *srv)
    {
        saved_main_destroy = srv->main_channel_destroy;
        srv->main_channel_destroy = mockMainChannelDestroy;
        saved_display_destroy = srv->display_channel_destroy;
        srv->display_channel_destroy = mockDisplayChannelDestroy;
        saved_cursor_destroy = srv->cursor_channel_destroy;
        srv->cursor_channel_destroy = mockCursorChannelDestroy;
        saved_input_destroy = srv->input_channel_destroy;
        srv->input_channel_destroy = mockInputChannelDestroy;
    }

    struct svp_server *srv;

    void (*saved_main_destroy)(struct svp_server *, struct main_channel *);
    void (*saved_display_destroy)(struct svp_server *, struct display_channel *);
    void (*saved_cursor_destroy)(struct svp_server *, struct cursor_channel *);
    void (*saved_input_destroy)(struct svp_server *, struct input_channel *);

    bool mainChannelDestroyCalled;
    bool displayChannelDestroyCalled;
    bool cursorChannelDestroyCalled;
    bool inputChannelDestroyCalled;
};

TEST_F(SvpServerTest, CreateServerWillCreateChannels)
{
    srv = svp_server_create("0.0.0.0", TEST_PORT);
    ASSERT_TRUE(srv);
    ASSERT_TRUE(srv->main_channel);
    ASSERT_TRUE(srv->display_channel);
    ASSERT_TRUE(srv->cursor_channel);
    ASSERT_TRUE(srv->input_channel);
    svp_server_destroy(srv);
}

TEST_F(SvpServerTest, DestroyServerWillDestroyChannels)
{
    srv = svp_server_create("0.0.0.0", TEST_PORT);
    hookChannelDestroy(srv);
    svp_server_destroy(srv);
    ASSERT_TRUE(mainChannelDestroyCalled);
    ASSERT_TRUE(displayChannelDestroyCalled);
    ASSERT_TRUE(cursorChannelDestroyCalled);
    ASSERT_TRUE(inputChannelDestroyCalled);
}

TEST_F(SvpServerTest, Start)
{
    srv = svp_server_create("0.0.0.0", TEST_PORT);
    ASSERT_EQ(0, srv->start(srv));
    svp_server_destroy(srv);
}

void initRect(struct svp_solid_rect *s_rect)
{
    *s_rect = {1, 2, 10, 20, 0xff00ff00};
}

bool checkGraphicUpdate(struct out_cmd *cmd, struct svp_solid_rect *s_rect)
{
    if (cmd->type != SC_SOLID_RECT
            || cmd->size != sizeof(struct pac_hdr) + sizeof(struct pac_solid_rect)) {
        GTEST_LOG_(ERROR) << "bad size, expect"
                          << sizeof(struct pac_hdr) + sizeof(struct pac_solid_rect)
                          << "got" << cmd->size;
        return false;
    }
    struct pac_hdr *ahdr = (struct pac_hdr *)cmd->data;
    struct pac_solid_rect *rect = (struct pac_solid_rect *)(cmd->data + sizeof(*ahdr));
    if (ahdr->c_magic != SVP_MAGIC || ahdr->c_cmd != SC_SOLID_RECT
            || ahdr->c_size != cmd->size - sizeof(struct pac_hdr)) {
        GTEST_LOG_(ERROR) << "bad header size, expect"
                          << sizeof(struct pac_solid_rect)
                          << "got" << ahdr->c_size;
        return false;
    }
    if (rect->x != s_rect->x || rect->y != s_rect->y
            || rect->w != s_rect->w || rect->h != s_rect->h
            || rect->rgb != s_rect->rgb) {
        GTEST_LOG_(ERROR) << "rect mismatch";
        return false;
    }
    return true;
}

class SvpChannelTest;
static SvpChannelTest *g_svpChannelTest = 0;
class SvpChannelTest : public ::testing::Test
{
public:
    void SetUp()
    {
        g_svpChannelTest = this;
        srv = svp_server_create("0.0.0.0", TEST_PORT);
        sentCmdH.clear();
        keyboardInputCalled = false;
        mouseInputCalled = false;
        downH = -1;
        scancodeH = -1;
        xH = -1;
        yH = -1;
        buttonsH = -1;
        clientLogonCalled = false;
        clientLogoutCalled = false;
        videoModeSetCalled = false;

        srv->start(srv);
    }

    void TearDown()
    {
        srv->stop(srv);
        svp_server_destroy(srv);
    }

    static int mockSendCmd(struct client *c, struct out_cmd *cmd)
    {
        struct out_cmd copy = *cmd;
        g_svpChannelTest->sentCmdH.push_back(copy);
    }

    void hookSendCmd(struct client *c)
    {
        c->send_cmd = mockSendCmd;
    }

    bool checkChannelList(struct out_cmd *cmd)
    {
        if (cmd->type != SC_CHANNEL_LIST
                || cmd->size != sizeof(struct pac_hdr) + sizeof(struct pac_channel_list))
            return false;
        struct pac_hdr *ahdr = (struct pac_hdr *)cmd->data;
        struct pac_channel_list *chlist = (struct pac_channel_list *)(cmd->data + sizeof(*ahdr));
        if (ahdr->c_magic != SVP_MAGIC || ahdr->c_cmd != SC_CHANNEL_LIST
                || ahdr->c_size != sizeof(struct pac_channel_list))
            return false;
        if (chlist->server_version > 1 || chlist->count != 4)
            return false;
        for (int i = 0; i < chlist->count; i++) {
            if (chlist->channel[i].type != SH_MAIN + i)
                return false;
            if (chlist->channel[i].port != TEST_PORT + i)
                return false;
        }
        return true;
    }

    bool checkChannelReady(struct out_cmd *cmd)
    {
        if (cmd->type != SC_CHANNEL_READY
                || cmd->size != sizeof(struct pac_hdr) + sizeof(struct pac_channel_ready))
            return false;
        struct pac_hdr *ahdr = (struct pac_hdr *)cmd->data;
        struct pac_channel_ready *ready = (struct pac_channel_ready *)(cmd->data + sizeof(*ahdr));
        if (ahdr->c_magic != SVP_MAGIC || ahdr->c_cmd != SC_CHANNEL_READY
                || ahdr->c_size != sizeof(struct pac_channel_ready))
            return false;
        if (ready->features == 0)
            return false;
        return true;
    }

    bool checkScreenResize(struct out_cmd *cmd, int w, int h)
    {
        struct
        {
            struct pac_hdr hdr;
            struct pac_screen_resize resize;
        } __attribute__((packed)) *p = (typeof(p))cmd->data;
        if (cmd->type != SC_SCREEN_RESIZE || cmd->size != sizeof(*p))
            return false;
        if (p->hdr.c_magic != SVP_MAGIC || p->hdr.c_cmd != SC_SCREEN_RESIZE
                || p->hdr.c_size != sizeof(p->resize))
            return false;
        if (p->resize.w != w || p->resize.h != h)
            return false;
        return true;
    }

    static void mockKeyboardInput(int down, uint32_t scancode)
    {
        g_svpChannelTest->keyboardInputCalled = true;
        g_svpChannelTest->downH = down;
        g_svpChannelTest->scancodeH = scancode;
    }

    static void mockMouseInput(int x, int y, int buttons)
    {
        g_svpChannelTest->mouseInputCalled = true;
        g_svpChannelTest->xH = x;
        g_svpChannelTest->yH = y;
        g_svpChannelTest->buttonsH = buttons;
    }

    static void onClientLogon(int id)
    {
        g_svpChannelTest->clientLogonCalled = true;
    }

    static void onClientLogout(int id)
    {
        g_svpChannelTest->clientLogoutCalled = true;
    }

    void hookMainChannel(struct main_channel *channel)
    {
        channel->client_logon = onClientLogon;
        channel->client_logout = onClientLogout;
    }

    static void onVideoModeSet(int w, int h, int depth)
    {
        g_svpChannelTest->videoModeSetCalled = true;
        g_svpChannelTest->wH = w;
        g_svpChannelTest->hH = h;
        g_svpChannelTest->depthH = depth;
    }

    void hookDisplayChannel(struct display_channel *channel)
    {
        channel->video_modeset = onVideoModeSet;
    }

    bool checkVideoModeSet()
    {
        return wH == video_modeset.mode.w
                && hH == video_modeset.mode.h
                && depthH == video_modeset.mode.depth;
    }

    void hookInputChannel(struct input_channel *channel)
    {
        channel->keyboard_input = mockKeyboardInput;
        channel->mouse_input = mockMouseInput;
    }

    bool checkKeyboardEvent()
    {
        if (keyboard.event.down != downH || keyboard.event.scancode != scancodeH)
            return false;
        return true;
    }

    bool checkMouseEvent()
    {
        if (mouse.event.x != xH || mouse.event.y != yH || mouse.event.buttons != buttonsH)
            return false;
        return true;
    }

    void initCursor()
    {
        cursor.x_hot = 1;
        cursor.y_hot = 2;
        cursor.w = 16;
        cursor.h = 16;
        cursor.mask.format = SPF_MONO;
        cursor.mask.x = 0;
        cursor.mask.y = 0;
        cursor.mask.w = 16;
        cursor.mask.h = 16;
        cursor.mask.size = 32;
        cursor.mask.bits = (uint8_t *)malloc(cursor.mask.size);
        memset(cursor.mask.bits, 0x80, cursor.mask.size);
        cursor.pixmap.format = SPF_RGB24;
        cursor.pixmap.x = 0;
        cursor.pixmap.y = 0;
        cursor.pixmap.w = 16;
        cursor.pixmap.h = 16;
        cursor.pixmap.size = 16 * 16 * 3;
        cursor.pixmap.bits = (uint8_t *)malloc(cursor.pixmap.size);
        memset(cursor.pixmap.bits, 0x80, cursor.pixmap.size);
    }

    bool checkCursorUpdate(struct out_cmd *cmd)
    {
        if (cmd->type != SC_CURSOR || cmd->size != sizeof(struct pac_hdr) + sizeof(struct pac_cursor)
                + cursor.mask.size + cursor.pixmap.size) {
            GTEST_LOG_(ERROR) << "bad size, expect"
                              << sizeof(struct pac_hdr) + sizeof(struct pac_cursor)
                                 + cursor.mask.size + cursor.pixmap.size
                              << "got" << cmd->size;
            return false;
        }
        struct pac_hdr *ahdr = (struct pac_hdr *)cmd->data;
        struct pac_cursor *c = (struct pac_cursor *)(cmd->data + sizeof(*ahdr));
        uint8_t *mask = cmd->data + sizeof(*ahdr) + sizeof(*c);
        uint8_t *data = mask + c->mask_len;
        if (ahdr->c_magic != SVP_MAGIC || ahdr->c_cmd != SC_CURSOR
                || ahdr->c_size != cmd->size - sizeof(struct pac_hdr)) {
            GTEST_LOG_(ERROR) << "bad header size, expect"
                              << sizeof(struct pac_cursor) + cursor.mask.size + cursor.pixmap.size
                              << "got" << ahdr->c_size;
            return false;
        }
        if (c->x_hot != cursor.x_hot || c->y_hot != cursor.y_hot
                || c->w != cursor.w || c->h != cursor.h
                || c->mask_len != cursor.mask.size
                || c->data_len != cursor.pixmap.size) {
            GTEST_LOG_(ERROR) << "cursor coord mismatch";
            return false;
        }
        if (memcmp(mask, cursor.mask.bits, c->mask_len)) {
            GTEST_LOG_(ERROR) << "cursor mask data mismatch";
            return false;
        }
        if (memcmp(data, cursor.pixmap.bits, c->data_len)) {
            GTEST_LOG_(ERROR) << "cursor pixmap data mismatch";
            return false;
        }
        return true;
    }

    struct svp_server *srv;
    vector<struct out_cmd> sentCmdH;

    struct {
        struct pac_hdr hdr;
        struct pac_main_channel_init init;
    }__attribute__((packed)) main_channel_init;
    struct {
        struct pac_hdr hdr;
        struct pac_channel_init init;
    }__attribute__((packed)) channel_init;
    struct {
        struct pac_hdr hdr;
        struct pac_keyboard_event event;
    }__attribute__((packed)) keyboard;
    struct {
        struct pac_hdr hdr;
        struct pac_mouse_event event;
    }__attribute__((packed)) mouse;
    struct {
        struct pac_hdr hdr;
        struct pac_video_modeset mode;
    }__attribute__((packed)) video_modeset;
    struct svp_solid_rect s_rect;
    struct svp_cursor cursor;
    bool keyboardInputCalled;
    bool mouseInputCalled;
    int downH;
    uint32_t scancodeH;
    int xH;
    int yH;
    int buttonsH;
    bool clientLogonCalled;
    bool clientLogoutCalled;
    bool videoModeSetCalled;
    int wH;
    int hH;
    int depthH;
};

TEST_F(SvpChannelTest, MainChannel_CreateDestroy)
{
    ASSERT_TRUE(srv->main_channel);
    ASSERT_FALSE(bindablePort(srv->port + SH_MAIN));
    srv->main_channel_destroy(srv, srv->main_channel);
    ASSERT_TRUE(bindablePort(srv->port + SH_MAIN));
    ASSERT_FALSE(srv->main_channel);    
}

TEST_F(SvpChannelTest, MainChannel_Init)
{
    struct out_cmd cmd;
    struct main_channel *channel = srv->main_channel;
    struct client *c = channel->base.create_client(&channel->base);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    main_channel_init.hdr = {SVP_MAGIC, SC_MAIN_CHANNEL_INIT, sizeof(main_channel_init.init)};
    main_channel_init.init = {1, 10, 100};
    cmd = {SC_MAIN_CHANNEL_INIT, (uint8_t *)&main_channel_init, sizeof(main_channel_init)};
    c->recv_cmd(c, &cmd);
    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkChannelList(&sentCmdH[0]));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->main_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, MainChannel_ClientLogonLogout)
{
    struct main_channel *channel = srv->main_channel;
    struct client *c = channel->base.create_client(&channel->base);
    hookMainChannel(channel);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    ASSERT_TRUE(clientLogonCalled);
    channel->base.disconnect(&channel->base, c);
    ASSERT_TRUE(clientLogoutCalled);
    channel->base.destroy_client(&channel->base, c);
    srv->main_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, DisplayChannel_CreateDestroy)
{
    ASSERT_TRUE(srv->display_channel);
    ASSERT_FALSE(bindablePort(srv->port + SH_DISPLAY));
    srv->display_channel_destroy(srv, srv->display_channel);
    ASSERT_FALSE(srv->display_channel);
    ASSERT_TRUE(bindablePort(srv->port + SH_DISPLAY));
}

TEST_F(SvpChannelTest, DisplayChannel_Init)
{
    struct out_cmd cmd;
    struct display_channel *channel = srv->display_channel_create(srv);
    struct client *c = channel->base.create_client(&channel->base);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    channel_init.hdr = {SVP_MAGIC, SC_CHANNEL_INIT, sizeof(channel_init.init)};
    channel_init.init = {SH_DISPLAY, 10, 0xffffffff};
    cmd = {SC_CHANNEL_INIT, (uint8_t *)&channel_init, sizeof(channel_init)};
    c->recv_cmd(c, &cmd);
    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkChannelReady(&sentCmdH[0]));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->display_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, DisplayChannel_VideoModeSet)
{
    struct display_channel *channel = srv->display_channel;
    struct client *c = channel->base.create_client(&channel->base);
    struct out_cmd cmd;
    initRect(&s_rect);
    hookDisplayChannel(channel);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);

    video_modeset.hdr = {SVP_MAGIC, SC_VIDEO_MODESET, sizeof(video_modeset.mode)};
    video_modeset.mode = {800, 600, 32};
    cmd = {SC_VIDEO_MODESET, (uint8_t *)&video_modeset, sizeof(video_modeset)};
    c->recv_cmd(c, &cmd);
    ASSERT_TRUE(videoModeSetCalled);
    ASSERT_TRUE(checkVideoModeSet());

    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->display_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, DisplayChannel_ScreenResize)
{
    struct display_channel *channel = srv->display_channel;
    struct client *c = channel->base.create_client(&channel->base);
    initRect(&s_rect);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    channel->screen_resize(channel, 800, 600);
    usleep(50 * 1000);
    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkScreenResize(&sentCmdH[0], 800, 600));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->display_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, DisplayChannel_GraphicUpdate)
{
    struct display_channel *channel = srv->display_channel;
    struct client *c = channel->base.create_client(&channel->base);
    struct svp_solid_rect *rect = (struct svp_solid_rect *)malloc(sizeof(*rect));
    struct in_cmd cmd = {SC_SOLID_RECT, (uint8_t *)rect, sizeof(*rect)};

    initRect(&s_rect);
    *rect = s_rect;
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    channel->graphic_update(channel, &cmd);
    usleep(50 * 1000);
    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkGraphicUpdate(&sentCmdH[0], &s_rect));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->display_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, CursorChannel_CreateDestroy)
{
    ASSERT_TRUE(srv->cursor_channel);
    ASSERT_FALSE(bindablePort(srv->port + SH_CURSOR));
    srv->cursor_channel_destroy(srv, srv->cursor_channel);
    ASSERT_FALSE(srv->cursor_channel);
    ASSERT_TRUE(bindablePort(srv->port + SH_CURSOR));
}

TEST_F(SvpChannelTest, CursorChannel_Init)
{
    struct out_cmd cmd;
    struct cursor_channel *channel = srv->cursor_channel;
    struct client *c = channel->base.create_client(&channel->base);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    channel_init.hdr = {SVP_MAGIC, SC_CHANNEL_INIT, sizeof(channel_init.init)};
    channel_init.init = {SH_CURSOR, 10, 0xffffffff};
    cmd = {SC_CHANNEL_INIT, (uint8_t *)&channel_init, sizeof(channel_init)};
    c->recv_cmd(c, &cmd);
    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkChannelReady(&sentCmdH[0]));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->cursor_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, CursorChannel_CursorUpdate)
{
    struct cursor_channel *channel = srv->cursor_channel;
    struct client *c = channel->base.create_client(&channel->base);
    struct svp_cursor *cursor2 = (struct svp_cursor *)malloc(sizeof(*cursor2));

    initCursor();
    *cursor2 = cursor;
    cursor2->mask.bits = (uint8_t *)malloc(cursor.mask.size);
    memcpy(cursor2->mask.bits, cursor.mask.bits, cursor.mask.size);
    cursor2->pixmap.bits = (uint8_t *)malloc(cursor.pixmap.size);
    memcpy(cursor2->pixmap.bits, cursor.pixmap.bits, cursor.pixmap.size);

    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    channel->cursor_update(channel, cursor2);
    usleep(50 * 1000);

    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkCursorUpdate(&sentCmdH[0]));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->cursor_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, InputChannel_CreateDestroy)
{
    ASSERT_TRUE(srv->input_channel);
    ASSERT_FALSE(bindablePort(srv->port + SH_INPUT));
    srv->input_channel_destroy(srv, srv->input_channel);
    ASSERT_TRUE(bindablePort(srv->port + SH_INPUT));
    ASSERT_FALSE(srv->input_channel);
}

TEST_F(SvpChannelTest, InputChannel_Init)
{
    struct out_cmd cmd;
    struct input_channel *channel = srv->input_channel;
    struct client *c = channel->base.create_client(&channel->base);
    hookSendCmd(c);
    channel->base.connect(&channel->base, c);
    channel_init.hdr = {SVP_MAGIC, SC_CHANNEL_INIT, sizeof(channel_init.init)};
    channel_init.init = {SH_INPUT, 10, 0xffffffff};
    cmd = {SC_CHANNEL_INIT, (uint8_t *)&channel_init, sizeof(channel_init)};
    c->recv_cmd(c, &cmd);
    ASSERT_EQ(1, sentCmdH.size());
    ASSERT_TRUE(checkChannelReady(&sentCmdH[0]));
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->input_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, InputChannel_KeyboardMessage)
{
    struct out_cmd cmd;
    struct input_channel *channel = srv->input_channel;
    struct client *c = channel->base.create_client(&channel->base);
    hookInputChannel(channel);
    channel->base.connect(&channel->base, c);
    keyboard.hdr = {SVP_MAGIC, SC_KEYBOARD_EVENT, sizeof(keyboard.event)};
    keyboard.event = {1, 12};
    cmd = {SC_KEYBOARD_EVENT, (uint8_t *)&keyboard, sizeof(keyboard)};
    c->recv_cmd(c, &cmd);
    ASSERT_TRUE(keyboardInputCalled);
    ASSERT_TRUE(checkKeyboardEvent());
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->input_channel_destroy(srv, channel);
}

TEST_F(SvpChannelTest, InputChannel_MouseMessage)
{
    struct out_cmd cmd;
    struct input_channel *channel = srv->input_channel;
    struct client *c = channel->base.create_client(&channel->base);
    hookInputChannel(channel);
    channel->base.connect(&channel->base, c);
    mouse.hdr = {SVP_MAGIC, SC_MOUSE_EVENT, sizeof(mouse.event)};
    mouse.event = {100, 200, 1};
    cmd = {SC_MOUSE_EVENT, (uint8_t *)&mouse, sizeof(mouse)};
    c->recv_cmd(c, &cmd);
    ASSERT_TRUE(mouseInputCalled);
    ASSERT_TRUE(checkMouseEvent());
    channel->base.disconnect(&channel->base, c);
    channel->base.destroy_client(&channel->base, c);
    srv->input_channel_destroy(srv, channel);
}

class DisplayWorkerTest;
static DisplayWorkerTest *g_displayWorkerTest = 0;
class DisplayWorkerTest : public ::testing::Test
{
public:
    void SetUp()
    {
        g_displayWorkerTest = this;
        outq = outq_create();
        worker = display_worker_create(outq);
        outputUpdateCalled = false;
    }

    void TearDown()
    {
        display_worker_destroy(worker);
        outq_destroy(outq);
    }

    static void mockOutputUpdate(struct display_worker *w)
    {
        DisplayWorkerTest *me = (DisplayWorkerTest *)w->output_update_arg;
        me->outputUpdateCalled = true;
    }

    void hookWorker()
    {
        worker->output_update = mockOutputUpdate;
        worker->output_update_arg = this;
    }

    struct svp_solid_rect s_rect;
    struct display_worker *worker;
    struct outqueue *outq;
    bool outputUpdateCalled;
};

TEST_F(DisplayWorkerTest, StartStop)
{
    worker->start(worker);
    usleep(50 * 1000);
    worker->stop(worker);
}

TEST_F(DisplayWorkerTest, GraphicUpdate)
{
    struct svp_solid_rect *rect = (struct svp_solid_rect *)malloc(sizeof(*rect));
    struct in_cmd in = {SC_SOLID_RECT, (uint8_t *)rect, sizeof(*rect)};
    struct out_cmd out;

    initRect(&s_rect);
    *rect = s_rect;

    hookWorker();
    worker->start(worker);
    worker->graphic_update(worker, &in);
    usleep(50 * 1000);
    worker->stop(worker);

    ASSERT_TRUE(outputUpdateCalled);
    ASSERT_EQ(1, outq_size(outq));
    outq_pop(outq, &out);
    ASSERT_TRUE(checkGraphicUpdate(&out, &s_rect));
}

class SvpInterfaceTest;
static SvpInterfaceTest *g_svpInterfaceTest = 0;
class SvpInterfaceTest : public ::testing::Test
{
public:
    void SetUp()
    {
        g_svpInterfaceTest = this;
        interfaceGot = (svp_get_interface(&bops, &fops) == 0);
        initRect(&rect);
        initCursor(&cursor);
        initBackend(&bops);
    }

    void TearDown()
    {

    }

    void initBackend(struct svp_backend_operations *bops)
    {
        bops->client_logon = onClientLogon;
        bops->client_logout = onClientLogout;
        bops->keyboard_input = onKeyboardInput;
        bops->mouse_input = onMouseInput;
        bops->video_modeset = onVideoModeSet;
        clientLogonCalled = false;
        clientLogoutCalled = false;
        keyboardInputCalled = false;
        mouseInputCalled = false;
        videoModeSetCalled = false;
    }

    static void onClientLogon(struct svp_backend_operations *, int id)
    {
        g_svpInterfaceTest->clientLogonCalled = true;
    }

    static void onClientLogout(struct svp_backend_operations *, int id)
    {
        g_svpInterfaceTest->clientLogoutCalled = true;
    }

    static void onKeyboardInput(struct svp_backend_operations *, int pressed_down, uint32_t scancode)
    {
        g_svpInterfaceTest->keyboardInputCalled = true;
    }

    static void onMouseInput(struct svp_backend_operations *, int x, int y, int button_mask)
    {
        g_svpInterfaceTest->mouseInputCalled = true;
    }

    static void onVideoModeSet(struct svp_backend_operations *, int w, int h, int depth)
    {
        g_svpInterfaceTest->videoModeSetCalled = true;
    }

    void initCursor(struct svp_cursor *cursor)
    {
        cursor->x_hot = 1;
        cursor->y_hot = 1;
        cursor->w = 16;
        cursor->h = 16;
        cursor->mask.format = SPF_MONO;
        cursor->mask.w = 16;
        cursor->mask.h = 16;
        cursor->mask.size = 2 * 16;
        cursor->mask.bits = (uint8_t *)malloc(cursor->mask.size);
        cursor->pixmap.format = SPF_RGB24;
        cursor->pixmap.w = 16;
        cursor->pixmap.h = 16;
        cursor->pixmap.size = 16 * 16 * 3;
        cursor->pixmap.bits = (uint8_t *)malloc(cursor->pixmap.size);
    }

    static void mockScreenResize(struct display_channel *channel, int w, int h)
    {
        g_svpInterfaceTest->screenResizeCalled = true;
    }

    static void mockGraphicUpdate(struct display_channel *channel, struct in_cmd *cmd)
    {
        g_svpInterfaceTest->graphicUpdateCalled = true;
    }

    static void mockCursorUpdate(struct cursor_channel *channel, struct svp_cursor *cursor)
    {
        g_svpInterfaceTest->cursorUpdateCalled = true;
    }

    void hookServer(struct svp_server *srv)
    {
        if (!srv || !srv->display_channel || !srv->cursor_channel)
            return;
        srv->display_channel->screen_resize = mockScreenResize;
        srv->display_channel->graphic_update = mockGraphicUpdate;
        srv->cursor_channel->cursor_update = mockCursorUpdate;
    }

    void mockClientLogon(struct svp_server *srv)
    {
        if (!srv || !srv->main_channel || !srv->main_channel->client_logon)
            return;
        srv->main_channel->client_logon(1);
    }

    void mockClientLogout(struct svp_server *srv)
    {
        if (!srv || !srv->main_channel || !srv->main_channel->client_logout)
            return;
        srv->main_channel->client_logout(1);
    }

    void mockKeyboardInput(struct svp_server *srv)
    {
        if (!srv || !srv->input_channel || !srv->input_channel->keyboard_input)
            return;
        srv->input_channel->keyboard_input(1, 123);
    }

    void mockMouseInput(struct svp_server *srv)
    {
        if (!srv || !srv->input_channel || !srv->input_channel->mouse_input)
            return;
        srv->input_channel->mouse_input(1, 123, SMB_LEFT);
    }

    void mockVideoModeSet(struct svp_server *srv)
    {
        if (!srv || !srv->display_channel || !srv->display_channel->video_modeset)
            return;
        srv->display_channel->video_modeset(800, 600, 32);
    }

    struct svp_backend_operations bops;
    struct svp_frontend_operations fops;
    bool interfaceGot;
    struct svp_solid_rect rect;
    struct svp_cursor cursor;

    bool screenResizeCalled;
    bool graphicUpdateCalled;
    bool cursorUpdateCalled;

    bool clientLogonCalled;
    bool clientLogoutCalled;
    bool keyboardInputCalled;
    bool mouseInputCalled;
    bool videoModeSetCalled;
};

TEST_F(SvpInterfaceTest, Get)
{
    ASSERT_TRUE(interfaceGot);
}

TEST_F(SvpInterfaceTest, InitFini)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    ASSERT_TRUE(fops.priv);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, ScreenResize)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    hookServer((struct svp_server *)fops.priv);
    fops.screen_resize(&fops, 800, 600);
    ASSERT_TRUE(screenResizeCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, GraphicUpdate)
{
    struct in_cmd cmd = {SC_SOLID_RECT, (uint8_t *)&rect, sizeof(rect)};
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    hookServer((struct svp_server *)fops.priv);
    fops.graphic_update(&fops, &cmd);
    ASSERT_TRUE(graphicUpdateCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, CursorUpdate)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    hookServer((struct svp_server *)fops.priv);
    fops.cursor_update(&fops, &cursor);
    ASSERT_TRUE(cursorUpdateCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, KeyboardInput)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    mockKeyboardInput((struct svp_server *)fops.priv);
    ASSERT_TRUE(keyboardInputCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, MouseInput)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    mockMouseInput((struct svp_server *)fops.priv);
    ASSERT_TRUE(mouseInputCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, SetVideoMode)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    mockVideoModeSet((struct svp_server *)fops.priv);
    ASSERT_TRUE(videoModeSetCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, ClientLogon)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    mockClientLogon((struct svp_server *)fops.priv);
    ASSERT_TRUE(clientLogonCalled);
    fops.fini(&fops);
}

TEST_F(SvpInterfaceTest, ClientLogout)
{
    fops.init(&fops, "0.0.0.0", TEST_PORT);
    mockClientLogout((struct svp_server *)fops.priv);
    ASSERT_TRUE(clientLogoutCalled);
    fops.fini(&fops);
}
