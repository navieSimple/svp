#include "USBDevice.h"
#include "USBUtils.h"
#include "libwdi/libwdi.h"
#include "libusbx-1.0/libusb.h"
#include "svp.h"
#include "net_packet.h"
#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <stdint.h>

static uint32_t g_id = 0;

#define DRIVER_PATH "usb_driver"
#define DRIVER_INF "usb_device.inf"

USBDevice::USBDevice(libusb_device *dev, QObject *parent) :
    QObject(parent),
    cnt(1),
    m_dev(dev),
    m_handle(0),
    m_attached(false)
{
    libusb_device_descriptor desc;
    int rc;
    uint32_t ports = 0;

    m_dev = libusb_ref_device(dev);
    rc = libusb_get_device_descriptor(dev, &desc);
    if (rc < 0) {
        qWarning() << QString("failed to get descriptor for device %1:%2")
                      .arg(desc.idVendor, 4, 10, QLatin1Char('0'))
                      .arg(desc.idProduct, 4, 10, QLatin1Char('0'));
        return;
    }
    usbVersion = desc.bcdUSB;
    klass = desc.bDeviceClass;
    subClass = desc.bDeviceSubClass;
    protocol = desc.bDeviceProtocol;
    maxPacketSize = desc.bMaxPacketSize0;
    vid = desc.idVendor;
    pid = desc.idProduct;
    revision = desc.bcdDevice;

    port = libusb_get_port_number(dev);
    // TODO: fix usb device speed detection
#if 0
    speed = FullSpeed;
#else
    speed = (USBSpeed)libusb_get_device_speed(dev);
#endif

    id = g_id++;
    libusb_get_port_numbers(dev, (uint8_t *)&ports, sizeof(ports));
    key = MakeKey(desc.idVendor, desc.idProduct, ports);

    rc = libusb_open(m_dev, &m_handle);
    if (rc) {
        strncpy(vendorName, wdi_get_vendor_name(vid), sizeof(vendorName));
        vendorName[sizeof(vendorName) - 1] = 0;
        strcpy(productName, "Unknown");
    } else {
        loadStringDescriptors();
    }
}

USBDevice::~USBDevice()
{
    if (m_handle)
        libusb_close(m_handle);
    libusb_unref_device(m_dev);
}

void USBDevice::ref()
{
    cnt++;
}

void USBDevice::deref()
{
    cnt--;
    if (cnt == 0)
        delete this;
}

bool USBDevice::isAttached() const
{
    return m_attached;
}

bool USBDevice::attachDriver()
{
    int rc;
    wdi_device_info *list;
    wdi_device_info *device;
    wdi_options_create_list optCreateList = {TRUE, FALSE, TRUE};
    wdi_options_prepare_driver optPrepareDriver = {WDI_LIBUSBK, 0, 0, FALSE, FALSE, 0, FALSE};

    if (m_attached)
        return true;
    rc = wdi_create_list(&list, &optCreateList);
    if (rc) {
        qWarning() << "wdi error:" << wdi_strerror(rc);
        return false;
    }
    for (device = list; device; device = device->next) {
        if (device->vid == vid && device->pid == pid) {
            break;
        }
    }
    if (device && isAttachedDriver(device->driver)) {
        m_attached = true;
        return true;
    }
    if (!device) {
        qWarning() << QString("wdi: not found device %1:%2").arg(vid, 4, 16, QChar('0'))
                      .arg(vid, 4, 16, QChar('0'));
        return false;
    }
    rc = wdi_prepare_driver(device, DRIVER_PATH, DRIVER_INF, &optPrepareDriver);
    if (rc) {
        qWarning() << "wdi: prepare driver error:" << wdi_strerror(rc);
        goto out;
    }
    rc = wdi_install_driver(device, DRIVER_PATH, DRIVER_INF, 0);
    if (rc) {
        qWarning() << "wdi: install driver error:" << wdi_strerror(rc);
        goto out;
    }
    libusb_open(m_dev, &m_handle);
    libusb_reset_device(m_handle);
    loadStringDescriptors();
    m_attached = true;
    qDebug() << QString("device %1[%2] attached").arg(productName).arg(vendorName);
out:
    wdi_destroy_list(list);
    return m_attached;
}

void USBDevice::detachDriver()
{
    if (!m_attached)
        return;
    qWarning() << "detach driver not implemented";
    m_attached = false;
}

int USBDevice::reset()
{
    if (thread() == QThread::currentThread())
        return __reset();
    else {
        int rc = -1;
        metaObject()->invokeMethod(this, "__reset", Qt::BlockingQueuedConnection,
                                   Q_RETURN_ARG(int, rc));
        return rc;
    }
}

