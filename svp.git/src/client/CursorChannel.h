#ifndef CURSORCHANNEL_H
#define CURSORCHANNEL_H

#include "Channel.h"

class CursorChannel : public Channel
{
    Q_OBJECT
public:
    explicit CursorChannel(const QHostAddress &addr, uint16_t port, QObject *parent = 0);
    ~CursorChannel();

signals:
    void cursorCommand(pac_cursor *cursor, int size);

public slots:
    void negotiate();
    void process();

private:
    Q_INVOKABLE void processHeader();
    Q_INVOKABLE void processCommand();

    pac_hdr m_header;
    QByteArray m_buf;
};

#endif // CURSORCHANNEL_H
