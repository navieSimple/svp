#include "CursorChannel.h"
#include <QDebug>

#define CURSOR_BUFSIZE  8192

CursorChannel::CursorChannel(const QHostAddress &addr, uint16_t port, QObject *parent)
    : Channel(addr, port, parent)
{
    setType(SH_CURSOR);
    m_buf.resize(CURSOR_BUFSIZE);
    connect(this, SIGNAL(connected()), SLOT(negotiate()), Qt::DirectConnection);
    connect(this, SIGNAL(negotiated(uint32_t)), SLOT(process()), Qt::DirectConnection);
}

CursorChannel::~CursorChannel()
{

}

void CursorChannel::negotiate()
{
    genericNegotiate(SH_CURSOR, 0, ~0);
}

void CursorChannel::process()
{
    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}

void CursorChannel::processHeader()
{
    if (m_header.c_size < 0 || m_header.c_size > CURSOR_BUFSIZE)
        qDebug() << "error header";
    else
        asyncRead((uint8_t *)m_buf.data(), m_header.c_size, "processCommand", 0);
}

void CursorChannel::processCommand()
{
    uint8_t *data = (uint8_t *)m_buf.data();
    switch (m_header.c_cmd) {
    case SC_CURSOR:
        emit cursorCommand((pac_cursor *)data, m_header.c_size);
        break;
    default:
        qDebug() << Q_FUNC_INFO << "not handled cmd" << m_header.c_cmd;
    }
    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}
