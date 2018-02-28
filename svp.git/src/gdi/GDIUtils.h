#ifndef GDIUTILS_H
#define GDIUTILS_H

#include "GDITypes.h"
#include <QByteArray>
#include <QColor>
#include <QCursor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <stdint.h>
#include <svp.h>
#include <net_packet.h>

class QPainter;
namespace GDIUtils
{
class Cursor
{
public:
    Cursor(const QImage &_image = QImage(), const QImage &_mask = QImage(), int x = 0, int y = 0,
           bool _useBitmap = false)
        : image(_image), mask(_mask), hotx(x), hoty(y), useBitmap(_useBitmap)
    {}

    QImage image;
    QImage mask;
    int hotx;
    int hoty;
    bool useBitmap;
};

static inline QString Rop3Name(uint8_t rop3)
{
    return ROP3_TABLE.value(rop3, QString("<0x%1>").arg(rop3, 0, 16));
}

static inline QString Rop2Name(uint8_t rop2)
{
    return ROP2_TABLE.value(rop2, QString("<0x%1>").arg(rop2, 0, 16));
}

QString CacheId(pac_bitmap_hash_t hash);
Cursor MakeCursor(pac_cursor *cursor);
QImage MakeGlyph(pac_glyph *glyph, QRgb color);
QImage MakeImage(pac_bitmap *bitmap);
QImage MakeText(pac_text *text);

void SolidBlt(QPainter *p, QImage &dest, const QRect &rect, const QColor &color, uint8_t rop);

void PatBlt(QPainter *p, QImage &dest, const QRect &rect, const uint8_t *pattern,
            const QColor &fgColor, const QColor &bgColor, const QPoint &origin, uint8_t rop);

void DstBlt(QPainter *p, QImage &dest, const QRect &rect, uint8_t rop);

void MemBlt(QPainter *p, QImage &dest, const QRect &rect,
            const QImage &src, const QPoint &origin, uint8_t rop);

}
Q_DECLARE_METATYPE(GDIUtils::Cursor)

#endif // GDIUTILS_H
