#include "SvpRender.h"
#include "Codec.h"
#include <QBitmap>
#include <QDebug>
#include <QDebug>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QUuid>
#include <stdlib.h>
#include <string.h>
#include <gdi/GDIUtils.h>
using namespace GDIUtils;

SvpRender::SvpRender(QObject *parent)
    : QObject(parent)
{
    connect(&m_updateTimer, SIGNAL(timeout()), SLOT(timeout()));
    m_image = QImage(800, 600, QImage::Format_RGB32);
    m_image.fill(Qt::black);
    m_painter.begin(&m_image);
    m_painter.setBackgroundMode(Qt::OpaqueMode);
    m_updateTimer.start(33);
    m_clipCount = 0;
}

SvpRender::~SvpRender()
{
    m_updateTimer.stop();
}

void SvpRender::changeScreenSize(pac_screen_resize *resize, int size)
{
    qDebug() << "resolution change" << resize->w << resize->h;
    m_painter.end();
    m_image = QImage(resize->w, resize->h, QImage::Format_RGB32);
    m_image.fill(Qt::black);
    m_painter.begin(&m_image);
    m_painter.setBackgroundMode(Qt::OpaqueMode);
    emit sizeChanged(QSize(resize->w, resize->h));
    emit contentChanged(m_image);
}

void SvpRender::changeCursor(pac_cursor *cursor, int size)
{
    m_cursor = MakeCursor(cursor);
    emit cursorChanged(m_cursor);
}

void SvpRender::drawDirtyRect(pac_bitmap *bitmap, int size)
{
    QImage img = MakeImage(bitmap);
    m_painter.drawImage(bitmap->x, bitmap->y, img);
    finishOneCommand();
}

void SvpRender::drawSolidRect(pac_solid_rect *rect, int size)
{
    m_painter.fillRect(rect->x, rect->y, rect->w, rect->h, QColor(rect->rgb));
    finishOneCommand();
}

void SvpRender::drawSolidBlt(pac_solid_blt *blt, int size)
{
    SolidBlt(&m_painter, m_image, QRect(blt->x, blt->y, blt->w, blt->h), QColor(blt->rgb), blt->rop);
    finishOneCommand();
}

void SvpRender::drawDstBlt(pac_dst_blt *blt, int size)
{
    DstBlt(&m_painter, m_image, QRect(blt->x, blt->y, blt->w, blt->h), blt->rop);
    finishOneCommand();
}

void SvpRender::drawScreenBlt(pac_screen_blt *blt, int size)
{
    QImage copy = m_image.copy(blt->x_src, blt->y_src, blt->w, blt->h);
    m_painter.drawImage(blt->x, blt->y, copy);
    finishOneCommand();
}

void SvpRender::drawPatternBltBrush(pac_pattern_blt_brush *blt, int size)
{
    PatBlt(&m_painter, m_image, QRect(blt->x, blt->y, blt->w, blt->h), blt->pattern,
           blt->rgb_fg, blt->rgb_bg, QPoint(blt->x_src, blt->y_src), blt->rop);
    finishOneCommand();
}

void SvpRender::drawMemBlt(pac_mem_blt *blt, int size)
{
    QString key = CacheId(blt->hash);
    static int i = 0;
    if (!m_bitmapCache.contains(key)) {
        qDebug() << "cache miss" << key;
        m_painter.fillRect(blt->x, blt->y, blt->w, blt->h, QColor(Qt::red));
        return;
    }
    QImage src = m_bitmapCache.value(key);
    MemBlt(&m_painter, m_image, QRect(blt->x, blt->y, blt->w, blt->h),
           src, QPoint(blt->x_src, blt->y_src), blt->rop);
    finishOneCommand();
}

void SvpRender::cacheInsertBitmap(pac_cached_bitmap *cache, int size)
{
    QString key = CacheId(cache->hash);
    pac_bitmap *bitmap = (pac_bitmap *)((uint8_t *)cache + sizeof(*cache));
    QImage img = MakeImage(bitmap);
    m_bitmapCache.insert(key, img.copy());
    finishOneCommand();
}

