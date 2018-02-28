#ifndef GUEST_CHANNEL_H
#define GUEST_CHANNEL_H

#include <stdint.h>
#include <string.h>
#include <windows.h>

#define CHANNEL_EVENT_CANCELLED    0    /* Event was cancelled by FN_EVENT_CANCEL. */
#define CHANNEL_EVENT_UNREGISTERED 1    /* Channel was unregistered on host. */
#define CHANNEL_EVENT_RECV         2    /* Data is available for receiving. */
#define CHANNEL_EVENT_USER         1000 /* Base of channel specific events. */

/** Success. */
#define VINF_SUCCESS                        0
/** Invalid parameter. */
#define VERR_INVALID_PARAMETER              (-2)
/** Memory allocation failed. */
#define VERR_NO_MEMORY                      (-8)
/** Incorrect call order. */
#define VERR_WRONG_ORDER                    (-22)

#define IOCTL_CODE(DeviceType, Function, Method, Access, DataSize_ignored) \
  ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define VBOXGUEST_IOCTL_CODE_(Function, Size)      IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2048 + (Function), METHOD_BUFFERED, FILE_WRITE_ACCESS, 0)
#define VBOXGUEST_IOCTL_STRIP_SIZE(Code)           (Code)

#define VBOXGUEST_IOCTL_FLAG     0

#define VBOXGUEST_IOCTL_CODE(Function, Size)       VBOXGUEST_IOCTL_CODE_((Function) | VBOXGUEST_IOCTL_FLAG, Size)

#define VBOXGUEST_IOCTL_HGCM_CONNECT               VBOXGUEST_IOCTL_CODE(16, sizeof(VBoxGuestHGCMConnectInfo))

/** IOCTL to VBoxGuest to disconnect from a HGCM service. */
#define VBOXGUEST_IOCTL_HGCM_DISCONNECT            VBOXGUEST_IOCTL_CODE(17, sizeof(VBoxGuestHGCMDisconnectInfo))

/** IOCTL to VBoxGuest to make a call to a HGCM service.
 * @see VBoxGuestHGCMCallInfo */
#define VBOXGUEST_IOCTL_HGCM_CALL(Size)            VBOXGUEST_IOCTL_CODE(18, (Size))

/** IOCTL to VBoxGuest to make a timed call to a HGCM service. */
#define VBOXGUEST_IOCTL_HGCM_CALL_TIMED(Size)      VBOXGUEST_IOCTL_CODE(20, (Size))

/** IOCTL to VBoxGuest passed from the Kernel Mode driver, but containing a user mode data in VBoxGuestHGCMCallInfo
 * the driver received from the UM. Called in the context of the process passing the data.
 * @see VBoxGuestHGCMCallInfo */
#define VBOXGUEST_IOCTL_HGCM_CALL_USERDATA(Size)      VBOXGUEST_IOCTL_CODE(21, (Size))

/** Get the pointer to the first HGCM parameter.  */
#define VBOXGUEST_HGCM_CALL_PARMS(a)             ( (HGCMFunctionParameter   *)((uint8_t *)(a) + sizeof(VBoxGuestHGCMCallInfo)) )

/*
 * Host calls.
 */
#define VBOX_HOST_CHANNEL_HOST_FN_REGISTER      1
#define VBOX_HOST_CHANNEL_HOST_FN_UNREGISTER    2

/*
 * Guest calls.
 */
#define VBOX_HOST_CHANNEL_FN_ATTACH       1 /* Attach to a channel. */
#define VBOX_HOST_CHANNEL_FN_DETACH       2 /* Detach from the channel. */
#define VBOX_HOST_CHANNEL_FN_SEND         3 /* Send data to the host. */
#define VBOX_HOST_CHANNEL_FN_RECV         4 /* Receive data from the host. */
#define VBOX_HOST_CHANNEL_FN_CONTROL      5 /* Generic data exchange using a channel instance. */
#define VBOX_HOST_CHANNEL_FN_EVENT_WAIT   6 /* Blocking wait for a host event. */
#define VBOX_HOST_CHANNEL_FN_EVENT_CANCEL 7 /* Cancel the blocking wait. */
#define VBOX_HOST_CHANNEL_FN_QUERY        8 /* Generic data exchange using a channel name. */

/*
 * The host event ids for the guest.
 */
#define VBOX_HOST_CHANNEL_EVENT_CANCELLED    0    /* Event was cancelled by FN_EVENT_CANCEL. */
#define VBOX_HOST_CHANNEL_EVENT_UNREGISTERED 1    /* Channel was unregistered on host. */
#define VBOX_HOST_CHANNEL_EVENT_RECV         2    /* Data is available for receiving. */
#define VBOX_HOST_CHANNEL_EVENT_USER         1000 /* Base of channel specific events. */

