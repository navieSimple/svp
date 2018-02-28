#ifndef INFOWIDGET_H
#define INFOWIDGET_H

#include <QWidget>
#include <svp.h>
#include <net_packet.h>

namespace Ui {
class InfoWidget;
}

class Channel;
class CursorChannel;
class InfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InfoWidget(QWidget *parent = 0);
    ~InfoWidget();

public slots:
    void cursorChanged(pac_cursor *cursor,int size);

private slots:
    void onChannelCreated(Channel *channel);

private:
    Ui::InfoWidget *ui;
    CursorChannel *m_cursor;
};

#endif // INFOWIDGET_H
