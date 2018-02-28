#include "MainChannel.h"
#include "ChannelManager.h"
#include <QDebug>

MainChannel::MainChannel(const QHostAddress &addr, uint16_t port, QObject *parent)
    : Channel(addr, port, parent)
{
    setType(SH_MAIN);
    connect(this, SIGNAL(connected()), SLOT(initChannel()), Qt::DirectConnection);
}

MainChannel::~MainChannel()
{

}

void MainChannel::initChannel()
{
    qDebug() << Q_FUNC_INFO;

    m_header.c_magic = SVP_MAGIC;
    m_header.c_cmd = SC_MAIN_CHANNEL_INIT;
    m_header.c_size = sizeof(m_channelInit);

    m_channelInit.bandwidth = 100;
    m_channelInit.client_version = 1;
    m_channelInit.rtt = 2;

    write((uint8_t *)&m_header, sizeof(m_header));
    write((uint8_t *)&m_channelInit, sizeof(m_channelInit));
    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}

void MainChannel::processHeader()
{
    qDebug() << Q_FUNC_INFO;
    if (m_header.c_cmd != SC_CHANNEL_LIST || m_header.c_size != sizeof(m_channelList))
        qDebug("bad CHANNEL_LIST header");
    else
        asyncRead((uint8_t *)&m_channelList, sizeof(m_channelList), "processChannelList", 0);
}

void MainChannel::processChannelList()
{
    qDebug() << "server_version: " << m_channelList.server_version << "channels: " << m_channelList.count;
    int i;
    Channel *channel;
    for (int i = 0; i < m_channelList.count; i++) {
        if (m_channelList.channel[i].type == SH_MAIN)
            continue;
        qDebug() << "creating channel" << m_channelList.channel[i].type << "port" << m_channelList.channel[i].port;
        channel = ChannelManager::instance()->createChannel(m_channelList.channel[i].type,
                                                            m_channelList.channel[i].port);
        if (channel)
            channel->connectTo();
    }
}
