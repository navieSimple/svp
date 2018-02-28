#ifndef ENCODER_H
#define ENCODER_H

#include "svp.h"
#include "net_packet.h"
#include "encoder_global.h"
#include "graphic.h"

struct graphic_option;
struct encoder_base
{
    // interfaces
    int (*init)(struct encoder_base *);
    void (*fini)(struct encoder_base *);
    void (*configure)(struct encoder_base *, struct graphic_option *opt);
    void (*encode)(struct encoder_base *, struct pac_bitmap *bitmap);

    int id;
    char name[32];
    struct list_head link;
};

struct encoder_base *encoder_load(const char *libpath);

#endif // ENCODER_H
