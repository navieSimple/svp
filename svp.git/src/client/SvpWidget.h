#ifndef SVPWIDGET_H
#define SVPWIDGET_H

#include <QWidget>
#include <QImage>
#include <QCursor>
#include <svp.h>
#include "SvpRender.h"

class Channel;
class CursorChannel;
class DisplayChannel;
class InputChannel;
class SvpWidget : public QWidget
{
    Q_OBJECT

public:
    SvpWidget(QWidget *parent = 0);
    ~SvpWidget();

    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyboardInput(quint32 scancode, bool down);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void showEvent(QShowEvent *event);

public slots:
    void drawDesktop(const QImage &image);
    void resizeDesktop(const QSize &size);
    void setDesktopCursor(const GDIUtils::Cursor &cursor);

private slots:
    void onChannelCreated(Channel *channel);

private:
    void bindCursor();
    void bindDisplay();
    void drawGrid(QPainter *p);

    CursorChannel *m_cursor;
    DisplayChannel *m_display;
    InputChannel *m_input;
    SvpRender *m_render;
    QImage m_image;
};

#endif // SVPWIDGET_H