int USBDevice::setConfig(uint8_t config)
{
    if (thread() == QThread::currentThread())
        return __setConfig(config);
    else {
        int rc = -1;
        metaObject()->invokeMethod(this, "__setConfig", Qt::BlockingQueuedConnection,
                                   Q_RETURN_ARG(int, rc),
                                   Q_ARG(uint8_t, config));
        return rc;
    }
}

int USBDevice::claimInterface(uint8_t iface)
{
    if (thread() == QThread::currentThread())
        return __reset();
    else {
        int rc = -1;
        metaObject()->invokeMethod(this, "__claimInterface", Qt::BlockingQueuedConnection,
                                   Q_RETURN_ARG(int, rc),
                                   Q_ARG(uint8_t, iface));
        return rc;
    }
}

void USBDevice::queueURB(pac_usb_queue_urb *urb)
{
    pac_usb_queue_urb *copy = (pac_usb_queue_urb *)malloc(sizeof(*urb) + urb->datalen);
    memcpy(copy, urb, sizeof(*urb) + urb->datalen);
    if (QThread::currentThread() == thread())
        __queueURB(copy);
    else
        metaObject()->invokeMethod(this, "__queueURB", Qt::QueuedConnection,
                                   Q_ARG(pac_usb_queue_urb *, copy));
}

void USBDevice::cancelURB(pac_usb_cancel_urb *urb)
{
    if (QThread::currentThread() == thread())
        __cancelURB(urb);
    else
        metaObject()->invokeMethod(this, "__cancelURB", Qt::BlockingQueuedConnection, Q_ARG(pac_usb_cancel_urb *, urb));
}

int USBDevice::clearHaltedEP(uint8_t ep)
{
    if (QThread::currentThread() == thread())
        return __clearHaltedEP(ep);
    else {
        int rc = -1;
        metaObject()->invokeMethod(this, "__clearHaltedEP", Qt::BlockingQueuedConnection,
                                   Q_RETURN_ARG(int, rc),
                                   Q_ARG(uint8_t, ep));
        return rc;
    }
}

void USBDevice::onTransferFinished(libusb_transfer *transfer)
{
#if 0
    qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
             << QString("Urb reply: status %1 len %2 actualLen %3")
                .arg(transfer->status).arg(transfer->length).arg(transfer->actual_length);
#endif
    USBDevice *device = (USBDevice *)transfer->user_data;
    uint32_t handle;
    pac_usb_urb *urb = (pac_usb_urb *)malloc(sizeof(*urb));
    if (!device)
        return;

    handle = device->m_handleMap.value(transfer, -1);
    device->m_handleMap.remove(transfer);
    device->m_transferMap.remove(handle);

    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        urb->error = SUS_OK;
        break;
    case LIBUSB_TRANSFER_ERROR:
        urb->error = SUS_ERR;
        break;
    case LIBUSB_TRANSFER_TIMED_OUT:
        urb->error = SUS_DNR;
        break;
    case LIBUSB_TRANSFER_CANCELLED:
        libusb_free_transfer(transfer);
        return;
    case LIBUSB_TRANSFER_STALL:
        urb->error = SUS_STALL;
        break;
    case LIBUSB_TRANSFER_NO_DEVICE:
        urb->error = SUS_DNR;
        break;
    case LIBUSB_TRANSFER_OVERFLOW:
        urb->error = SUS_DO;
        break;
    default:
        urb->error = SUS_ERR;
    }

    urb->id = device->id;
    urb->nodata = 0;
    urb->flags = SUF_LAST;
    urb->handle = handle;
    if ((transfer->type == LIBUSB_TRANSFER_TYPE_INTERRUPT || transfer->type == LIBUSB_TRANSFER_TYPE_BULK)
            && (transfer->endpoint & LIBUSB_ENDPOINT_IN))
        urb->len = transfer->actual_length;
    else
        urb->len = transfer->length;
    if (transfer->type == LIBUSB_TRANSFER_TYPE_BULK && !(transfer->endpoint & LIBUSB_ENDPOINT_IN))
        urb->nodata = 1;
    emit device->urbFinished(urb, transfer->buffer);
    libusb_free_transfer(transfer);
}

void USBDevice::loadStringDescriptors()
{
    libusb_device_descriptor desc;
    int rc;

    strncpy(vendorName, wdi_get_vendor_name(vid), sizeof(vendorName));
    vendorName[sizeof(vendorName) - 1] = 0;
    if (!m_handle)
        return;
    rc = libusb_get_device_descriptor(m_dev, &desc);
    if (rc)
        return;
    libusb_get_string_descriptor_ascii(m_handle, desc.iProduct, (unsigned char *)productName, sizeof(productName));
    productName[sizeof(productName) - 1] = 0;
    libusb_get_string_descriptor_ascii(m_handle, desc.iSerialNumber, (unsigned char *)serialNumber, sizeof(serialNumber));
    serialNumber[sizeof(serialNumber) - 1] = 0;
}

