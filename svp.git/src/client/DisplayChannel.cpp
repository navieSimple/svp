#include "DisplayChannel.h"
#include "MainWindow.h"
#include <SvpSettings.h>
#include <QDebug>

class DisplayChannel::Proxy : public QObject
{
    Q_OBJECT
public:
    Proxy(DisplayChannel *_q) : q(_q) {}
    ~Proxy() {}

public slots:
    void sendGraphicSettingsProxy()
    {
        q->sendGraphicSettings();
    }

private:
    DisplayChannel *q;
};

#define DISPLAY_BUFSIZE SVP_SEND_BUFSIZE
DisplayChannel::DisplayChannel(const QHostAddress &addr, uint16_t port, QObject *parent)
    : Channel(addr, port, parent)
    , m_proxy(new DisplayChannel::Proxy(this))
{
    setType(SH_DISPLAY);
    m_buf.resize(DISPLAY_BUFSIZE);
    m_exbuf.resize(DISPLAY_BUFSIZE);
    connect(this, SIGNAL(connected()), SLOT(negotiate()), Qt::DirectConnection);
    connect(this, SIGNAL(negotiated(uint32_t)), SLOT(process()), Qt::DirectConnection);

    m_proxy->moveToThread(this);
    connect(MainWindow::instance(), SIGNAL(settingsChanged()),
            m_proxy, SLOT(sendGraphicSettingsProxy()), Qt::QueuedConnection);
    connect(this, SIGNAL(destroyed()), m_proxy, SLOT(deleteLater()));
}

DisplayChannel::~DisplayChannel()
{

}

void DisplayChannel::negotiate()
{
    genericNegotiate((int)SH_DISPLAY, 0, ~0);
}

void DisplayChannel::process()
{
    sendGraphicSettings();
    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}

void DisplayChannel::processHeader()
{
//    qDebug() << Q_FUNC_INFO << m_header.c_cmd << m_header.c_size;
    if (m_header.c_size < 0 || m_header.c_size > DISPLAY_BUFSIZE)
        qDebug() << "error header";
    else if (m_header.c_magic == SVP_MAGIC)
        asyncRead((uint8_t *)m_buf.data(), m_header.c_size, "processCommand", 0);
}

void DisplayChannel::processCommand()
{
    uint8_t *data;
    int size;
    pac_lz4_hdr *lz4;
    if (m_header.c_magic == SVP_MAGIC) {
        data = (uint8_t *)m_buf.data();
        size = m_header.c_size;
    } else {
        qDebug() << "bad packet magic" << hex << m_header.c_magic;
        return;
    }
    switch (m_header.c_cmd) {
    case SC_NOP:
        break;
    case SC_SCREEN_RESIZE:
        emit screenResizeCommand((pac_screen_resize *)data, size);
        break;
    case SC_DIRTY_RECT:
        emit dirtyRectCommand((pac_bitmap *)data, size);
        break;
    case SC_SOLID_RECT:
        emit solidRectCommand((pac_solid_rect *)data, size);
        break;
    case SC_SOLID_BLT:
        emit solidBltCommand((pac_solid_blt *)data, size);
        break;
    case SC_DST_BLT:
        emit dstBltCommand((pac_dst_blt *)data, size);
        break;
    case SC_SCREEN_BLT:
        emit screenBltCommand((pac_screen_blt *)data, size);
        break;
    case SC_PATTERN_BLT_BRUSH:
        emit patternBltBrushCommand((pac_pattern_blt_brush *)data, size);
        break;
    case SC_MEM_BLT:
        emit memBltCommand((pac_mem_blt *)data, size);
        break;
    case SC_CACHED_BITMAP:
        emit cachedBitmapCommand((pac_cached_bitmap *)data, size);
        break;
    case SC_DELETED_BITMAP:
        emit deletedBitmapCommand((pac_deleted_bitmap *)data, size);
        break;
    case SC_LINE:
        emit lineCommand((pac_line *)data, size);
        break;
    case SC_CLIP_DRAW:
        emit clipDrawCommand((pac_clip_draw *)data, size);
        break;
    case SC_POLYLINE:
        emit polylineCommand((pac_polyline *)data, size);
        break;
    case SC_ELLIPSE:
        emit ellipseCommand((pac_ellipse *)data, size);
        break;
    case SC_SAVE_SCREEN:
    case SC_RESTORE_SCREEN:
        emit saveScreenCommand((pac_save_screen *)data, size);
        break;
    case SC_TEXT:
        emit textCommand((pac_text *)data, size);
        break;
    default:
        qDebug() << Q_FUNC_INFO << "not handled cmd" << m_header.c_cmd;
    }
    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}

void DisplayChannel::sendGraphicSettings()
{
    QByteArray b = SvpSettings::instance()->dump();
    int size;
    struct
    {
        pac_hdr hdr;
        pac_graphic_config conf;
        char data[4096];
    } __attribute__((packed)) req;

    if (b.size() > sizeof(req.data)) {
        qWarning() << QString("settings data size(%1) too large").arg(b.size());
        return;
    }

    size = sizeof(req.hdr) + sizeof(req.conf) + b.size();
    req.hdr = {SVP_MAGIC, SC_GRAPHIC_CONFIG, (uint16_t)(size - sizeof(req.hdr))};
    req.conf.size = b.size();
    memcpy(req.data, b.constData(), b.size());
    write((uint8_t *)&req, size);
}

#include "DisplayChannel.moc"
