#ifndef USBMONITOR_H
#define USBMONITOR_H

#include <QObject>
#include <QList>
#include <stdint.h>

class MonitorThread;
class USBDevice;
struct pac_usb_urb;
class USBMonitor : public QObject
{
    Q_OBJECT
public:
    ~USBMonitor();

    static USBMonitor *instance();

    QList<USBDevice *> getDeviceList();

    void putDeviceList(QList<USBDevice *> &deviceList);

    bool attachDevice(uint32_t id);

    void detachDevice(uint32_t id);

    USBDevice *getDevice(uint32_t id);

    void putDevice(USBDevice *device);

signals:
    void deviceListChanged();

    void urbReady(pac_usb_urb *urb, void *data);

public slots:

private:
    USBMonitor();

    MonitorThread *m_thread;
};

#endif // USBMONITOR_H
