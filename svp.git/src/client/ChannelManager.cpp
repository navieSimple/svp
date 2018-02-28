#include "ChannelManager.h"
#include "MainChannel.h"
#include "DisplayChannel.h"
#include "CursorChannel.h"
#include "InputChannel.h"
#include "USBChannel.h"
#include <QMessageBox>
#include <QDebug>

static ChannelManager *g_mananger = 0;

ChannelManager::ChannelManager(QObject *parent)
    : QObject(parent)
    , m_mainChannel(0)
{
    g_mananger = this;
}

ChannelManager::~ChannelManager()
{
}

ChannelManager *ChannelManager::instance()
{
    return g_mananger;
}

void ChannelManager::setMainAddr(const QHostAddress &addr)
{
    m_mainAddr = addr;
}

QHostAddress ChannelManager::mainAddr() const
{
    return m_mainAddr;
}

void ChannelManager::setMainPort(uint16_t port)
{
    m_mainPort = port;
}

uint16_t ChannelManager::mainPort() const
{
    return m_mainPort;
}

MainChannel *ChannelManager::createMainChannel()
{
    if (!m_mainChannel) {
        m_mainChannel = new MainChannel(m_mainAddr, m_mainPort, this);
        processNewChannel(m_mainChannel);
    }
    return m_mainChannel;
}

MainChannel *ChannelManager::mainChannel() const
{
    return m_mainChannel;
}

Channel *ChannelManager::createChannel(int type, uint16_t port)
{
    Channel *channel = 0;
    switch (type) {
    case SH_MAIN:
        return createMainChannel();
    case SH_DISPLAY:
        channel = new DisplayChannel(m_mainAddr, port);
        break;
    case SH_CURSOR:
        channel = new CursorChannel(m_mainAddr, port);
        break;
    case SH_INPUT:
        channel = new InputChannel(m_mainAddr, port);
        break;
    case SH_USB:
        channel = new USBChannel(m_mainAddr, port);
        break;
    }
    processNewChannel(channel);
    return channel;
}

Channel *ChannelManager::channel(int type)
{
    return m_channelMap.value(type);
}

void ChannelManager::start(const QString &addr, quint16 port)
{
    qDebug() << Q_FUNC_INFO << addr << port;
    setMainAddr(QHostAddress(addr));
    setMainPort(port);
    createMainChannel();
    if (m_mainChannel)
        m_mainChannel->connectTo();
}

void ChannelManager::onChannelError(QAbstractSocket::SocketError err, const QString &msg)
{
    Channel *channel = qobject_cast<Channel *>(sender());
    if (!channel)
        return;
    if (channel->type() == SH_MAIN) {
        QMessageBox::critical(0, "连接错误", msg);
        emit mainChannelError();
    }
}

void ChannelManager::processNewChannel(Channel *channel)
{
    if (!channel)
        return;
    m_channelMap.insert(channel->type(), channel);
    connect(channel, SIGNAL(error(QAbstractSocket::SocketError,QString)),
            SLOT(onChannelError(QAbstractSocket::SocketError,QString)),
            Qt::QueuedConnection);
    emit channelCreated(channel);
    if (channel->type() == SH_MAIN)
        connect(channel, SIGNAL(connected()), SIGNAL(mainChannelConnected()), Qt::QueuedConnection);
}
