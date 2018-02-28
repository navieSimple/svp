#include "encoder.h"
#include "lz4.h"
#include "lz4hc.h"
#include <stdlib.h>
#include <string.h>

#define ENC_ID SX_LZ4
#define ENC_NAME "LZ4"
#define ENC_BUFSIZ (16 * 1024 * 1024)

struct lz4_encoder
{
    struct encoder_base base;
    char *buf;
    int use_hc;
};

static int encoder_init(struct encoder_base *);
static void encoder_fini(struct encoder_base *);
static void encoder_configure(struct encoder_base *, struct graphic_option *opt);
static void encoder_encode(struct encoder_base *, struct pac_bitmap *bitmap);

ENCODER_PUBLIC struct encoder_base *encoder_plugin_create_encoder()
{
    struct lz4_encoder *enc = (struct lz4_encoder *)malloc(sizeof(*enc));
    if (!enc)
        return 0;
    memset(enc, 0, sizeof(*enc));
    enc->base.id = ENC_ID;
    strcpy(enc->base.name, ENC_NAME);
    enc->base.init = encoder_init;
    enc->base.fini = encoder_fini;
    enc->base.configure = encoder_configure;
    enc->base.encode = encoder_encode;
    return (struct encoder_base *)enc;
}

static int encoder_init(struct encoder_base *base)
{
    struct lz4_encoder *enc = (struct lz4_encoder *)base;
    if (enc) {
        enc->buf = (char *)malloc(ENC_BUFSIZ);
        enc->use_hc = 0;
    }
    return 0;
}

static void encoder_fini(struct encoder_base *base)
{
    struct lz4_encoder *enc = (struct lz4_encoder *)base;
    if (enc)
        free(enc->buf);
}

static void encoder_configure(struct encoder_base *base, struct graphic_option *opt)
{
    struct lz4_encoder *enc = (struct lz4_encoder *)base;
    if (!enc || !opt)
        return;
    enc->use_hc = opt->lz4_hc;
}

static void encoder_encode(struct encoder_base *base, struct pac_bitmap *bitmap)
{
    struct lz4_encoder *enc = (struct lz4_encoder *)base;
    char *data = (char *)bitmap + sizeof(*bitmap);
    int size;
    if (!enc || !bitmap)
        return;
    if (enc->use_hc)
        size = LZ4_compressHC(data, enc->buf, bitmap->size);
    else
        size = LZ4_compress(data, enc->buf, bitmap->size);
    if (size == 0 || size >= bitmap->size)
        return;
    memcpy(data, enc->buf, size);
    bitmap->codec = SX_LZ4;
    bitmap->size = size;
}
