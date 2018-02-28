#include "SvpWidget.h"
#include "ChannelManager.h"
#include "CursorChannel.h"
#include "DisplayChannel.h"
#include "InputChannel.h"
#include "Scancode.h"
#include <QHostAddress>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QTime>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QBitmap>
#include <QPixmap>
#include <QCursor>
#include <QUuid>
#include <QPolygon>
#include <QRegion>
#include <svp.h>

SvpWidget::SvpWidget(QWidget *parent)
    : QWidget(parent)
    , m_cursor(0)
    , m_display(0)
    , m_input(0)
{
    connect(ChannelManager::instance(), SIGNAL(channelCreated(Channel*)), SLOT(onChannelCreated(Channel*)));

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    resizeDesktop(QSize(800, 600));

    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);

    setFocusPolicy(Qt::StrongFocus);
}

SvpWidget::~SvpWidget()
{

}

void SvpWidget::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    if (m_image.isNull()) {
        p.fillRect(event->rect(), Qt::black);
        return;
    }
    p.drawImage(rect(), m_image, m_image.rect(), Qt::ColorOnly | Qt::NoOpaqueDetection | Qt::NoFormatConversion);
//    drawGrid(&p);
}

void SvpWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_input)
        m_input->mouseInput(event->pos(), event->buttons());
}

void SvpWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_input)
        m_input->mouseInput(event->pos(), event->buttons());
}

void SvpWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_input)
        m_input->mouseInput(event->pos(), event->buttons());
}

void SvpWidget::keyboardInput(quint32 scancode, bool down)
{
    if (m_input)
        m_input->keyboardInput(scancode, down);
}

void SvpWidget::focusInEvent(QFocusEvent *event)
{
    grabKeyboard();
}

void SvpWidget::focusOutEvent(QFocusEvent *event)
{
    releaseKeyboard();
}

void SvpWidget::keyPressEvent(QKeyEvent *event)
{
#ifdef Q_OS_UNIX
    int scancode = GetScancode(event);
    keyboardInput(scancode, true);
#endif
}

void SvpWidget::keyReleaseEvent(QKeyEvent *event)
{
#ifdef Q_OS_UNIX
    int scancode = GetScancode(event);
    keyboardInput(scancode, false);
#endif
}

void SvpWidget::showEvent(QShowEvent *event)
{
    setFocus();
}

void SvpWidget::drawDesktop(const QImage &image)
{
    m_image = image;
    update();
}

void SvpWidget::resizeDesktop(const QSize &size)
{
    setFixedSize(size);
}

void SvpWidget::setDesktopCursor(const GDIUtils::Cursor &cursor)
{
    QCursor cur;
    if (cursor.useBitmap) {
#ifdef Q_OS_WIN
        cur = QCursor(QBitmap::fromImage(cursor.image), QBitmap::fromImage(cursor.mask),
                      cursor.hotx, cursor.hoty);
#elif defined(Q_OS_UNIX)
        // there is problem with inverted color cursor under linux, work around it
        QPixmap pixmap = QPixmap::fromImage(cursor.image);
        pixmap.setMask(QBitmap::fromImage(cursor.image));
        cur = QCursor(pixmap, cursor.hotx, cursor.hoty);
#endif
    } else {
        QPixmap pixmap = QPixmap::fromImage(cursor.image);
        pixmap.setMask(QBitmap::fromImage(cursor.mask));
        cur = QCursor(pixmap, cursor.hotx, cursor.hoty);
    }
    setCursor(cur);
}

void SvpWidget::onChannelCreated(Channel *channel)
{
    switch (channel->type()) {
    case SH_DISPLAY:
        m_display = qobject_cast<DisplayChannel *>(channel);
        if (m_display) {
            m_render = new SvpRender();
            m_render->moveToThread(m_display);
            bindDisplay();
        }
        break;
    case SH_CURSOR:
        m_cursor = qobject_cast<CursorChannel *>(channel);
        if (m_cursor)
            bindCursor();
        break;
    case SH_INPUT:
        m_input = qobject_cast<InputChannel *>(channel);
        break;
    }
}

void SvpWidget::bindCursor()
{
    connect(m_cursor, SIGNAL(cursorCommand(pac_cursor*,int)),
            m_render, SLOT(changeCursor(pac_cursor*,int)), Qt::DirectConnection);
}

void SvpWidget::bindDisplay()
{
    connect(m_render, SIGNAL(contentChanged(QImage)),
            SLOT(drawDesktop(QImage)), Qt::BlockingQueuedConnection);
    connect(m_render, SIGNAL(sizeChanged(QSize)),
            SLOT(resizeDesktop(QSize)), Qt::BlockingQueuedConnection);
    qRegisterMetaType<GDIUtils::Cursor>("GDIUtils::Cursor");
    connect(m_render, SIGNAL(cursorChanged(GDIUtils::Cursor)),
            SLOT(setDesktopCursor(GDIUtils::Cursor)), Qt::BlockingQueuedConnection);

    connect(m_display, SIGNAL(screenResizeCommand(pac_screen_resize*,int)),
            m_render, SLOT(changeScreenSize(pac_screen_resize*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(dirtyRectCommand(pac_bitmap*,int)),
            m_render, SLOT(drawDirtyRect(pac_bitmap*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(solidRectCommand(pac_solid_rect*,int)),
            m_render, SLOT(drawSolidRect(pac_solid_rect*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(solidBltCommand(pac_solid_blt*,int)),
            m_render, SLOT(drawSolidBlt(pac_solid_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(dstBltCommand(pac_dst_blt*,int)),
            m_render, SLOT(drawDstBlt(pac_dst_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(screenBltCommand(pac_screen_blt*,int)),
            m_render, SLOT(drawScreenBlt(pac_screen_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(patternBltBrushCommand(pac_pattern_blt_brush*,int)),
            m_render, SLOT(drawPatternBltBrush(pac_pattern_blt_brush*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(memBltCommand(pac_mem_blt*,int)),
            m_render, SLOT(drawMemBlt(pac_mem_blt*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(cachedBitmapCommand(pac_cached_bitmap*,int)),
            m_render, SLOT(cacheInsertBitmap(pac_cached_bitmap*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(deletedBitmapCommand(pac_deleted_bitmap*,int)),
            m_render, SLOT(cacheDeleteBitmap(pac_deleted_bitmap*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(lineCommand(pac_line*,int)),
            m_render, SLOT(drawLine(pac_line*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(clipDrawCommand(pac_clip_draw*,int)),
            m_render, SLOT(drawClipDraw(pac_clip_draw*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(polylineCommand(pac_polyline*,int)),
            m_render, SLOT(drawPolyline(pac_polyline*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(ellipseCommand(pac_ellipse*,int)),
            m_render, SLOT(drawEllipse(pac_ellipse*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(saveScreenCommand(pac_save_screen*,int)),
            m_render, SLOT(drawSaveScreen(pac_save_screen*,int)), Qt::DirectConnection);
    connect(m_display, SIGNAL(textCommand(pac_text*,int)),
            m_render, SLOT(drawText(pac_text*,int)), Qt::DirectConnection);
}
