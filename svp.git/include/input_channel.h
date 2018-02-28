#ifndef INPUT_CHANNEL_H
#define INPUT_CHANNEL_H

#include "channel.h"

struct input_channel
{
    struct channel_base base;
    // callbacks
    void (*keyboard_input)(int down, uint32_t scancode);
    void (*mouse_input)(int x, int y, int buttons);
};

struct svp_server;

struct input_channel *input_channel_create(struct svp_server *srv);

void input_channel_destroy(struct svp_server *srv, struct input_channel *channel);

#endif // INPUT_CHANNEL_H
