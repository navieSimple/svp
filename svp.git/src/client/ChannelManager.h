#ifndef CHANNELMANAGER_H
#define CHANNELMANAGER_H

#include <svp.h>
#include <QObject>
#include <QHostAddress>
#include <QMap>
class Channel;
class MainChannel;
class ChannelManager : public QObject
{
    Q_OBJECT
public:
    explicit ChannelManager(QObject *parent = 0);
    ~ChannelManager();

    static ChannelManager *instance();

    void setMainAddr(const QHostAddress &addr);
    QHostAddress mainAddr() const;

    void setMainPort(uint16_t port);
    uint16_t mainPort() const;

    MainChannel *createMainChannel();
    MainChannel *mainChannel() const;

    Channel *createChannel(int type, uint16_t port);
    Channel *channel(int type);

signals:
    void channelCreated(Channel *channel);
    void mainChannelConnected();
    void mainChannelError();

public slots:
    void start(const QString &addr, quint16 port);

private slots:
    void onChannelError(QAbstractSocket::SocketError err, const QString &msg);

private:
    void processNewChannel(Channel *channel);

    QHostAddress m_mainAddr;
    uint16_t m_mainPort;
    MainChannel *m_mainChannel;
    QMap<int, Channel *> m_channelMap;
};

#endif // CHANNELMANAGER_H
