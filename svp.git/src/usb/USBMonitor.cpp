#include "USBMonitor.h"
#include "USBDevice.h"
#include "USBUtils.h"
#include "libwdi/libwdi.h"
#include "libusbx-1.0/libusb.h"
#include <QSocketNotifier>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QList>
#include <QMutex>
#include <stdint.h>

static bool g_ok;

__attribute__((constructor))
static void usb_init()
{
    g_ok = (libusb_init(0) == 0);
    libusb_set_debug(0, LIBUSB_LOG_LEVEL_INFO);
}

__attribute__((destructor))
static void usb_fini()
{
    libusb_exit(0);
}

class MonitorThread;
class MonitorObject : public QObject
{
    Q_OBJECT
public:
    MonitorObject(MonitorThread *monitor);
    ~MonitorObject();

    USBDevice *__findDeviceByKey(uint64_t key);
    USBDevice *__findDeviceById(uint32_t id);
    bool isGoodDevice(libusb_device *dev);

public slots:
    void onDeviceScanTimeout();
    void onReapTimeout();

private:
    MonitorThread *q;
};

class MonitorThread : public QThread
{
    Q_OBJECT
public:
    explicit MonitorThread(QObject *parent = 0)
    {

    }

    ~MonitorThread()
    {

    }

    void lock()
    {
        m_mutex.lock();
    }

    void unlock()
    {
        m_mutex.unlock();
    }

    QList<USBDevice *> getDeviceList()
    {
        QList<USBDevice *> devs;
        lock();
        devs = m_devs;
        foreach (USBDevice *device, devs)
            device->ref();
        unlock();
        return devs;
    }

    void putDeviceList(QList<USBDevice *> &deviceList)
    {
        foreach (USBDevice *device, deviceList)
            device->deref();
        deviceList.clear();
    }

    bool attachDevice(uint32_t id)
    {
        bool ret = false;
        USBDevice *device;
        lock();
        device = m_obj->__findDeviceById(id);
        if (device) {
            qDebug() << "attaching device" << device->vendorName;
            ret = device->attachDriver();
        }
        unlock();
        if (ret)
            connect(device, SIGNAL(urbFinished(pac_usb_urb*,void*)),
                    USBMonitor::instance(), SIGNAL(urbReady(pac_usb_urb*,void*)),
                    Qt::DirectConnection);
        return ret;
    }

    void detachDevice(uint32_t id)
    {
        USBDevice *device;
        lock();
        device = m_obj->__findDeviceById(id);
        if (device) {
            device->detachDriver();
            device->disconnect(this);
        }
        unlock();
    }

    USBDevice *getDevice(uint32_t id)
    {
        USBDevice *device;
        lock();
        device = m_obj->__findDeviceById(id);
        if (device)
            device->ref();
        unlock();
        return device;
    }

    void putDevice(USBDevice *device)
    {
        lock();
        device->deref();
        unlock();
    }

    void run()
    {
        m_obj = new MonitorObject(this);
        m_scanTimer = new QTimer;
        m_reapTimer = new QTimer;

        connect(m_scanTimer, SIGNAL(timeout()), m_obj, SLOT(onDeviceScanTimeout()));
        m_scanTimer->start(2000);

        connect(m_reapTimer, SIGNAL(timeout()), m_obj, SLOT(onReapTimeout()));
        m_reapTimer->start(0);

        exec();

        delete m_scanTimer;
        delete m_obj;
    }

private:
    friend class MonitorObject;

    MonitorObject *m_obj;
    QTimer *m_scanTimer;
    QTimer *m_reapTimer;
    QMutex m_mutex;
    QList<USBDevice *> m_devs;
};

MonitorObject::MonitorObject(MonitorThread *monitor) : q(monitor)
{

}

MonitorObject::~MonitorObject()
{

}

