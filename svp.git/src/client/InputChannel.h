#ifndef INPUTCHANNEL_H
#define INPUTCHANNEL_H

#include "Channel.h"
#include <QPoint>

class InputChannel : public Channel
{
    Q_OBJECT
public:
    explicit InputChannel(const QHostAddress &addr, uint16_t port, QObject *parent = 0);
    ~InputChannel();

    void mouseInput(const QPoint &pos, Qt::MouseButtons buttons);
    void keyboardInput(uint32_t scancode, bool down);

private slots:
    void negotiate();
    void process();

private:
    void run();

    class Private;
    Private *d;
};

#endif // INPUTCHANNEL_H
