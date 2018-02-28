#include "SvpRecorder.h"
#include "ChannelManager.h"
#include "CursorChannel.h"
#include "DisplayChannel.h"
#include <gdi/GDIUtils.h>
#include <QBitmap>
#include <QColor>
#include <QTime>
#include <QStringBuilder>
using namespace GDIUtils;

SvpRecorder::SvpRecorder(QObject *parent)
    : QObject(parent)
    , m_display(0)
    , m_cursor(0)
    , m_cursorCount(0)
    , m_bitmapCount(0)
    , m_indent(0)
    , m_binded(false)
    , m_clipCount(0)
{
    connect(ChannelManager::instance(), SIGNAL(channelCreated(Channel*)), SLOT(onChannelCreated(Channel*)));
}

SvpRecorder::~SvpRecorder()
{
    m_file.close();
}

void SvpRecorder::setDir(const QString &dir)
{
    m_dirPath = dir;
    initRecordFile();
}

void SvpRecorder::saveScreenResize(pac_screen_resize *resize, int size)
{
    writeHeader();
    m_file.write(QString("screen_info %1x%2")
                 .arg(resize->w).arg(resize->h)
                 .toUtf8());
    writeTail();
}

void SvpRecorder::saveCursor(pac_cursor *cursor, int size)
{
    uint8_t *ptr = (uint8_t *)&cursor[1];
    writeHeader();
    m_file.write(QString("cursor hot %1,%2 %3x%4 mask_len %5 data_len %6")
                 .arg(cursor->x_hot).arg(cursor->y_hot).arg(cursor->w).arg(cursor->h)
                 .arg(cursor->mask_len).arg(cursor->data_len)
                 .toUtf8());
    writeTail();
    QBitmap mask = QBitmap::fromData(QSize(cursor->w, cursor->h), ptr, QImage::Format_Mono);
    saveImage(mask, QString("cursor-%1-mask.bmp").arg(m_cursorCount++, 4, 16, QLatin1Char('0')));
    QImage pixmap(ptr + cursor->mask_len, cursor->w, cursor->h, QImage::Format_RGB888);
    saveImage(pixmap, QString("cursor-%1-bitmap.bmp").arg(m_cursorCount, 4, 16, QLatin1Char('0')));
}

void SvpRecorder::saveDirtyRect(pac_bitmap *bitmap, int size)
{
    QString fname = QString("bitmap-%1.bmp").arg(m_bitmapCount++, 5, 10, QLatin1Char('0'));
    QImage img;
    writeHeader();
    m_file.write(QString("dirty_rect: %1,%2@%3x%4 fmt %5 file %6")
                 .arg(bitmap->x).arg(bitmap->y).arg(bitmap->w).arg(bitmap->h).arg(bitmap->format).arg(fname)
                 .toUtf8());
    writeTail();
    img = MakeImage(bitmap);
    saveImage(img, fname);
}

void SvpRecorder::saveSolidRect(pac_solid_rect *rect, int size)
{
    writeHeader();
    m_file.write(QString("solid_rect %1,%2@%3x%4 color %5")
                 .arg(rect->x).arg(rect->y) .arg(rect->w).arg(rect->h)
                 .arg(QColor(rect->rgb).name()).toUtf8());
    writeTail();
}

void SvpRecorder::saveSolidBlt(pac_solid_blt *blt, int size)
{
    writeHeader();
    m_file.write(QString("solid_blt %1,%2@%3x%4 color %5 rop %6")
                 .arg(blt->x).arg(blt->y) .arg(blt->w).arg(blt->h)
                 .arg(QColor(blt->rgb).name()).arg(Rop3Name(blt->rop)).toUtf8());
    writeTail();
}

void SvpRecorder::saveDstBlt(pac_dst_blt *blt, int size)
{
    writeHeader();
    m_file.write(QString("dst_blt %1,%2@%3x%4 rop %5")
                 .arg(blt->x).arg(blt->y) .arg(blt->w).arg(blt->h)
                 .arg(Rop3Name(blt->rop)).toUtf8());
    writeTail();
}

void SvpRecorder::saveScreenBlt(pac_screen_blt *blt, int size)
{
    writeHeader();
    m_file.write(QString("screen_blt dst: %1,%2@%3x%4 src: %5,%6 rop %7")
                 .arg(blt->x).arg(blt->y) .arg(blt->w).arg(blt->h)
                 .arg(blt->x_src).arg(blt->y_src)
                 .arg(Rop3Name(blt->rop)).toUtf8());
    writeTail();
}

void SvpRecorder::savePatternBltBrush(pac_pattern_blt_brush *blt, int size)
{
    writeHeader();
    m_file.write(QString("pat_blt dst: %1,%2@%3x%4 src: %5,%6 pattern %7 fg_color %8 bg_color %9 rop %10")
                 .arg(blt->x).arg(blt->y).arg(blt->w).arg(blt->h)
                 .arg(blt->x_src).arg(blt->y_src)
                 .arg(*(uint64_t *)blt->pattern, 0, 16)
                 .arg(QColor(blt->rgb_fg).name()).arg(QColor(blt->rgb_bg).name())
                 .arg(Rop3Name(blt->rop)).toUtf8());
    writeTail();
}

