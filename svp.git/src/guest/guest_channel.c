#include "guest_channel.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#include <winnt.h>
#endif

#define dzlog_debug(fmt, ...) fprintf(stderr, fmt"\n", #__VA_ARGS__)

#ifdef WIN32
#define VBOXGUEST_DEVICE_NAME          "\\\\.\\VBoxGuest"
static HANDLE g_hFile = INVALID_HANDLE_VALUE;
#endif

static int g_clientId = -1;

static int dev_init()
{
#if defined(WIN32)
    if (g_hFile != INVALID_HANDLE_VALUE)
        return 0;

    /*
     * Have to use CreateFile here as we want to specify FILE_FLAG_OVERLAPPED
     * and possible some other bits not available thru iprt/file.h.
     */
    HANDLE hFile = CreateFile(VBOXGUEST_DEVICE_NAME,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                              NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return -1;
    g_hFile = hFile;

    return 0;
#else
    dzlog_debug("not supported platform");
    return -1;
#endif
}

static void dev_fini()
{
#ifdef WIN32
    HANDLE hFile = g_hFile;
    g_hFile = INVALID_HANDLE_VALUE;
    CloseHandle(hFile);
#else
    dzlog_debug("not supported platform");
#endif
}

static int dev_ioctl(int req, void *data, int size)
{
#ifdef WIN32
    DWORD cbReturned = 0;
    if (!DeviceIoControl(g_hFile, req, data, (DWORD)size, data, (DWORD)size, &cbReturned, NULL)) {
        DWORD LastErr = GetLastError();
        return LastErr;
    }
    return 0;
#else
    dzlog_debug("not supported platform");
    return -1;
#endif
}

__attribute__((constructor))
static void svp_channel_init()
{
    int ret;
    ret = dev_init();
    if (ret == -1) {
        dzlog_debug("device init error");
        return;
    }
    VBoxGuestHGCMConnectInfo connectInfo;
    memset(&connectInfo, 0, sizeof(connectInfo));

    connectInfo.result = VERR_WRONG_ORDER;
    connectInfo.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    strcpy(connectInfo.Loc.u.host.achName, "VBoxHostChannel");
    connectInfo.u32ClientID = 0;

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CONNECT, &connectInfo, sizeof(connectInfo));

    if (rc >= 0) {
        rc = connectInfo.result;
        if (rc >= 0)
            g_clientId = connectInfo.u32ClientID;
    }

    return;
}

__attribute__((destructor))
static void svp_channel_fini()
{
    dev_fini();
    VBoxGuestHGCMDisconnectInfo disconnectInfo;
    disconnectInfo.result = VERR_WRONG_ORDER;
    disconnectInfo.u32ClientID = g_clientId;

    dev_ioctl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &disconnectInfo, sizeof(disconnectInfo));
}

int svp_channel_open(const char *name, int flags)
{
    /* Make a heap copy of the name, because HGCM can not use some of other memory types. */
    size_t cbName = strlen(name) + 1;
    char *pszCopy = (char *)malloc(cbName);
    if (pszCopy == NULL)
    {
        return VERR_NO_MEMORY;
    }

    memcpy(pszCopy, name, cbName);

    VBoxHostChannelAttach parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_ATTACH;
    parms.hdr.cParms = 3;

    HGCMParmPtrSet(&parms.name, pszCopy, (uint32_t)cbName);
    HGCMParmUInt32Set(&parms.flags, flags);
    HGCMParmUInt32Set(&parms.handle, -1);

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (rc >= 0)
    {
        rc = parms.handle.u.value32;
    }

    free(pszCopy);

    return rc;
}

void svp_channel_close(int channel)
{
    VBoxHostChannelDetach parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_DETACH;
    parms.hdr.cParms = 1;

    HGCMParmUInt32Set(&parms.handle, channel);

    dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));
}

int svp_channel_write(int channel, void *buf, int size)
{
    VBoxHostChannelSend parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_SEND;
    parms.hdr.cParms = 2;

    HGCMParmUInt32Set(&parms.handle, channel);
    HGCMParmPtrSet(&parms.data, buf, size);

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (rc >= 0)
    {
        rc = parms.hdr.result;
        if (rc == 0)
            rc = size;
    }

    return rc;
}

int svp_channel_read(int channel, void *buf, int size)
{
    VBoxHostChannelRecv parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_RECV;
    parms.hdr.cParms = 4;

    HGCMParmUInt32Set(&parms.handle, channel);
    HGCMParmPtrSet(&parms.data, buf, size);
    HGCMParmUInt32Set(&parms.sizeReceived, 0);
    HGCMParmUInt32Set(&parms.sizeRemaining, 0);

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (rc >= 0)
    {
        rc = parms.hdr.result;

        if (rc >= 0)
        {
/*
            *pu32SizeReceived = parms.sizeReceived.u.value32;
            *pu32SizeRemaining = parms.sizeRemaining.u.value32;
*/
            rc = parms.sizeReceived.u.value32;
        }
    }

    return rc;
}

int svp_channel_ioctl(int channel, int req, void *data, int size)
{
    VBoxHostChannelControl parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_CONTROL;
    parms.hdr.cParms = 5;

    HGCMParmUInt32Set(&parms.handle, channel);
    HGCMParmUInt32Set(&parms.code, req);
    HGCMParmPtrSet(&parms.parm, data, size);
    HGCMParmPtrSet(&parms.data, data, size);
    HGCMParmUInt32Set(&parms.sizeDataReturned, 0);

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (rc >= 0)
    {
        rc = parms.hdr.result;

        /*
        if (rc >= 0)
        {
            *pu32SizeDataReturned = parms.sizeDataReturned.u.value32;
        }
        */
    }

    return rc;
}

int svp_channel_wait_event(int *channel, void *data, int *size)
{
    VBoxHostChannelEventWait parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_EVENT_WAIT;
    parms.hdr.cParms = 4;

    HGCMParmUInt32Set(&parms.handle, 0);
    HGCMParmUInt32Set(&parms.id, 0);
    HGCMParmPtrSet(&parms.parm, data, *size);
    HGCMParmUInt32Set(&parms.sizeReturned, 0);

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (rc >= 0)
    {
        *channel = parms.handle.u.value32;
        rc = parms.id.u.value32;
        *size = parms.sizeReturned.u.value32;
    }

    return rc;
}

int svp_channel_cancel_wait()
{
    VBoxHostChannelEventCancel parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = g_clientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_EVENT_CANCEL;
    parms.hdr.cParms = 0;

    int rc = dev_ioctl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (rc >= 0)
    {
        rc = parms.hdr.result;
    }

    return rc;
}
