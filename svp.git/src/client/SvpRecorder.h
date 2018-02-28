#ifndef SVPRECORDER_H
#define SVPRECORDER_H

#include <svp.h>
#include <net_packet.h>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QBitmap>
#include <QObject>
#include <QString>
#include <QMutex>

class Channel;
class CursorChannel;
class DisplayChannel;
class SvpRecorder : public QObject
{
    Q_OBJECT
public:
    explicit SvpRecorder(QObject *parent = 0);
    ~SvpRecorder();

    void setDir(const QString &dir);

signals:

public slots:
    void saveScreenResize(pac_screen_resize *resize, int size);
    void saveCursor(pac_cursor *cursor, int size);

    void saveDirtyRect(pac_bitmap *bitmap, int size);
    void saveSolidRect(pac_solid_rect *rect, int size);
    void saveSolidBlt(pac_solid_blt *blt, int size);
    void saveDstBlt(pac_dst_blt *blt, int size);
    void saveScreenBlt(pac_screen_blt *blt, int size);
    void savePatternBltBrush(pac_pattern_blt_brush *blt, int size);
    void saveMemBlt(pac_mem_blt *blt, int size);

    void saveCacheInsert(pac_cached_bitmap *cache, int size);
    void saveCacheDelete(pac_deleted_bitmap *cache, int size);

    void saveLine(pac_line *line, int size);
    void saveClipDraw(pac_clip_draw *clip, int size);
    void savePolyline(pac_polyline *pline, int size);
    void saveEllipse(pac_ellipse *ellipse, int size);
    void saveScreen(pac_save_screen *screen, int size);
    void saveText(pac_text *text, int size);

private slots:
    void onChannelCreated(Channel *channel);

private:
    void bindRecorder();
    void initRecordFile();
    void writeHeader();
    void writeTail();
    void saveImage(QImage &img, const QString &name);
    void saveImage(QBitmap &img, const QString &name);

    QMutex m_mutex;
    DisplayChannel *m_display;
    CursorChannel *m_cursor;
    QString m_fileName;
    QString m_dirPath;
    QDir m_dir;
    QFile m_file;
    int m_cursorCount;
    int m_bitmapCount;
    int m_indent;
    bool m_binded;
    int m_clipCount;
};

#endif // SVPRECORDER_H
