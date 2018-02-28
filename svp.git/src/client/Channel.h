#ifndef CHANNEL_H
#define CHANNEL_H

#include <QObject>
#include <QThread>
#include <QHostAddress>
#include <QTcpSocket>
#include <stdint.h>
#include <svp.h>
#include <net_packet.h>

class Channel : public QThread
{
    Q_OBJECT
    struct AsyncReadInfo
    {
        AsyncReadInfo() : active(false), now(0), end(0), done(0), error(0) {}

        bool active;
        uint8_t *now;
        uint8_t *end;
        QByteArray done;
        QByteArray error;
    };
public:
    explicit Channel(const QHostAddress addr, uint16_t port, QObject *parent = 0);
    virtual ~Channel();

    void setType(int ty);
    int type() const;

    void connectTo();
    void asyncRead(uint8_t *data, int size, const char *doneMethoid, const char *errorMethod);
    int read(uint8_t *data, int size);
    void write(const uint8_t *data, int size);

    void run();

protected:
    void genericNegotiate(int channel, int qlen, uint32_t features);

signals:
    void connected();
    void negotiated(uint32_t features);
    void error(QAbstractSocket::SocketError err, const QString &msg);

private slots:
    void readData();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    Q_INVOKABLE void processNegotiateHeader();
    Q_INVOKABLE void processNegotiateCommand();

    int m_type;
    QHostAddress m_addr;
    uint16_t m_port;
    QTcpSocket m_socket;
    bool m_connected;
    AsyncReadInfo m_async;
    pac_hdr m_header;
    pac_channel_init m_init;
    pac_channel_ready m_ready;
};

#endif // CHANNEL_H
