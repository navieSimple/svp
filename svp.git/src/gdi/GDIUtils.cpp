#include "GDIUtils.h"
#include "client/Codec.h"
#include <QDebug>
#include <QPainter>
#include <QUuid>
#include <QPixmap>
#include <QBitmap>
#include <stdint.h>

namespace GDIUtils
{

QString CacheId(pac_bitmap_hash_t hash)
{
    return QUuid((hash[0] << 24) + (hash[1] << 16) + (hash[2] << 8) + hash[3],
            (hash[4] << 8) + hash[5], (hash[6] << 8) + hash[7],
            hash[8], hash[9], hash[10], hash[11],
            hash[12], hash[13], hash[14], hash[15]).toString();
}

QImage makeMask(int w, int h, uint8_t *data)
{
    QImage img(w, h, QImage::Format_Mono);
    uchar *sptr = data;
    uchar *dptr = img.bits();
    int spitch = (w + 7) / 8;
    int dpitch = (w + 31) / 32 * 4;
    img.fill(Qt::color0);
    for (int i = 0; i < h; i++) {
        memcpy(dptr, sptr, spitch);
        sptr += spitch;
        dptr += dpitch;
    }
    return img;
}

static bool isFullyTransparent(const QImage &mask)
{
    for (int i = 0; i < mask.height(); i++)
        for (int j = 0; j < mask.width(); j++) {
            if (mask.pixel(j, i) != 0xffffffff)
                return false;
        }
    return true;
}

Cursor MakeCursor(pac_cursor *cursor)
{
    uint8_t *data = (uint8_t *)cursor + sizeof(*cursor);
    uint8_t *copy;
    bool useBitmap = false;
    QImage mask = makeMask(cursor->w, cursor->h, data);
    copy = (uint8_t *)malloc(cursor->data_len);
    memcpy(copy, data + cursor->mask_len, cursor->data_len);
    QImage image(copy, cursor->w, cursor->h, QImage::Format_RGB888, free, copy);
    if (isFullyTransparent(mask)) {
        image.invertPixels();
        useBitmap = true;
    }
    return Cursor(image, mask, cursor->x_hot, cursor->y_hot, useBitmap);
}

void SolidBlt_PATINVERT(QPainter *p, QImage &dest, const QRect &rect, const QColor &color)
{
    QRgb pixel;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            pixel = dest.pixel(rect.x() + j, rect.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, pixel ^ color.rgb());
        }
}

void SolidBlt_Pn(QPainter *p, QImage &dest, const QRect &rect, const QColor &color)
{
    QRgb pixel;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            pixel = dest.pixel(rect.x() + j, rect.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, ~color.rgb());
        }
}

void SolidBlt(QPainter *p, QImage &dest, const QRect &rect, const QColor &color, uint8_t rop)
{
    QVector<QRect> rects;
    if (p->hasClipping())
        rects = p->clipRegion().intersected(rect).rects();
    else
        rects.append(rect);
    foreach (const QRect &r, rects) {
        switch (rop) {
        case GDI_PATINVERT:
            SolidBlt_PATINVERT(p, dest, r, color);
            break;
        case GDI_Pn:
            SolidBlt_Pn(p, dest, r, color);
            break;
        default:
            qDebug() << "not implemented solid_blt rop" << Rop3Name(rop);
        }
    }
}

static inline bool patternSet(int x, int y, const uint8_t pattern[8])
{
    return (pattern[y & 7] & (1 << (x & 7))) != 0;
}

void PatBlt_PATINVERT(QPainter *p, QImage &dest, const QRect &rect, const uint8_t *pattern,
                      const QColor &fgColor, const QColor &bgColor, QPoint origin)
{
    QRgb pixel;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            pixel = dest.pixel(rect.x() + j, rect.y() + i);
            if (patternSet(j + origin.x(), i + origin.y(), pattern))
                dest.setPixel(rect.x() + j, rect.y() + i, pixel ^ fgColor.rgb());
            else
                dest.setPixel(rect.x() + j, rect.y() + i, pixel ^ bgColor.rgb());
        }
}

