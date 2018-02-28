#ifndef USBCHANNEL_H
#define USBCHANNEL_H

#include "Channel.h"

struct pac_usb_urb;
class USBChannelObject;
class USBChannel : public Channel
{
    Q_OBJECT
public:
    explicit USBChannel(const QHostAddress &addr, uint16_t port, QObject *parent = 0);
    ~USBChannel();

signals:

public slots:
    void negotiate();
    void process();

private:
    Q_INVOKABLE void processHeader();
    Q_INVOKABLE void processCommand();
    void processDeviceListQuery();
    void processDeviceOpen(uint8_t *data, uint32_t size);
    void processDeviceReset(uint8_t *data, uint32_t size);
    void processDeviceSetConfig(uint8_t *data, uint32_t size);
    void processDeviceClaimInterface(uint8_t *data, uint32_t size);
    void processDeviceQueueURB(uint8_t *data, uint32_t size);
    void processDeviceReapURB(uint8_t *data, uint32_t size);
    void processDeviceClearHaltedEP(uint8_t *data, uint32_t size);
    void processDeviceCancelURB(uint8_t *data, uint32_t size);

    pac_hdr m_header;
    QByteArray m_buf;
    USBChannelObject *m_obj;

    friend class USBChannelObject;
};

#endif // USBCHANNEL_H
