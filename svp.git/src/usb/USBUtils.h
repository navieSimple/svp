#ifndef USBUTILS_H
#define USBUTILS_H

#include <QList>
#include <QString>
#include <stdint.h>
#include "USBDevice.h"

static inline uint64_t MakeKey(uint16_t vid, uint16_t pid, uint32_t ports)
{
    return ((uint64_t)vid << 48) | ((uint64_t)pid << 32) | (uint64_t)ports;
}


#endif // USBUTILS_H