void SvpRecorder::saveMemBlt(pac_mem_blt *blt, int size)
{
    writeHeader();
    m_file.write(QString("mem_blt dst: %1,%2@%3x%4 src: %5,%6 cache %7 rop %8")
                 .arg(blt->x).arg(blt->y) .arg(blt->w).arg(blt->h)
                 .arg(blt->x_src).arg(blt->y_src)
                 .arg(CacheId(blt->hash))
                 .arg(Rop3Name(blt->rop))
                 .toUtf8());
    writeTail();
}

void SvpRecorder::saveCacheInsert(pac_cached_bitmap *cache, int size)
{
    pac_bitmap *bitmap = (pac_bitmap *)&cache[1];
    QString id = CacheId(cache->hash);
    QImage img;

    writeHeader();
    m_file.write(QString("cache_insert %1")
                 .arg(id)
                 .toUtf8());
    m_file.write(QString(" geometry %1,%2@%3x%4x fmt %5")
                 .arg(bitmap->x).arg(bitmap->y).arg(bitmap->w).arg(bitmap->h).arg(bitmap->format)
                 .toUtf8());
    writeTail();
    img = MakeImage(bitmap);
    saveImage(img, QString("cache-%1.bmp").arg(id));
}

void SvpRecorder::saveCacheDelete(pac_deleted_bitmap *cache, int size)
{
    writeHeader();
    m_file.write(QString("cache_delete %1")
                 .arg(CacheId(cache->hash))
                 .toUtf8());
    writeTail();
}

void SvpRecorder::saveLine(pac_line *line, int size)
{
    writeHeader();
    m_file.write(QString("line %1,%2-%3,%4 bound %5,%6-%7,%8 color %9 mix %10")
                 .arg(line->x1).arg(line->y1).arg(line->x2).arg(line->y2)
                 .arg(line->x_bounds1).arg(line->y_bounds1).arg(line->x_bounds2).arg(line->y_bounds2)
                 .arg(QColor(line->rgb).name()).arg(Rop2Name(line->mix))
                 .toUtf8());
    writeTail();
}

void SvpRecorder::saveClipDraw(pac_clip_draw *clip, int size)
{
    writeHeader();
    m_clipCount = clip->cmd_count;
    m_file.write(QString("clip_draw rects %1 cmds %2")
                 .arg(clip->rect_count).arg(clip->cmd_count)
                 .toUtf8());
    writeTail();
}

void SvpRecorder::savePolyline(pac_polyline *pline, int size)
{
    writeHeader();
    m_file.write(QString("polyline %1 points, start %2,%3")
                 .arg(pline->points.c).arg(pline->pt_start.x).arg(pline->pt_start.y)
                 .toUtf8());
    for (int i = 0; i < pline->points.c; i++)
        m_file.write(QString(" %1,%2")
                     .arg(pline->points.a[i].x).arg(pline->points.a[i].y)
                     .toUtf8());
    m_file.write(QString(" color %1 mix %2")
                 .arg(QColor(pline->rgb).name()).arg(Rop2Name(pline->mix))
                 .toUtf8());
    writeTail();
}

void SvpRecorder::saveEllipse(pac_ellipse *ellipse, int size)
{
    writeHeader();
    m_file.write(QString("ellipse: %1,%2@%3x%4 color %5 fill_mode %6 mix %7")
                 .arg(ellipse->pt1.x).arg(ellipse->pt1.y)
                 .arg(ellipse->pt2.x - ellipse->pt1.x).arg(ellipse->pt2.y - ellipse->pt1.y)
                 .arg(QColor(ellipse->rgb).name()).arg(ellipse->fill_mode).arg(Rop2Name(ellipse->mix))
                 .toUtf8());
    writeTail();
}

void SvpRecorder::saveScreen(pac_save_screen *screen, int size)
{
    QImage img;
    writeHeader();
    m_file.write(QString("save_screen pt1: %1,%2; pt2: %3,%4 indent %5 restore %6")
                 .arg(screen->pt1.x).arg(screen->pt1.y)
                 .arg(screen->pt2.x).arg(screen->pt2.y)
                 .arg(screen->ident).arg(screen->restore).toUtf8());
    writeTail();
    if (screen->restore) {
        m_indent++;
        saveDirtyRect((pac_bitmap *)&screen[1], 0);
        m_indent--;
    }
}