typedef enum
{
    VMMDevHGCMParmType_Invalid            = 0,
    VMMDevHGCMParmType_32bit              = 1,
    VMMDevHGCMParmType_64bit              = 2,
    VMMDevHGCMParmType_PhysAddr           = 3,  /**< @deprecated Doesn't work, use PageList. */
    VMMDevHGCMParmType_LinAddr            = 4,  /**< In and Out */
    VMMDevHGCMParmType_LinAddr_In         = 5,  /**< In  (read;  host<-guest) */
    VMMDevHGCMParmType_LinAddr_Out        = 6,  /**< Out (write; host->guest) */
    VMMDevHGCMParmType_LinAddr_Locked     = 7,  /**< Locked In and Out */
    VMMDevHGCMParmType_LinAddr_Locked_In  = 8,  /**< Locked In  (read;  host<-guest) */
    VMMDevHGCMParmType_LinAddr_Locked_Out = 9,  /**< Locked Out (write; host->guest) */
    VMMDevHGCMParmType_PageList           = 10, /**< Physical addresses of locked pages for a buffer. */
} HGCMFunctionParameterType;

typedef enum {
    VMMDevHGCMLoc_Invalid    = 0,
    VMMDevHGCMLoc_LocalHost  = 1,
    VMMDevHGCMLoc_LocalHost_Existing = 2,
} HGCMServiceLocationType;

#pragma pack(1)
typedef struct
{
    char achName[128]; /**< This is really szName. */
} HGCMServiceLocationHost;

typedef struct HGCMServiceLocation
{
    /** Type of the location. */
    HGCMServiceLocationType type;

    union
    {
        HGCMServiceLocationHost host;
    } u;
} HGCMServiceLocation;

typedef struct VBoxGuestHGCMConnectInfo
{
    int32_t result;           /**< OUT */
    HGCMServiceLocation Loc;  /**< IN */
    uint32_t u32ClientID;     /**< OUT */
} VBoxGuestHGCMConnectInfo;

typedef struct VBoxGuestHGCMDisconnectInfo
{
    int32_t result;           /**< OUT */
    uint32_t u32ClientID;     /**< IN */
} VBoxGuestHGCMDisconnectInfo;

typedef struct VBoxGuestHGCMCallInfo
{
    int32_t result;           /**< OUT Host HGCM return code.*/
    uint32_t u32ClientID;     /**< IN  The id of the caller. */
    uint32_t u32Function;     /**< IN  Function number. */
    uint32_t cParms;          /**< IN  How many parms. */
    /* Parameters follow in form HGCMFunctionParameter aParms[cParms] */
} VBoxGuestHGCMCallInfo;

typedef struct
{
    HGCMFunctionParameterType type;
    union
    {
        uint32_t   value32;
        uint64_t   value64;
        struct
        {
            uint32_t size;

            union
            {
                uint32_t physAddr;
                uint32_t linearAddr;
            } u;
        } Pointer;
        struct
        {
            uint32_t size;   /**< Size of the buffer described by the page list. */
            uint32_t offset; /**< Relative to the request header, valid if size != 0. */
        } PageList;
    } u;
#ifdef __cplusplus
    void SetUInt32(uint32_t u32)
    {
        type = VMMDevHGCMParmType_32bit;
        u.value64 = 0; /* init unused bits to 0 */
        u.value32 = u32;
    }

    int GetUInt32(uint32_t *pu32)
    {
        if (type == VMMDevHGCMParmType_32bit)
        {
            *pu32 = u.value32;
            return VINF_SUCCESS;
        }
        return VERR_INVALID_PARAMETER;
    }

    void SetUInt64(uint64_t u64)
    {
        type      = VMMDevHGCMParmType_64bit;
        u.value64 = u64;
    }

    int GetUInt64(uint64_t *pu64)
    {
        if (type == VMMDevHGCMParmType_64bit)
        {
            *pu64 = u.value64;
            return VINF_SUCCESS;
        }
        return VERR_INVALID_PARAMETER;
    }

    void SetPtr(void *pv, uint32_t cb)
    {
        type                    = VMMDevHGCMParmType_LinAddr;
        u.Pointer.size          = cb;
        u.Pointer.u.linearAddr  = (uintptr_t)pv;
    }
#endif /* __cplusplus */
} HGCMFunctionParameter;

typedef struct VBoxHostChannelAttach
{
    VBoxGuestHGCMCallInfo hdr;
    HGCMFunctionParameter name;   /* IN linear ptr: Channel name utf8 nul terminated. */
    HGCMFunctionParameter flags;  /* IN uint32_t: Channel specific flags. */
    HGCMFunctionParameter handle; /* OUT uint32_t: The channel handle. */
} VBoxHostChannelAttach;

