#ifndef HOST_CHANNEL_H
#define HOST_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zlog/zlog.h"

#pragma pack(1)
struct svp_host_channel_interface
{
    int (*open)(void *priv, int flags);
    void (*close)(void *priv, int channel);
    int (*read)(void *priv, int channel, void *data, int size);
    int (*write)(void *priv, int channel, const void *data, int size);
    int (*ioctl)(void *priv, int channel, int code, void *data, int size);
    void *priv;
};

struct svp_host_channel_callback
{
    int (*register_channel)(void *priv, const char *channel, struct svp_host_channel_interface *cb);
    void (*set_readable)(void *priv, int avail);
    void *priv;
};
#pragma pack()

#define HOST_PLUGIN_ENTRY "ChannelPluginInit"
typedef int (*svp_host_channel_init_func)(struct svp_host_channel_callback *callback);

#ifdef __GNUC__
#define HOST_PLUGIN_EXPORT __attribute__ ((visibility("default")))
#endif

#ifdef __cplusplus
}
#endif
#endif // HOST_CHANNEL_H
