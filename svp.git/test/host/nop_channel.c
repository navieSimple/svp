#include <host_channel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>

int nop_open(void *priv, int flags);
void nop_close(void *priv, int channel);
int nop_read(void *priv, int channel, void *data, int size);
int nop_write(void *priv, int channel, const void *data, int size);
int nop_ioctl(void *priv, int channel, int code, void *data, int size);

struct svp_host_channel_interface g_ifce =
{
    .open = &nop_open,
    .close = &nop_close,
    .read = &nop_read,
    .write = &nop_write,
    .ioctl = &nop_ioctl,
    0
};

HOST_PLUGIN_EXPORT
int ChannelPluginInit(struct svp_host_channel_callback *callback)
{
    int rc;
    rc = callback->register_channel(callback->priv, "/svp/nop", &g_ifce);
//    dzlog_debug("plugin init ret %x", rc);
    return rc;
}

int nop_open(void *priv, int flags)
{
//    dzlog_debug("open flags %d", flags);
    return 1;
}

void nop_close(void *priv, int channel)
{
//    dzlog_debug("close channel %d", channel);
}

int nop_read(void *priv, int channel, void *data, int size)
{
//    dzlog_debug("read channel %d size %d", channel, size);
    memset(data, 0xaa, size);
    return size;
}

int nop_write(void *priv, int channel, const void *data, int size)
{
//    dzlog_debug("write channel %d size %d", channel, size);
    return size;
}

int nop_ioctl(void *priv, int channel, int code, void *data, int size)
{
    dzlog_debug("channel %d code %d size %d", channel, code, size);
    memset(data, 0xaa, size);
    return 0;
}