typedef struct VBoxHostChannelDetach
{
    VBoxGuestHGCMCallInfo hdr;
    HGCMFunctionParameter handle; /* IN uint32_t: The channel handle. */
} VBoxHostChannelDetach;

typedef struct VBoxHostChannelSend
{
    VBoxGuestHGCMCallInfo hdr;
    HGCMFunctionParameter handle; /* IN uint32_t: The channel handle. */
    HGCMFunctionParameter data;   /* IN linear pointer: Data to be sent. */
} VBoxHostChannelSend;

typedef struct VBoxHostChannelRecv
{
    VBoxGuestHGCMCallInfo hdr;
    HGCMFunctionParameter handle;        /* IN uint32_t: The channel handle. */
    HGCMFunctionParameter data;          /* OUT linear pointer: Buffer for data to be received. */
    HGCMFunctionParameter sizeReceived;  /* OUT uint32_t: Bytes received. */
    HGCMFunctionParameter sizeRemaining; /* OUT uint32_t: Bytes remaining in the channel. */
} VBoxHostChannelRecv;

typedef struct VBoxHostChannelControl
{
    VBoxGuestHGCMCallInfo hdr;
    HGCMFunctionParameter handle; /* IN uint32_t: The channel handle. */
    HGCMFunctionParameter code;   /* IN uint32_t: The channel specific control code. */
    HGCMFunctionParameter parm;   /* IN linear pointer: Parameters of the function. */
    HGCMFunctionParameter data;   /* OUT linear pointer: Buffer for results. */
    HGCMFunctionParameter sizeDataReturned; /* OUT uint32_t: Bytes returned in the 'data' buffer. */
} VBoxHostChannelControl;

typedef struct VBoxHostChannelEventWait
{
    VBoxGuestHGCMCallInfo hdr;
    HGCMFunctionParameter handle;       /* OUT uint32_t: The channel which generated the event. */
    HGCMFunctionParameter id;           /* OUT uint32_t: The event VBOX_HOST_CHANNEL_EVENT_*. */
    HGCMFunctionParameter parm;         /* OUT linear pointer: Parameters of the event. */
    HGCMFunctionParameter sizeReturned; /* OUT uint32_t: Size of the parameters. */
} VBoxHostChannelEventWait;

typedef struct VBoxHostChannelEventCancel
{
    VBoxGuestHGCMCallInfo hdr;
} VBoxHostChannelEventCancel;
#pragma pack()

static inline void HGCMParmUInt32Set(HGCMFunctionParameter *pParm, uint32_t u32)
{
    pParm->type = VMMDevHGCMParmType_32bit;
    pParm->u.value64 = 0; /* init unused bits to 0 */
    pParm->u.value32 = u32;
}

static inline int HGCMParmUInt32Get(HGCMFunctionParameter *pParm, uint32_t *pu32)
{
    if (pParm->type == VMMDevHGCMParmType_32bit)
    {
        *pu32 = pParm->u.value32;
        return VINF_SUCCESS;
    }
    return VERR_INVALID_PARAMETER;
}

static inline void HGCMParmUInt64Set(HGCMFunctionParameter *pParm, uint64_t u64)
{
    pParm->type      = VMMDevHGCMParmType_64bit;
    pParm->u.value64 = u64;
}


static inline int HGCMParmUInt64Get(HGCMFunctionParameter *pParm, uint64_t *pu64)
{
    if (pParm->type == VMMDevHGCMParmType_64bit)
    {
        *pu64 = pParm->u.value64;
        return VINF_SUCCESS;
    }
    return VERR_INVALID_PARAMETER;
}

static inline void HGCMParmPtrSet(HGCMFunctionParameter *pParm, void *pv, uint32_t cb)
{
    pParm->type                    = VMMDevHGCMParmType_LinAddr;
    pParm->u.Pointer.size          = cb;
    pParm->u.Pointer.u.linearAddr  = (uintptr_t)pv;
}

static inline void HGCMParmPtrSetString(HGCMFunctionParameter *pParm, const char *psz)
{
    pParm->type                    = VMMDevHGCMParmType_LinAddr_In;
    pParm->u.Pointer.size          = (uint32_t)strlen(psz) + 1;
    pParm->u.Pointer.u.linearAddr  = (uintptr_t)psz;
}

int svp_channel_open(const char *name, int flags);
void svp_channel_close(int channel);
int svp_channel_write(int channel, void *buf, int size);
int svp_channel_read(int channel, void *buf, int size);
int svp_channel_ioctl(int channel, int req, void *data, int size);
int svp_channel_wait_event(int *channel, void *data, int *size);
/* cancel a waiting thread */
int svp_channel_cancel_wait();

#endif // GUEST_CHANNEL_H
