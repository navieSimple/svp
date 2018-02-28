#include "codec.h"
#include "encoder.h"
#include "graphic.h"
#include <glob.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CODEC_DIR "/usr/lib/svp/codec"

int codec_init(struct codec *);
void codec_fini(struct codec *);
void codec_configure(struct codec *, struct graphic_option *opt);
void codec_encode(struct codec *, struct pac_bitmap *bitmap);
struct encoder_base *codec_encoder_for_id(struct codec *, int id);

struct codec *codec_create()
{
    struct codec *c = (struct codec *)malloc(sizeof(*c));
    if (!c)
        return 0;
    memset(c, 0, sizeof(*c));
    c->init = codec_init;
    c->fini = codec_fini;
    c->configure = codec_configure;
    c->encode = codec_encode;
    c->encoder_for_id = codec_encoder_for_id;
    INIT_LIST_HEAD(&c->encoder_list);
    return c;
}

void codec_destroy(struct codec *c)
{
    if (!c)
        return;
    c->fini(c);
    free(c);
}

int codec_init(struct codec *c)
{
    glob_t globbuf;
    int rc;
    int i;
    struct encoder_base *encoder;
    rc = glob(CODEC_DIR "/*.so", 0, NULL, &globbuf);
    if (rc) {
        dzlog_debug("scan dir %s error", CODEC_DIR);
        return -1;
    }
    for (i = 0; i < globbuf.gl_pathc; i++) {
        encoder = encoder_load(globbuf.gl_pathv[i]);
        if (!encoder)
            continue;
        rc = encoder->init(encoder);
        if (rc) {
            dzlog_debug("codec %s init error %d", encoder->name, rc);
            continue;
        }
        list_add_tail(&encoder->link, &c->encoder_list);
        dzlog_debug("codec %s loaded", encoder->name);
    }
    globfree(&globbuf);
    return 0;
}

void codec_fini(struct codec *c)
{
    struct encoder_base *encoder, *tmp;
    list_for_each_entry_safe(encoder, tmp, &c->encoder_list, link) {
        list_del_init(&encoder->link);
        encoder->fini(encoder);
    }
}

void codec_configure(struct codec *c, struct graphic_option *opt)
{
    struct encoder_base *encoder;
    list_for_each_entry(encoder, &c->encoder_list, link) {
        encoder->configure(encoder, opt);
    }
    c->best_encoder = 0;
    if (opt->force_codec != SX_AUTO) {
        list_for_each_entry(encoder, &c->encoder_list, link) {
            if (encoder->id == opt->force_codec) {
                dzlog_debug("encoder changed to %s", encoder->name);
                c->best_encoder = encoder;
                break;
            }
        }
    }
}

void codec_encode(struct codec *c, struct pac_bitmap *bitmap)
{
    if (!c->best_encoder)
        return;
    c->best_encoder->encode(c->best_encoder, bitmap);
}

struct encoder_base *codec_encoder_for_id(struct codec *cdc, int id)
{
    struct encoder_base *enc;
    list_for_each_entry(enc, &cdc->encoder_list, link) {
        if (enc->id == id)
            return enc;
    }
    return 0;
}
