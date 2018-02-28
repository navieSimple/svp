#include "USBChannel.h"
#include "usb/USBMonitor.h"
#include "usb/USBDevice.h"
#include <QDebug>

#define USB_BUFSIZE  1048576

class USBChannelObject : public QObject
{
    Q_OBJECT
public:
    USBChannelObject(USBChannel *_q) : QObject(0), q(_q) {}
    ~USBChannelObject() {}

public slots:
    void onURBReady(pac_usb_urb *urb, void *data)
    {
        QByteArray buf;
        pac_hdr cmd;

//        qDebug() << "DeviceUrbData" << urb->id << urb->error << urb->len;
//        qDebug() << Q_FUNC_INFO << QThread::currentThread();

        cmd.c_magic = SVP_MAGIC;
        cmd.c_cmd = SC_USB_URB_DATA;
        cmd.c_size = sizeof(*urb) + (urb->nodata ? 0 : urb->len);
        buf.append((const char *)&cmd, sizeof(cmd));

        buf.append((const char *)urb, sizeof(*urb));

        if (!urb->nodata)
            buf.append((const char *)data, urb->len);

        q->write((const uint8_t *)buf.constData(), buf.size());

        free(data);
        free(urb);
    }

private:
    USBChannel *q;
};

USBChannel::USBChannel(const QHostAddress &addr, uint16_t port, QObject *parent)
    : Channel(addr, port, parent)
    , m_obj(0)
{
    setType(SH_USB);
    m_buf.resize(USB_BUFSIZE);
    connect(this, SIGNAL(connected()), SLOT(negotiate()), Qt::DirectConnection);
    connect(this, SIGNAL(negotiated(uint32_t)), SLOT(process()), Qt::DirectConnection);
    qRegisterMetaType<pac_usb_queue_urb *>("pac_usb_queue_urb *");
    qRegisterMetaType<pac_usb_urb *>("pac_usb_urb *");
}

USBChannel::~USBChannel()
{
    if (m_obj)
        m_obj->deleteLater();
}

void USBChannel::negotiate()
{
    genericNegotiate(SH_USB, 0, ~0);
}

void USBChannel::process()
{
    m_obj = new USBChannelObject(this);
    connect(USBMonitor::instance(), SIGNAL(urbReady(pac_usb_urb*,void*)),
            m_obj, SLOT(onURBReady(pac_usb_urb*,void*)), Qt::QueuedConnection);

    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}

void USBChannel::processHeader()
{
    if (m_header.c_size < 0 || m_header.c_size > USB_BUFSIZE)
        qDebug() << "error header";
    else
        asyncRead((uint8_t *)m_buf.data(), m_header.c_size, "processCommand", 0);
}

void USBChannel::processCommand()
{
    uint8_t *data = (uint8_t *)m_buf.data();
    switch (m_header.c_cmd) {
    case SC_USB_DEVICE_LIST_QUERY:
        processDeviceListQuery();
        break;
    case SC_USB_OPEN:
        processDeviceOpen(data, m_header.c_size);
        break;
        /*
    case SC_USB_CLOSE:
        processDeviceClose(data, m_header.c_size);
        break;
        */
    case SC_USB_RESET:
        processDeviceReset(data, m_header.c_size);
        break;
    case SC_USB_SET_CONFIG:
        processDeviceSetConfig(data, m_header.c_size);
        break;
    case SC_USB_CLAIM_INTERFACE:
        processDeviceClaimInterface(data, m_header.c_size);
        break;
        /*
    case SC_USB_RELEASE_INTERFACE:
        processDeviceReleaseInterface(data, m_header.c_size);
        break;
    case SC_USB_SET_INTERFACE:
        processDeviceSetInterface(data, m_header.c_size);
        break;
        */
    case SC_USB_QUEUE_URB:
        processDeviceQueueURB(data, m_header.c_size);
        break;
    case SC_USB_REAP_URB:
        processDeviceReapURB(data, m_header.c_size);
        break;
    case SC_USB_CLEAR_HALTED_EP:
        processDeviceClearHaltedEP(data, m_header.c_size);
        break;
    case SC_USB_CANCEL_URB:
        processDeviceCancelURB(data, m_header.c_size);
        break;
    default:
        qDebug() << Q_FUNC_INFO << "not handled cmd" << m_header.c_cmd;
    }
    asyncRead((uint8_t *)&m_header, sizeof(m_header), "processHeader", 0);
}