void SvpRecorder::saveText(pac_text *text, int size)
{
    struct pac_glyph *glyph;
    struct pac_bitmap *bitmap;
    uint8_t *ptr = (uint8_t *)text + sizeof(*text);
    QImage img;

    writeHeader();
    m_file.write(QString("text: order %1 bg %2,%3@%4x%5 opaque %6,%7@%8x%9"
                         " max_glyph %10 glyphs %11 flags %12 char_inc %13"
                         " fg_color %14 bg_color %15")
                 .arg(text->size)
                 .arg(text->x_bg).arg(text->y_bg).arg(text->w_bg).arg(text->h_bg)
                 .arg(text->x_opaque).arg(text->y_opaque).arg(text->w_opaque).arg(text->h_opaque)
                 .arg(text->max_glyph).arg(text->glyphs).arg(text->flags).arg(text->char_inc)
                 .arg(QColor(text->rgb_fg).name()).arg(QColor(text->rgb_bg).name())
                 .toUtf8());
    writeTail();
    m_indent++;
    for (int i = 0; i < text->glyphs; i++) {
        glyph = (struct pac_glyph *)ptr;
        ptr += sizeof(*glyph);
        writeHeader();
        m_file.write(QString("glyph: next %1 handle %2 %3,%4@%5x%6 orig %7,%8")
                     .arg(glyph->size).arg(glyph->handle, 0, 16)
                     .arg(glyph->x).arg(glyph->y).arg(glyph->w).arg(glyph->h)
                     .arg(glyph->x_origin).arg(glyph->y_origin)
                     .toUtf8());
        writeTail();
        img = MakeGlyph(glyph, text->rgb_fg);
        saveImage(img, QString("glyph-%1.bmp").arg(glyph->handle, 0, 16));
        ptr += glyph->size - sizeof(*glyph);
    }
    m_indent--;
}

void SvpRecorder::onChannelCreated(Channel *channel)
{
    if (channel->type() == SH_DISPLAY)
        m_display = qobject_cast<DisplayChannel *>(channel);
    else if (channel->type() == SH_CURSOR)
        m_cursor = qobject_cast<CursorChannel *>(channel);
    if (m_display && m_cursor && !m_binded) {
        bindRecorder();
        m_binded = true;
    }
}

void SvpRecorder::bindRecorder()
{
    connect(m_display, SIGNAL(screenResizeCommand(pac_screen_resize*,int)),
            SLOT(saveScreenResize(pac_screen_resize*,int)), Qt::DirectConnection);
    connect(m_cursor, SIGNAL(cursorCommand(pac_cursor*,int)),
            SLOT(saveCursor(pac_cursor*,int)), Qt::DirectConnection);

    connect(m_display, SIGNAL(dirtyRectCommand(pac_bitmap*,int)),
            SLOT(saveDirtyRect(pac_bitmap*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(solidRectCommand(pac_solid_rect*,int)),
            SLOT(saveSolidRect(pac_solid_rect*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(solidBltCommand(pac_solid_blt*,int)),
            SLOT(saveSolidBlt(pac_solid_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(dstBltCommand(pac_dst_blt*,int)),
            SLOT(saveDstBlt(pac_dst_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(screenBltCommand(pac_screen_blt*,int)),
            SLOT(saveScreenBlt(pac_screen_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(patternBltBrushCommand(pac_pattern_blt_brush*,int)),
            SLOT(savePatternBltBrush(pac_pattern_blt_brush*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(memBltCommand(pac_mem_blt*,int)),
            SLOT(saveMemBlt(pac_mem_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(cachedBitmapCommand(pac_cached_bitmap*,int)),
            SLOT(saveCacheInsert(pac_cached_bitmap*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(deletedBitmapCommand(pac_deleted_bitmap*,int)),
            SLOT(saveCacheDelete(pac_deleted_bitmap*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(lineCommand(pac_line*,int)),
            SLOT(saveLine(pac_line*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(clipDrawCommand(pac_clip_draw*,int)),
            SLOT(saveClipDraw(pac_clip_draw*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(polylineCommand(pac_polyline*,int)),
            SLOT(savePolyline(pac_polyline*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(ellipseCommand(pac_ellipse*,int)),
            SLOT(saveEllipse(pac_ellipse*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(saveScreenCommand(pac_save_screen*,int)),
            SLOT(saveScreen(pac_save_screen*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(textCommand(pac_text*,int)),
            SLOT(saveText(pac_text*,int)), Qt::DirectConnection);
}

void SvpRecorder::initRecordFile()
{
    bool ret = QDir::current().mkpath(m_dirPath);
    if (ret) {
        m_dir = QDir(m_dirPath);
    } else {
        m_dir = QDir::current();
        m_dirPath = QDir::currentPath();
    }
    m_file.close();
    m_file.setFileName(m_dir.filePath("DrawCmd.txt"));
    m_file.open(QIODevice::WriteOnly | QIODevice::Unbuffered | QIODevice::Text);
    qDebug() << "recording to" << QFileInfo(m_file).filePath();
    m_cursorCount = m_bitmapCount = 0;
    m_indent = 0;
    m_clipCount = 0;
}

void SvpRecorder::writeHeader()
{
    m_mutex.lock();
    m_file.write(QTime::currentTime().toString("HH:mm:ss.zzz ").toUtf8());
    m_file.write(QByteArray(m_indent * 4, ' '));
    if (m_clipCount > 0) {
        m_file.write(QByteArray(4, ' '));
        m_clipCount--;
    }
}

void SvpRecorder::writeTail()
{
    m_file.write("\n");
    m_mutex.unlock();
}

void SvpRecorder::saveImage(QImage &img, const QString &name)
{
    img.save(m_dirPath + "/" + name);
}

void SvpRecorder::saveImage(QBitmap &img, const QString &name)
{
    img.save(m_dirPath + "/" + name);
}