void PatBlt_PATCOPY(QPainter *p, QImage &dest, const QRect &rect, const uint8_t *pattern,
                    const QColor &fgColor, const QColor &bgColor, QPoint origin)
{
    QRgb pixel;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            pixel = dest.pixel(rect.x() + j, rect.y() + i);
            if (patternSet(j + origin.x(), i + origin.y(), pattern))
                dest.setPixel(rect.x() + j, rect.y() + i, fgColor.rgb());
            else
                dest.setPixel(rect.x() + j, rect.y() + i, bgColor.rgb());
        }
}

void PatBlt(QPainter *p, QImage &dest, const QRect &rect, const uint8_t *pattern,
            const QColor &fgColor, const QColor &bgColor, const QPoint &origin, uint8_t rop)
{
    switch (rop) {
    case GDI_PATINVERT:
        PatBlt_PATINVERT(p, dest, rect, pattern, fgColor, bgColor, origin);
        break;
    case GDI_PATCOPY:
        PatBlt_PATCOPY(p, dest, rect, pattern, fgColor, bgColor, origin);
        break;
    default:
        qDebug() << "not implemented pat_blt rop" << Rop3Name(rop);
    }
}

void DstBlt_DSTINVERT(QPainter *p, QImage &dest, const QRect &rect)
{
    QRgb pixel;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            pixel = dest.pixel(rect.x() + j, rect.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, ~pixel);
        }
}

void DstBlt(QPainter *p, QImage &dest, const QRect &rect, uint8_t rop)
{
    QVector<QRect> rects;
    if (p->hasClipping())
        rects = p->clipRegion().intersected(rect).rects();
    else
        rects.append(rect);
    foreach (const QRect &r, rects) {
        switch (rop) {
        case GDI_BLACKNESS:
            p->fillRect(r, Qt::black);
            break;
        case GDI_WHITENESS:
            p->fillRect(r, Qt::white);
            break;
        case GDI_DSTINVERT:
            DstBlt_DSTINVERT(p, dest, r);
            break;
        case GDI_D:
            // nop
            break;
        default:
            qDebug() << "not implemented dst_blt rop" << Rop3Name(rop);
        }
    }
}

void MemBlt_SRCINVERT(QPainter *p, QImage &dest, const QRect &rect,
                      const QImage &src, const QPoint &origin)
{
    QRgb p1, p2;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            p1 = dest.pixel(rect.x() + j, rect.y() + i);
            p2 = src.pixel(origin.x() + j, origin.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, p1 ^ p2);
        }
}

void MemBlt_SRCPAINT(QPainter *p, QImage &dest, const QRect &rect,
                     const QImage &src, const QPoint &origin)
{
    QRgb p1, p2;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            p1 = dest.pixel(rect.x() + j, rect.y() + i);
            p2 = src.pixel(origin.x() + j, origin.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, p1 | p2);
        }
}

void MemBlt_SRCAND(QPainter *p, QImage &dest, const QRect &rect,
                   const QImage &src, const QPoint &origin)
{
    QRgb p1, p2;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            p1 = dest.pixel(rect.x() + j, rect.y() + i);
            p2 = src.pixel(origin.x() + j, origin.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, p1 & p2);
        }
}

void MemBlt_Pn(QPainter *p, QImage &dest, const QRect &rect,
               const QImage &src, const QPoint &origin)
{
    QRgb p2;
    /*
    const int stride = dest.bytesPerLine();
    const int depth = dest.depth();
    uint8_5 *ptr = dest.bits() + (rect.y() - 1) * stride + (rect.x() - 1) * depth;
    */
    for (int i = 0; i < rect.height(); i++)
        for (int j = 0; j < rect.width(); j++) {
            p2 = src.pixel(origin.x() + j, origin.y() + i);
            dest.setPixel(rect.x() + j, rect.y() + i, ~p2);
        }
}

