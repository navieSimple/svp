#ifndef MAINCHANNEL_H
#define MAINCHANNEL_H

#include "Channel.h"
#include <svp.h>
#include <net_packet.h>

class MainChannel : public Channel
{
    Q_OBJECT
public:
    explicit MainChannel(const QHostAddress &addr, uint16_t port, QObject *parent = 0);
    ~MainChannel();

public slots:
    void initChannel();

    void processHeader();

    void processChannelList();

public slots:

private:
    pac_hdr m_header;
    pac_main_channel_init m_channelInit;
    pac_channel_list m_channelList;
};

#endif // MAINCHANNEL_H
