#ifndef MAIN_CHANNEL_H
#define MAIN_CHANNEL_H

#include "channel.h"

struct main_channel
{
    struct channel_base base;
    // callbacks
    void (*client_logon)(int id);
    void (*client_logout)(int id);
};

struct svp_server;

struct main_channel *main_channel_create(struct svp_server *srv);

void main_channel_destroy(struct svp_server *srv, struct main_channel *channel);

#endif // MAIN_CHANNEL_H