bool USBDevice::isAttachedDriver(const char *driverName)
{
    int i;
    const char* libusbName[] = { "WinUSB", "libusb0", "libusbK" };
    int numName = sizeof(libusbName) / sizeof(libusbName[0]);

    if (!driverName)
        return false;

    for (i = 0; i < numName; i++)
        if (strcmp(driverName, libusbName[i]) == 0)
            return true;
    return false;
}

int USBDevice::__reset()
{
    qDebug() << Q_FUNC_INFO;
    int rc;
    if (!m_handle)
        return -1;
    rc = libusb_reset_device(m_handle);
    if (rc) {
        qDebug() << "    " << libusb_error_name(rc);
        return -1;
    }
    return 0;
}

int USBDevice::__setConfig(uint8_t config)
{
    qDebug() << Q_FUNC_INFO << config;
    int rc;
    if (!m_handle)
        return -1;
    rc = libusb_set_configuration(m_handle, config);
    if (rc) {
        qDebug() << "    " << libusb_error_name(rc);
        return -1;
    }
    return 0;
}

int USBDevice::__claimInterface(uint8_t iface)
{
    qDebug() << Q_FUNC_INFO << iface;
    int rc;
    if (!m_handle)
        return -1;
    rc = libusb_claim_interface(m_handle, iface);
    if (rc) {
        qDebug() << "    " << libusb_error_name(rc);
        return -1;
    }
    return 0;
}

void USBDevice::__queueURB(pac_usb_queue_urb *urb)
{
    uint8_t *data = (uint8_t *)urb + sizeof(*urb);
    uint8_t *buf;
    int rc;
    libusb_transfer *transfer = 0;
    libusb_transfer_type utype;

#if 0
    qDebug() << Q_FUNC_INFO << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
             << QString("type %1 ep %2 dir %3 urblen %4 datalen %5")
                .arg(urb->type).arg(urb->ep).arg(urb->direction).arg(urb->urblen).arg(urb->datalen);
#endif

    if (!m_handle) {
        qWarning() << "invalid libusb_device_handle for device with id" << id;
        return;
    }

    transfer = libusb_alloc_transfer(0);
    buf = (uint8_t *)malloc(urb->urblen);
    memcpy(buf, data, urb->datalen);
    switch (urb->type) {
    case SUXT_CTRL:
    case SUXT_MSG:
        utype = LIBUSB_TRANSFER_TYPE_CONTROL;
        break;
    case SUXT_BULK:
        utype = LIBUSB_TRANSFER_TYPE_BULK;
        break;
    case SUXT_INTR:
        utype = LIBUSB_TRANSFER_TYPE_INTERRUPT;
        break;
    default:
        qDebug() << "not supported xfer type" << urb->type;
        goto out_err;
    }
    transfer->dev_handle = m_handle;
    transfer->flags = 0;
    transfer->endpoint = urb->ep;
    if (urb->direction == SUD_IN)
        transfer->endpoint |= LIBUSB_ENDPOINT_IN;
    transfer->type = utype;
    transfer->timeout = -1;
    transfer->length = urb->urblen;
    transfer->callback = USBDevice::onTransferFinished;
    transfer->user_data = this;
    transfer->buffer = buf;
    m_handleMap.insert(transfer, urb->handle);
    m_transferMap.insert(urb->handle, transfer);
    rc = libusb_submit_transfer(transfer);
    if (rc) {
        qDebug() << "libusb_submit_transfer error:" << libusb_error_name(rc);
        goto out_err;
    }
//    qDebug() << "transfer submitted";
    free(urb); // queued connection
    return;

out_err:
    free(urb); // queued connection
    free(buf);
}

void USBDevice::__cancelURB(pac_usb_cancel_urb *urb)
{
    qDebug() << Q_FUNC_INFO;
    libusb_transfer *transfer = m_transferMap.value(urb->handle);
    if (!transfer) {
        qWarning() << "    error: not found urb handle" << urb->handle;
        return;
    }
    libusb_cancel_transfer(transfer);
}

int USBDevice::__clearHaltedEP(uint8_t ep)
{
    qDebug() << Q_FUNC_INFO << ep;
    int rc;
    if (!m_handle)
        return -1;
    rc = libusb_clear_halt(m_handle, ep);
    if (rc) {
        qDebug() << "    " << libusb_error_name(rc);
        return -1;
    }
    return 0;
}