void USBChannel::processDeviceListQuery()
{
    QByteArray buf;
    QList<USBDevice *> dlist;
    USBDevice *device;
    pac_hdr cmd;
    pac_usb_device_list list;
    pac_usb_device dev;
    int numAttached = 0;

    dlist = USBMonitor::instance()->getDeviceList();

    foreach (device, dlist) {
        if (device->isAttached())
            numAttached++;
    }

    cmd.c_magic = SVP_MAGIC;
    cmd.c_cmd = SC_USB_DEVICE_LIST_RESULT;
    cmd.c_size = sizeof(list) + numAttached * sizeof(dev);
    buf.append((const char *)&cmd, sizeof(cmd));

    list.count = numAttached;
    buf.append((const char *)&list, sizeof(list));

    foreach (device, dlist) {
        if (!device->isAttached())
            continue;
        dev.id = device->id;
        dev.vid = device->vid;
        dev.pid = device->pid;
        dev.usb_version = device->usbVersion;
        dev.klass = device->klass;
        dev.sub_class = device->subClass;
        dev.protocol = device->protocol;
        dev.port = device->port;
        dev.revision = device->revision;
        dev.speed = device->speed;
        strcpy(dev.vender_name, device->vendorName);
        strcpy(dev.product_name, device->productName);
        strcpy(dev.serial_number, device->serialNumber);
        buf.append((const char *)&dev, sizeof(dev));
//        qDebug() << "report attached device" << dev.vender_name;
    }

    USBMonitor::instance()->putDeviceList(dlist);

    write((const uint8_t *)buf.constData(), buf.size());
}

void USBChannel::processDeviceOpen(uint8_t *data, uint32_t size)
{
    QByteArray buf;
    pac_usb_ids *ids = (pac_usb_ids *)data;
    pac_hdr cmd;
    pac_usb_status status;

//    qDebug() << "DeviceOpen" << ids->dev_id;

    cmd.c_magic = SVP_MAGIC;
    cmd.c_cmd = SC_USB_OPEN_STATUS;
    cmd.c_size = sizeof(status);
    buf.append((const char *)&cmd, sizeof(cmd));

    status.dev_id = ids->dev_id;
    status.status = 0;
    buf.append((const char *)&status, sizeof(status));

    write((const uint8_t *)buf.constData(), buf.size());
}

void USBChannel::processDeviceReset(uint8_t *data, uint32_t size)
{
    QByteArray buf;
    pac_usb_ids *ids = (pac_usb_ids *)data;
    pac_hdr cmd;
    pac_usb_status status;

//    qDebug() << "DeviceReset" << ids->dev_id;

    cmd.c_magic = SVP_MAGIC;
    cmd.c_cmd = SC_USB_OPEN_STATUS;
    cmd.c_size = sizeof(status);
    buf.append((const char *)&cmd, sizeof(cmd));

    USBDevice *device = USBMonitor::instance()->getDevice(ids->dev_id);
    if (!device) {
        qDebug() << "    not found device with id" << ids->dev_id;
        return;
    }
    status.status = device->reset();
    USBMonitor::instance()->putDevice(device);

    status.dev_id = ids->dev_id;
    buf.append((const char *)&status, sizeof(status));

    write((const uint8_t *)buf.constData(), buf.size());
}

