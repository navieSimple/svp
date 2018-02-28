#ifndef SVPRENDER_H
#define SVPRENDER_H

#include <gdi/GDIUtils.h>
#include <QObject>
#include <QPixmap>
#include <QTimer>
#include <QCursor>
#include <QPainter>
#include <svp.h>

class SvpRender : public QObject
{
    Q_OBJECT
public:
    explicit SvpRender(QObject *parent = 0);
    ~SvpRender();

signals:
    void cursorChanged(const GDIUtils::Cursor &cursor);
    void contentChanged(const QImage &image);
    void sizeChanged(const QSize &size);

public slots:
    void changeScreenSize(pac_screen_resize *resize, int size);
    void changeCursor(pac_cursor *cursor, int size);

    void drawDirtyRect(pac_bitmap *bits, int size);
    void drawSolidRect(pac_solid_rect *rect, int size);
    void drawSolidBlt(pac_solid_blt *blt, int size);
    void drawDstBlt(pac_dst_blt *blt, int size);
    void drawScreenBlt(pac_screen_blt *blt, int size);
    void drawPatternBltBrush(pac_pattern_blt_brush *blt, int size);
    void drawMemBlt(pac_mem_blt *blt, int size);

    void cacheInsertBitmap(pac_cached_bitmap *cache, int size);
    void cacheDeleteBitmap(pac_deleted_bitmap *cache, int size);

    void drawLine(pac_line *line, int size);
    void drawClipDraw(pac_clip_draw *clip, int size);
    void drawPolyline(pac_polyline *pline, int size);
    void drawEllipse(pac_ellipse *ellipse, int size);
    void drawSaveScreen(pac_save_screen *screen, int size);
    void drawText(pac_text *text, int size);

    void timeout();

private:
    void finishOneCommand();
    QImage decodeImage(pac_bitmap *bitmap);

    QImage m_image;
    QHash<QString, QImage> m_bitmapCache;
    QTimer m_updateTimer;
    GDIUtils::Cursor m_cursor;
    QPainter m_painter;
    int m_clipCount;
};

#endif // SVPRENDER_H
