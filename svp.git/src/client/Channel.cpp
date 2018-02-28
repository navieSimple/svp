#include "Channel.h"
#include <QDebug>

Channel::Channel(const QHostAddress addr, uint16_t port, QObject *parent)
    : QThread(parent)
    , m_type(-1)
    , m_addr(addr)
    , m_port(port)
    , m_connected(false)
{
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");

    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, true);
    m_socket.setSocketOption(QAbstractSocket::KeepAliveOption, true);
    m_socket.setReadBufferSize(4 * 1024 * 1024);
    m_socket.moveToThread(this);
    connect(&m_socket, SIGNAL(connected()), SIGNAL(connected()), Qt::DirectConnection);
    connect(&m_socket, SIGNAL(readyRead()), SLOT(readData()), Qt::DirectConnection);
    connect(&m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(onSocketError(QAbstractSocket::SocketError)), Qt::DirectConnection);
}

Channel::~Channel()
{
    quit();
    wait();
}

void Channel::setType(int ty)
{
    m_type = ty;
}

int Channel::type() const
{
    return m_type;
}

void Channel::connectTo()
{
    start();
}

void Channel::asyncRead(uint8_t *data, int size, const char *doneMethod, const char *errorMethod)
{
    if (m_async.active) {
        qDebug() << Q_FUNC_INFO << "ignore read on active channel";
        return;
    }
    m_async.active = true;
    m_async.now = data;
    m_async.end = m_async.now + size;
    m_async.done = doneMethod;
    m_async.error = errorMethod;
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qDebug() << Q_FUNC_INFO << "not connected state" << m_socket.state();
        return;
    }
    readData();
}

int Channel::read(uint8_t *data, int size)
{
    uint8_t *now = data;
    uint8_t *end = data + size;
    int n;
    while (now < end) {
        n = m_socket.read((char *)now, end - now);
        if (n == -1)
            return -1;
        else {
            now += n;
            if (now < end)
                m_socket.waitForReadyRead();
            else
                break;
        }
    }
    return size;
}

void Channel::write(const uint8_t *data, int size)
{
    int n = m_socket.write((const char *)data, size);
    if (n != size) {
        qDebug("Channel::write size %d error: ret %d", size, n);
        return;
    }
    m_socket.flush();
}

void Channel::run()
{
    m_socket.connectToHost(m_addr, m_port);
    exec();
}

void Channel::genericNegotiate(int channel, int qlen, uint32_t features)
{
    m_header.c_magic = SVP_MAGIC;
    m_header.c_cmd = SC_CHANNEL_INIT;
    m_header.c_size = sizeof(m_init);

    m_init.channel = channel;
    m_init.queue_length = qlen;
    m_init.features = features;

    write((uint8_t *)&m_header, sizeof(m_header));
    write((uint8_t *)&m_init, sizeof(m_init));

    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processNegotiateHeader", 0);
}

void Channel::readData()
{
    if (!m_async.active || !m_async.now || !m_async.end || m_async.now > m_async.end)
        return;
    int n = m_socket.read((char *)m_async.now, m_async.end - m_async.now);
    if (n == -1) {
        qDebug() << "error";
        m_async.active = false;
        if (!m_async.error.isEmpty())
            metaObject()->invokeMethod(this, m_async.error.constData(),
                                       Qt::DirectConnection,
                                       Q_ARG(QAbstractSocket::SocketError, m_socket.error()));
    } else {
        m_async.now += n;
        if (m_async.now == m_async.end) {
            m_async.active = false;
            if (!m_async.done.isEmpty())
                metaObject()->invokeMethod(this, m_async.done.constData(), Qt::DirectConnection);
        }
    }
}

void Channel::onSocketError(QAbstractSocket::SocketError err)
{
    emit error(err, m_socket.errorString());
    m_socket.abort();
    exit(-1);
}

void Channel::processNegotiateHeader()
{
    if (m_header.c_cmd != SC_CHANNEL_READY || m_header.c_size != sizeof(m_ready))
        qDebug() << Q_FUNC_INFO << "error header";
    else
        asyncRead((uint8_t *)&m_ready, sizeof(m_ready),
                  "processNegotiateCommand", 0);
}

void Channel::processNegotiateCommand()
{
    emit negotiated(m_ready.features);
}