USBDevice *MonitorObject::__findDeviceByKey(uint64_t key)
{
    foreach(USBDevice *device, q->m_devs)
        if (device->key == key)
            return device;
    return 0;
}

USBDevice *MonitorObject::__findDeviceById(uint32_t id)
{
    foreach(USBDevice *device, q->m_devs)
        if (device->id == id)
            return device;
    return 0;
}

void MonitorObject::onDeviceScanTimeout()
{
    wdi_options_create_list optCreateList = {TRUE, FALSE, TRUE};
    libusb_device **dlist;
    int n;
    libusb_device_descriptor desc;
    uint32_t ports;
    uint64_t key;
    USBDevice *device;
    QList<USBDevice *> newList;
    bool changed = false;

    q->lock();
    n = libusb_get_device_list(0, &dlist);
    for (int i = 0; i < n; i++) {
        if (!isGoodDevice(dlist[i]))
            continue;
        if (libusb_get_device_descriptor(dlist[i], &desc)) {
            qDebug() << "get device descriptor error";
            continue;
        }
        ports = 0;
        libusb_get_port_numbers(dlist[i], (uint8_t *)&ports, sizeof(ports));
        key = MakeKey(desc.idVendor, desc.idProduct, ports);
        device = __findDeviceByKey(key);
        if (device) {
            q->m_devs.removeOne(device);
        } else {
            device = new USBDevice(dlist[i]);
            changed = true;
        }
        newList.append(device);
    }
    libusb_free_device_list(dlist, 1);

    if (!q->m_devs.isEmpty())
        changed = true;
    foreach (device, q->m_devs)
        device->deref();
    q->m_devs = newList;

    q->unlock();

    if (changed)
        emit USBMonitor::instance()->deviceListChanged();
}

void MonitorObject::onReapTimeout()
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    libusb_handle_events_timeout(0, &tv);
}

bool MonitorObject::isGoodDevice(libusb_device *dev)
{
    libusb_device_descriptor desc;
    libusb_config_descriptor *conf = 0;
    int rc;
    bool isGood = true;
    rc = libusb_get_device_descriptor(dev, &desc);
    if (rc) {
//        qDebug() << Q_FUNC_INFO << "get device descriptor error:" << libusb_error_name(rc);
        return false;
    }
    rc = libusb_get_active_config_descriptor(dev, &conf);
    if (rc || !conf) {
//        qDebug() << Q_FUNC_INFO << "get config descriptor error:" << libusb_error_name(rc);
        return false;
    }
    if (conf->interface && conf->interface->num_altsetting > 0) {
//        qDebug() << "interface class" << conf->interface->altsetting[0].bInterfaceClass;
        if (conf->interface->altsetting[0].bInterfaceClass == LIBUSB_CLASS_HUB)
            isGood = false;
    }
    libusb_free_config_descriptor(conf);
    return isGood;
}

USBMonitor::USBMonitor()
    : QObject(0)
{
    m_thread = new MonitorThread();
    m_thread->start();
}

USBMonitor::~USBMonitor()
{
    m_thread->requestInterruption();
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

USBMonitor *USBMonitor::instance()
{
    static USBMonitor *monitor = 0;
    if (!monitor)
        monitor = new USBMonitor;
    return monitor;
}

QList<USBDevice *> USBMonitor::getDeviceList()
{
    return m_thread->getDeviceList();
}

void USBMonitor::putDeviceList(QList<USBDevice *> &deviceList)
{
    m_thread->putDeviceList(deviceList);
}

bool USBMonitor::attachDevice(uint32_t id)
{
    return m_thread->attachDevice(id);
}

void USBMonitor::detachDevice(uint32_t id)
{
    m_thread->detachDevice(id);
}

USBDevice *USBMonitor::getDevice(uint32_t id)
{
    return m_thread->getDevice(id);
}

void USBMonitor::putDevice(USBDevice *device)
{
    m_thread->putDevice(device);
}

#include "USBMonitor.moc"
