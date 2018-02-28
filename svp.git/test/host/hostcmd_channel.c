#include <host_channel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>

int hostcmd_open(void *priv, int flags);
void hostcmd_close(void *priv, int channel);
int hostcmd_read(void *priv, int channel, void *data, int size);
int hostcmd_write(void *priv, int channel, const void *data, int size);
int hostcmd_ioctl(void *priv, int channel, int code, void *data, int size);

struct hostcmd_context
{
    char cmdline[256];
    char result[65536];
    int valid;
} g_ctx = {{0}, {0}, 0};

struct svp_host_channel_interface g_ifce =
{
    .open = &hostcmd_open,
    .close = &hostcmd_close,
    .read = &hostcmd_read,
    .write = &hostcmd_write,
    .ioctl = &hostcmd_ioctl,
    &g_ctx
};

HOST_PLUGIN_EXPORT
int ChannelPluginInit(struct svp_host_channel_callback *callback)
{
    int rc;
    rc = callback->register_channel(callback->priv, "/svp/hostcmd", &g_ifce);
    dzlog_debug("plugin init ret %x", rc);
    return rc;
}

int hostcmd_open(void *priv, int flags)
{
    dzlog_debug("open flags %d", flags);
    return 1;
}

void hostcmd_close(void *priv, int channel)
{
    dzlog_debug("close channel %d", channel);
}

int hostcmd_read(void *priv, int channel, void *data, int size)
{
    dzlog_debug("read channel %d size %d", channel, size);
    struct hostcmd_context *ctx = (struct hostcmd_context *)priv;
    if (!ctx)
        return -1;
    if (!ctx->valid)
        return 0;
    FILE *f = popen(ctx->cmdline, "r");
    int n = fread(data, 1, size, f);
    pclose(f);
    return n;
}

int hostcmd_write(void *priv, int channel, const void *data, int size)
{
    dzlog_debug("write channel %d size %d", channel, size);
    struct hostcmd_context *ctx = (struct hostcmd_context *)priv;
    if (!ctx)
        return -1;
    if (strnlen((const char *)data, size) == size)
        return -1;
    snprintf(ctx->cmdline, sizeof(ctx->cmdline), "%s 2>&1", data);
    ctx->valid = 1;
    return size;
}

int hostcmd_ioctl(void *priv, int channel, int code, void *data, int size)
{
    dzlog_debug("channel %d code %d size %d", channel, code, size);
    char *s = (char *)data;
    while (size--) {
        if (islower(*s))
            *s = toupper(*s);
        else if (isupper(*s))
            *s = tolower(*s);
        s++;
    }
    return 0;
}
