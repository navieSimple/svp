#include "LZ4Decoder.h"
#include "svp.h"
#include "net_packet.h"
#include "lz4/lz4.h"
#include <QDebug>

#define DEC_NAME "LZ4"
#define DEC_BUFSIZ (16 * 1024 * 1024)

LZ4Decoder::LZ4Decoder()
    : m_buf(0)
{

}


LZ4Decoder::~LZ4Decoder()
{

}


int LZ4Decoder::id() const
{
    return SX_LZ4;
}


QString LZ4Decoder::name() const
{
    return DEC_NAME;
}


int LZ4Decoder::init()
{
    m_buf = (char *)malloc(DEC_BUFSIZ);
    return 0;
}


void LZ4Decoder::fini()
{
    free(m_buf);
}


bool LZ4Decoder::decode(pac_bitmap *in, QByteArray &out)
{
    char *src;
    pac_bitmap *hdr;
    char *dst;
    int n;

    if (in->codec != SX_LZ4)
        return false;

    src = (char *)in + sizeof(*in);
    hdr = (pac_bitmap *)m_buf;
    dst = m_buf + sizeof(*hdr);
    n = LZ4_decompress_safe(src, dst, in->size, DEC_BUFSIZ);
    if (n < 0 || n >= DEC_BUFSIZ)
        return false;
    *hdr = *in;
    hdr->codec = SX_RAW;
    hdr->size = n;
    out.setRawData(m_buf, sizeof(*hdr) + n);
    return true;
}