void SvpRender::cacheDeleteBitmap(pac_deleted_bitmap *cache, int size)
{
    QString key = CacheId(cache->hash);
    m_bitmapCache.remove(key);
    finishOneCommand();
}

void SvpRender::drawLine(pac_line *line, int size)
{
    switch (line->mix) {
    case GDI_R2_NOP:
        break;
    case GDI_R2_BLACK:
        m_painter.setPen(Qt::black);
        m_painter.drawLine(line->x1, line->y1, line->x2, line->y2);
        break;
    case GDI_R2_NOTCOPYPEN:
        m_painter.setPen(QColor(~line->rgb));
        m_painter.drawLine(line->x1, line->y1, line->x2, line->y2);
        break;
    case GDI_R2_COPYPEN:
        m_painter.setPen(QColor(line->rgb));
        m_painter.drawLine(line->x1, line->y1, line->x2, line->y2);
        break;
    case GDI_R2_WHITE:
        m_painter.setPen(Qt::white);
        m_painter.drawLine(line->x1, line->y1, line->x2, line->y2);
        break;
    default:
        qDebug() << "draw line not supported rop2" << Rop2Name(line->mix);
    }
    finishOneCommand();
}

void SvpRender::drawClipDraw(pac_clip_draw *clip, int size)
{
    QRegion region;
    if (m_clipCount > 0) {
        qWarning() << "recursive clip draw not supported";
        return;
    }
    for (int i = 0; i < clip->rect_count; i++)
        region |= QRect(clip->rect[i].x, clip->rect[i].y, clip->rect[i].w, clip->rect[i].h);
    m_painter.setClipRegion(region);
    m_painter.setClipping(true);
    m_clipCount = clip->cmd_count;
}

void SvpRender::drawPolyline(pac_polyline *pline, int size)
{
    QPolygon poly;
    poly.append(QPoint(pline->pt_start.x, pline->pt_start.y));
    for (int i = 0; i < pline->points.c; i++)
        poly.append(QPoint(pline->points.a[i].x, pline->points.a[i].y));
    switch (pline->mix) {
    case GDI_R2_COPYPEN:
        m_painter.setPen(QColor(pline->rgb));
        m_painter.drawPolyline(poly);
        break;
    default:
        qWarning() << "draw polyline not supported rop2" << Rop2Name(pline->mix);
    }
    finishOneCommand();
}

void SvpRender::drawEllipse(pac_ellipse *ellipse, int size)
{
    switch (ellipse->mix) {
    case GDI_R2_COPYPEN:
        m_painter.setPen(QColor(ellipse->rgb));
        m_painter.drawEllipse(ellipse->pt1.x, ellipse->pt1.y,
                              ellipse->pt2.x - ellipse->pt1.x, ellipse->pt2.y - ellipse->pt1.y);
        break;
    default:
        qWarning() << "draw polyline not supported rop2" << Rop2Name(ellipse->mix);
    }
    finishOneCommand();
}

void SvpRender::drawSaveScreen(pac_save_screen *screen, int size)
{
    if (!screen->restore)
        return;
    pac_bitmap *bitmap = (pac_bitmap *)((uint8_t *)screen + sizeof(*screen));
    QImage img = MakeImage(bitmap);
    m_painter.drawImage(screen->pt1.x, screen->pt1.y, img);
    finishOneCommand();
}

void SvpRender::drawText(pac_text *text, int size)
{
    if (text->w_opaque > 0 && text->h_opaque > 0)
        m_painter.fillRect(text->x_opaque, text->y_opaque, text->w_opaque, text->h_opaque,
                           QColor(text->rgb_bg));
    QImage img = MakeText(text);
    m_painter.drawImage(text->x_bg, text->y_bg, img);
    finishOneCommand();
}

void SvpRender::timeout()
{
    if (!m_image.isNull())
        emit contentChanged(m_image);
}

void SvpRender::finishOneCommand()
{
    if (m_clipCount > 0) {
        m_clipCount--;
        if (m_clipCount == 0)
            m_painter.setClipping(false);
    }
}
