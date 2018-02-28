#ifndef DISPLAYCHANNEL_H
#define DISPLAYCHANNEL_H

#include "Channel.h"
#include <QByteArray>

class DisplayChannel : public Channel
{
    Q_OBJECT
public:
    explicit DisplayChannel(const QHostAddress &addr, uint16_t port, QObject *parent = 0);
    ~DisplayChannel();

    void sendGraphicSettings();

signals:
    void screenResizeCommand(pac_screen_resize *resize, int size);
    void dirtyRectCommand(pac_bitmap *bits, int size);
    void solidRectCommand(pac_solid_rect *rect, int size);
    void solidBltCommand(pac_solid_blt *blt, int size);
    void dstBltCommand(pac_dst_blt *blt, int size);
    void screenBltCommand(pac_screen_blt *blt, int size);
    void patternBltBrushCommand(pac_pattern_blt_brush *blt, int size);
    void memBltCommand(pac_mem_blt *blt, int size);
    void cachedBitmapCommand(pac_cached_bitmap *cache, int size);
    void deletedBitmapCommand(pac_deleted_bitmap *cache, int size);
    void lineCommand(pac_line *line, int size);
    void clipDrawCommand(pac_clip_draw *clip, int size);
    void polylineCommand(pac_polyline *pline, int size);
    void ellipseCommand(pac_ellipse *ellipse, int size);
    void saveScreenCommand(pac_save_screen *screen, int size);
    void textCommand(pac_text *line, int size);

private slots:
    void negotiate();
    void process();

private:
    Q_INVOKABLE void processHeader();
    Q_INVOKABLE void processCommand();

    pac_hdr m_header;
    QByteArray m_buf;
    QByteArray m_exbuf;
    class Proxy;
    Proxy *m_proxy;
};

#endif // DISPLAYCHANNEL_H