void MemBlt(QPainter *p, QImage &dest, const QRect &rect, const QImage &src, const QPoint &origin, uint8_t rop)
{
    QVector<QRect> rects;
    if (p->hasClipping())
        rects = p->clipRegion().intersected(rect).rects();
    else
        rects.append(rect);
    foreach (const QRect &r, rects) {
        switch (rop) {
        case GDI_SRCCOPY:
            p->drawImage(r.x(), r.y(), src, origin.x(), origin.y(), r.width(), r.height());
            break;
        case GDI_SRCINVERT:
            MemBlt_SRCINVERT(p, dest, r, src, origin);
            break;
        case GDI_SRCPAINT:
            MemBlt_SRCPAINT(p, dest, r, src, origin);
            break;
        case GDI_SRCAND:
            MemBlt_SRCAND(p, dest, r, src, origin);
            break;
        case GDI_Pn:
            MemBlt_Pn(p, dest, r, src, origin);
            break;
        default:
            qDebug() << "not implemented mem_blt rop" << Rop3Name(rop);
        }
    }
}

QImage MakeGlyph(pac_glyph *glyph, QRgb color)
{
    QImage img(glyph->w, glyph->h, QImage::Format_ARGB32_Premultiplied);
    uchar *sptr = (uchar *)glyph + sizeof(*glyph);
    uchar *dptr = img.bits();
    int spitch = (glyph->w + 7) / 8;
    int dpitch = glyph->w * 4;
    img.fill(Qt::transparent);
    for (int i = 0; i < glyph->h; i++) {
        for (int j = 0; j < glyph->w; j++)
            if (sptr[j / 8] & (1 << (7 - j % 8)))
                *(uint32_t *)&dptr[j * 4] = 0xff000000 | color;
        sptr += spitch;
        dptr += dpitch;
    }
    return img;
}

static QImage makeSimpleImage(pac_bitmap *bitmap)
{
    QImage::Format format;
    uchar *src = (uchar *)bitmap + sizeof(*bitmap);
    int bytesPerLine;

    if (bitmap->codec != SX_RAW) {
        qDebug() << Q_FUNC_INFO << "not a raw image";
        return QImage();
    }

    switch (bitmap->format) {
    case SPF_RGB32:
        format = QImage::Format_RGB32;
        bytesPerLine = bitmap->w * 4;
        break;
    case SPF_BGR24:
    case SPF_RGB24:
        format = QImage::Format_RGB888;
        bytesPerLine = bitmap->w * 3;
        break;
    case SPF_RGB16:
        format = QImage::Format_RGB16;
        bytesPerLine = bitmap->w * 2;
        break;
    case SPF_RGB8:
        qWarning() << "256-color picture not supported now";
        format = QImage::Format_Mono;
        bytesPerLine = bitmap->w;
        break;
    case SPF_MONO:
        format = QImage::Format_Mono;
        bytesPerLine = (bitmap->w + 7) / 8;
        break;
    default:
        qWarning() << "not supported format" << bitmap->format;
    }
    if (bitmap->format == SPF_BGR24)
        return QImage(src, bitmap->w, bitmap->h, bytesPerLine, format).rgbSwapped();
    else
        return QImage(src, bitmap->w, bitmap->h, bytesPerLine, format);
}

QImage MakeImage(pac_bitmap *bitmap)
{
    QByteArray buf;
    pac_bitmap *decoded;
    if (bitmap->codec == SX_RAW)
        return makeSimpleImage(bitmap);
    if (!Codec::instance()->decode(bitmap, buf)) {
        qDebug() << QString("decode error, codec type %1").arg(bitmap->codec);
        return QImage();
    }
    decoded = (pac_bitmap *)buf.data();
    return makeSimpleImage(decoded).copy();
}

QImage MakeText(pac_text *text)
{
    struct pac_glyph *glyph;
    uint8_t *ptr = (uint8_t *)text + sizeof(*text);
    QImage img(text->w_bg, text->h_bg, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&img);
    img.fill(Qt::transparent);
    for (int i = 0; i < text->glyphs; i++) {
        glyph = (struct pac_glyph *)ptr;
        QImage subimg = MakeGlyph(glyph, text->rgb_fg);
        painter.drawImage(glyph->x + glyph->x_origin - text->x_bg,
                          glyph->y + glyph->y_origin - text->y_bg, subimg);
        ptr += glyph->size;
    }
    painter.end();
    return img;
}

} // namespace GDIUtils