void USBChannel::processDeviceSetConfig(uint8_t *data, uint32_t size)
{
    QByteArray buf;
    pac_hdr cmd;
    pac_usb_status status;
    pac_usb_set_config *p_setConf = (pac_usb_set_config *)data;

    cmd.c_magic = SVP_MAGIC;
    cmd.c_cmd = SC_USB_OPEN_STATUS;
    cmd.c_size = sizeof(status);
    buf.append((const char *)&cmd, sizeof(cmd));

    USBDevice *device = USBMonitor::instance()->getDevice(p_setConf->dev_id);
    if (!device) {
        qDebug() << "    not found device with id" << p_setConf->dev_id;
        return;
    }
    status.status = device->setConfig(p_setConf->config);
    USBMonitor::instance()->putDevice(device);

    status.dev_id = p_setConf->dev_id;
    buf.append((const char *)&status, sizeof(status));

    write((const uint8_t *)buf.constData(), buf.size());
}

void USBChannel::processDeviceClaimInterface(uint8_t *data, uint32_t size)
{
    QByteArray buf;
    pac_hdr cmd;
    pac_usb_status status;
    pac_usb_claim_interface *p_claimIface = (pac_usb_claim_interface *)data;

    cmd.c_magic = SVP_MAGIC;
    cmd.c_cmd = SC_USB_OPEN_STATUS;
    cmd.c_size = sizeof(status);
    buf.append((const char *)&cmd, sizeof(cmd));

    USBDevice *device = USBMonitor::instance()->getDevice(p_claimIface->dev_id);
    if (!device) {
        qDebug() << "    not found device with id" << p_claimIface->dev_id;
        return;
    }
    status.status = device->claimInterface(p_claimIface->iface);
    USBMonitor::instance()->putDevice(device);

    status.dev_id = p_claimIface->dev_id;
    buf.append((const char *)&status, sizeof(status));

    write((const uint8_t *)buf.constData(), buf.size());
}

void USBChannel::processDeviceQueueURB(uint8_t *data, uint32_t size)
{
    pac_usb_queue_urb *p_urb = (pac_usb_queue_urb *)data;
//    uint32_t remain = size - sizeof(*p_urb) - p_urb->datalen;

    USBDevice *device = USBMonitor::instance()->getDevice(p_urb->dev_id);
    if (!device) {
        qDebug() << "    not found device with id" << p_urb->dev_id;
        return;
    }
    device->queueURB(p_urb);
    USBMonitor::instance()->putDevice(device);
}

void USBChannel::processDeviceReapURB(uint8_t *data, uint32_t size)
{
    return;
}

void USBChannel::processDeviceClearHaltedEP(uint8_t *data, uint32_t size)
{
    QByteArray buf;
    pac_usb_clear_halted_ep *clr = (pac_usb_clear_halted_ep *)data;
    pac_hdr cmd;
    pac_usb_status status;

//    qDebug() << "DeviceClearHaltedEP" << clr->dev_id << clr->ep;

    cmd.c_magic = SVP_MAGIC;
    cmd.c_cmd = SC_USB_CLEAR_HALTED_EP_STATUS;
    cmd.c_size = sizeof(status);
    buf.append((const char *)&cmd, sizeof(cmd));

    USBDevice *device = USBMonitor::instance()->getDevice(clr->dev_id);
    if (!device) {
        qDebug() << "    not found device with id" << clr->dev_id;
        return;
    }
    status.status = device->clearHaltedEP(clr->ep);
    USBMonitor::instance()->putDevice(device);

    status.dev_id = clr->dev_id;
    buf.append((const char *)&status, sizeof(status));

    write((const uint8_t *)buf.constData(), buf.size());
}

void USBChannel::processDeviceCancelURB(uint8_t *data, uint32_t size)
{
    pac_usb_cancel_urb *p_urb = (pac_usb_cancel_urb *)data;

    USBDevice *device = USBMonitor::instance()->getDevice(p_urb->dev_id);
    if (!device) {
        qDebug() << "    not found device with id" << p_urb->dev_id;
        return;
    }
    device->cancelURB(p_urb);
    USBMonitor::instance()->putDevice(device);
}

#include "USBChannel.moc"
