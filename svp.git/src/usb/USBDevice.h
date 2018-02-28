#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <stdint.h>
#include "libusbx-1.0/libusb.h"

enum USBSpeed {
    LowSpeed = 1,
    FullSpeed = 2,
    HighSpeed = 3,
    SuperSpeed = 4,
};

typedef struct libusb_device libusb_device;
typedef struct libusb_device_handle libusb_device_handle;
struct pac_usb_queue_urb;
struct pac_usb_cancel_urb;
struct pac_usb_urb;
class USBDevice : public QObject
{
    Q_OBJECT
public:
    explicit USBDevice(libusb_device *dev, QObject *parent = 0);
    ~USBDevice();

    void ref();
    void deref();

    bool isAttached() const;
    bool attachDriver();
    void detachDriver();

    int reset();
    int setConfig(uint8_t config);
    int claimInterface(uint8_t iface);
    void queueURB(pac_usb_queue_urb *urb);
    void cancelURB(pac_usb_cancel_urb *urb);
    int clearHaltedEP(uint8_t ep);

    static void LIBUSB_CALL onTransferFinished(struct libusb_transfer *transfer);

signals:
    void urbFinished(pac_usb_urb *urb, void *data);

public slots:

public:
    int cnt;

    uint32_t id;
    uint64_t key;

    uint16_t usbVersion;
    uint8_t klass;
    uint8_t  subClass;
    uint8_t  protocol;
    uint8_t  maxPacketSize;
    uint16_t vid;
    uint16_t pid;
    uint16_t revision;
    char vendorName[32];
    char productName[32];
    char serialNumber[32];
    uint8_t port;
    USBSpeed speed;

private:
    void loadStringDescriptors();
    bool isAttachedDriver(const char *driverName);
    Q_INVOKABLE int __reset();
    Q_INVOKABLE int __setConfig(uint8_t config);
    Q_INVOKABLE int __claimInterface(uint8_t iface);
    Q_INVOKABLE void __queueURB(pac_usb_queue_urb *urb);
    Q_INVOKABLE void __cancelURB(pac_usb_cancel_urb *urb);
    Q_INVOKABLE int __clearHaltedEP(uint8_t ep);

    libusb_device *m_dev;
    libusb_device_handle *m_handle;
    bool m_attached;
    QMap<uint32_t, libusb_transfer *> m_transferMap;
    QMap<libusb_transfer *, uint32_t> m_handleMap;
};

#endif // USBDEVICE_H
