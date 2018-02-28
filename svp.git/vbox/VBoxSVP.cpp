/* $Id: VBoxSVP.cpp $ */
/** @file
 * VBoxSVP - SVP VRDE module.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VRDE
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/alloca.h>
#include <iprt/ldr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/mem.h>
#include <iprt/socket.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/RemoteDesktop/VRDEOrders.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <svp.h>
#include <vrde.h>
#include <inqueue.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
class SVPServerImpl
{
public:
    SVPServerImpl()
    {
        mClients = 0;
        memset(&m_frontend, 0, sizeof(m_frontend));
        m_backend.client_logon = svpClientLogon;
        m_backend.client_logout = svpClientLogout;
        m_backend.keyboard_input = svpKeyboardInput;
        m_backend.mouse_input = svpMouseInput;
        m_backend.video_modeset = svpVideoModeSet;
        m_backend.usb_input = svpUsbInput;
        m_backend.priv = this;
        memset(&m_fbInfo, 0, sizeof(m_fbInfo));
        m_inq = inq_create();
        m_usbq = inq_create();
    }

    ~SVPServerImpl()
    {
    }

    int Init(const VRDEINTERFACEHDR *pCallbacks, void *pvCallback);

    VRDEINTERFACEHDR *GetInterface() { return &Entries.header; }

    static DECLCALLBACK(void) svpClientLogon(struct svp_backend_operations *, int id);
    static DECLCALLBACK(void) svpClientLogout(struct svp_backend_operations *, int id);
    static DECLCALLBACK(void) svpKeyboardInput(struct svp_backend_operations *, int down, uint32_t scancode);
    static DECLCALLBACK(void) svpMouseInput(struct svp_backend_operations *, int x, int y, int buttons);
    static DECLCALLBACK(void) svpVideoModeSet(struct svp_backend_operations *, int w, int h, int depth);
    static DECLCALLBACK(void) svpUsbInput(struct svp_backend_operations *, int client_id, int req, const void *data, int size);

private:
    // SVP related variables
    svp_frontend_operations m_frontend;
    svp_backend_operations m_backend;
    vrde_framebuffer_info m_fbInfo;
    inqueue *m_inq;
    inqueue *m_usbq;

    void *mCallback;
    void *mUSBIntercept;
    VRDEFRAMEBUFFERINFO mFrameInfo;
    unsigned char *mScreenBuffer;
    unsigned char *mFrameBuffer;
    uint32_t mClients;

    static uint32_t RGB2BGR(uint32_t c)
    {
        c = ((c >> 0) & 0xff) << 16 |
            ((c >> 8) & 0xff) << 8 |
            ((c >> 16) & 0xff) << 0;

        return c;
    }

    void copyFrameBuffer(int x, int y, int w, int h);

    int queryVrdeFeature(const char *pszName, char *pszValue, size_t cbValue);

    static VRDEENTRYPOINTS_4 Entries;
    VRDECALLBACKS_4 *mCallbacks;

    static DECLCALLBACK(void) VRDEDestroy(HVRDESERVER hServer);
    static DECLCALLBACK(int)  VRDEEnableConnections(HVRDESERVER hServer, bool fEnable);
    static DECLCALLBACK(void) VRDEDisconnect(HVRDESERVER hServer, uint32_t u32ClientId, bool fReconnect);
    static DECLCALLBACK(void) VRDEResize(HVRDESERVER hServer);
    static DECLCALLBACK(void) VRDEUpdate(HVRDESERVER hServer, unsigned uScreenId, void *pvUpdate,uint32_t cbUpdate);
    static DECLCALLBACK(void) VRDEColorPointer(HVRDESERVER hServer, const VRDECOLORPOINTER *pPointer);
    static DECLCALLBACK(void) VRDEHidePointer(HVRDESERVER hServer);
    static DECLCALLBACK(void) VRDEAudioSamples(HVRDESERVER hServer, const void *pvSamples, uint32_t cSamples, VRDEAUDIOFORMAT format);
    static DECLCALLBACK(void) VRDEAudioVolume(HVRDESERVER hServer, uint16_t u16Left, uint16_t u16Right);
    static DECLCALLBACK(void) VRDEUSBRequest(HVRDESERVER hServer,
                                             uint32_t u32ClientId,
                                             void *pvParm,
                                             uint32_t cbParm);
    static DECLCALLBACK(void) VRDEClipboard(HVRDESERVER hServer,
                                            uint32_t u32Function,
                                            uint32_t u32Format,
                                            void *pvData,
                                            uint32_t cbData,
                                            uint32_t *pcbActualRead);
    static DECLCALLBACK(void) VRDEQueryInfo(HVRDESERVER hServer,
                                            uint32_t index,
                                            void *pvBuffer,
                                            uint32_t cbBuffer,
                                            uint32_t *pcbOut);
    static DECLCALLBACK(void) VRDERedirect(HVRDESERVER hServer,
                                           uint32_t u32ClientId,
                                           const char *pszServer,
                                           const char *pszUser,
                                           const char *pszDomain,
                                           const char *pszPassword,
                                           uint32_t u32SessionId,
                                           const char *pszCookie);
    static DECLCALLBACK(void) VRDEAudioInOpen(HVRDESERVER hServer,
                                              void *pvCtx,
                                              uint32_t u32ClientId,
                                              VRDEAUDIOFORMAT audioFormat,
                                              uint32_t u32SamplesPerBlock);
    static DECLCALLBACK(void) VRDEAudioInClose(HVRDESERVER hServer,
                                               uint32_t u32ClientId);
    static DECLCALLBACK(int) VRDEGetInterface(HVRDESERVER hServer,
                                              const char *pszId,
                                              VRDEINTERFACEHDR *pInterface,
                                              const VRDEINTERFACEHDR *pCallbacks,
                                              void *pvContext);
};

VRDEENTRYPOINTS_4 SVPServerImpl::Entries = {
    { VRDE_INTERFACE_VERSION_4, sizeof(VRDEENTRYPOINTS_4) },
    SVPServerImpl::VRDEDestroy,
    SVPServerImpl::VRDEEnableConnections,
    SVPServerImpl::VRDEDisconnect,
    SVPServerImpl::VRDEResize,
    SVPServerImpl::VRDEUpdate,
    SVPServerImpl::VRDEColorPointer,
    SVPServerImpl::VRDEHidePointer,
    SVPServerImpl::VRDEAudioSamples,
    SVPServerImpl::VRDEAudioVolume,
    SVPServerImpl::VRDEUSBRequest,
    SVPServerImpl::VRDEClipboard,
    SVPServerImpl::VRDEQueryInfo,
    SVPServerImpl::VRDERedirect,
    SVPServerImpl::VRDEAudioInOpen,
    SVPServerImpl::VRDEAudioInClose,
    SVPServerImpl::VRDEGetInterface
};

/** Destroy the server instance.
 *
 * @param hServer The server instance handle.
 *
 * @return IPRT status code.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEDestroy(HVRDESERVER hServer)
{
    SVPServerImpl *me = (SVPServerImpl *)hServer;

    if (me->m_frontend.fini)
        me->m_frontend.fini(&me->m_frontend);

    uint32_t port = UINT32_MAX;
    me->mCallbacks->VRDECallbackProperty(me->mCallback,
                                         VRDE_SP_NETWORK_BIND_PORT,
                                         &port, sizeof(port), NULL);
    return;
}


/**
 * Query a feature and store it's value in a user supplied buffer.
 *
 * @returns VBox status code.
 * @param   pszName             The feature name.
 * @param   pszValue            The value buffer.  The buffer is not touched at
 *                              all on failure.
 * @param   cbValue             The size of the output buffer.
 */
int SVPServerImpl::queryVrdeFeature(const char *pszName, char *pszValue, size_t cbValue)
{
    union
    {
        VRDEFEATURE Feat;
        uint8_t     abBuf[SVP_ADDRESS_OPTION_MAX + sizeof(VRDEFEATURE)];
    } u;

    u.Feat.u32ClientId = 0;
    int rc = RTStrCopy(u.Feat.achInfo, SVP_ADDRESS_OPTION_MAX, pszName); AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        uint32_t cbOut = 0;
        rc = mCallbacks->VRDECallbackProperty(mCallback,
                                              VRDE_QP_FEATURE,
                                              &u.Feat,
                                              SVP_ADDRESS_OPTION_MAX,
                                              &cbOut);
        if (RT_SUCCESS(rc))
        {
            size_t cbRet = strlen(u.Feat.achInfo) + 1;
            if (cbRet <= cbValue)
                memcpy(pszValue, u.Feat.achInfo, cbRet);
            else
                rc = VERR_BUFFER_OVERFLOW;
        }
    }

    return rc;
}

static inline void convert15To32bpp(uint8_t msb, uint8_t lsb, uint8_t &r, uint8_t &g, uint8_t &b)
{
    uint16_t px = lsb << 8 | msb;
    // RGB 555 (1 bit unused)
    r = (px >> 7) & 0xf8;
    g = (px >> 2) & 0xf8;
    b = (px << 3) & 0xf8;
}

static inline void convert16To32bpp(uint8_t msb, uint8_t lsb, uint8_t &r, uint8_t &g, uint8_t &b)
{
    uint16_t px = lsb << 8 | msb;
    // RGB 565 (all bits used, 1 extra bit for green)
    r = (px >> 8) & 0xf8;
    g = (px >> 3) & 0xfc;
    b = (px << 3) & 0xf8;
}

void SVPServerImpl::copyFrameBuffer(int x, int y, int w, int h)
{
    uint32_t width = mFrameInfo.cWidth;
    uint32_t bpp = mFrameInfo.cBitsPerPixel / 8;
    uint32_t joff = y * width + x;
    uint32_t srcx, srcy, destx, desty;
    if (mFrameInfo.cBitsPerPixel == 32 || mFrameInfo.cBitsPerPixel == 24)
    {
        for (srcy = joff * bpp, desty = joff * SVP_SIZEOFRGBA;
             desty < (joff + h * width) * SVP_SIZEOFRGBA;
             srcy += width * bpp, desty += width * SVP_SIZEOFRGBA)
        {
            // RGB to BGR
            for (srcx = srcy, destx = desty;
                 destx < desty + w * SVP_SIZEOFRGBA;
                 srcx += bpp, destx += SVP_SIZEOFRGBA)
            {
                mFrameBuffer[destx]     = mScreenBuffer[srcx + 2];
                mFrameBuffer[destx + 1] = mScreenBuffer[srcx + 1];
                mFrameBuffer[destx + 2] = mScreenBuffer[srcx];
            }
        }
    }
    else if (mFrameInfo.cBitsPerPixel == 16)
    {
        for (srcy = joff * bpp, desty = joff * SVP_SIZEOFRGBA;
             desty < (joff + h * width) * SVP_SIZEOFRGBA;
             srcy += width * bpp, desty += width * SVP_SIZEOFRGBA)
        {
            for (srcx = srcy, destx = desty;
                 destx < desty + w * SVP_SIZEOFRGBA;
                 srcx += bpp, destx += SVP_SIZEOFRGBA)
            {
                convert16To32bpp(mScreenBuffer[srcx],
                                 mScreenBuffer[srcx + 1],
                                 mFrameBuffer[destx],
                                 mFrameBuffer[destx + 1],
                                 mFrameBuffer[destx + 2]);
            }
        }
    }

}

/** The server should start to accept clients connections.
 *
 * @param hServer The server instance handle.
 * @param fEnable Whether to enable or disable client connections.
 *                When is false, all existing clients are disconnected.
 *
 * @return IPRT status code.
 */
DECLCALLBACK(int) SVPServerImpl::VRDEEnableConnections(HVRDESERVER hServer, bool fEnable)
{
    SVPServerImpl *me = (SVPServerImpl *)hServer;

#ifdef LOG_ENABLED
    // enable logging
#endif
    LogFlowFunc(("enter\n"));

    // query server for the framebuffer
    VRDEFRAMEBUFFERINFO info;
    int rc = me->mCallbacks->VRDECallbackFramebufferQuery(me->mCallback, 0, &info);

    // get listen address
    char szAddress[SVP_ADDRESSSIZE + 1] = {0};
    uint32_t cbOut = 0;
    rc = me->mCallbacks->VRDECallbackProperty(me->mCallback,
                                                    VRDE_QP_NETWORK_ADDRESS,
                                                    &szAddress, sizeof(szAddress), &cbOut);
    Assert(cbOut <= sizeof(szAddress));
    if (RT_FAILURE(rc) || !szAddress[0])
        strcpy(szAddress, "0.0.0.0");

    // get listen port
    uint16_t port = 0;
    char portRange[64];
    rc = me->mCallbacks->VRDECallbackProperty(me->mCallback,
                                                    VRDE_QP_NETWORK_PORT_RANGE,
                                                    &portRange, sizeof(portRange), &cbOut);
    port = atoi(portRange);
    if (RT_FAILURE(rc) || port == 0)
        port = 5678;

    rc = svp_get_interface(&me->m_backend, &me->m_frontend);
    if (rc) {
        LogRel(("Error get svp interface"));
        return VERR_GENERAL_FAILURE;
    }
    me->m_frontend.init(&me->m_frontend, szAddress, port);

    // notify about the actually used port
    me->mCallbacks->VRDECallbackProperty(me->mCallback,
                                               VRDE_SP_NETWORK_BIND_PORT,
                                               &port, sizeof(port), NULL);

    return VINF_SUCCESS;
}

/** The server should disconnect the client.
 *
 * @param hServer     The server instance handle.
 * @param u32ClientId The client identifier.
 * @param fReconnect  Whether to send a "REDIRECT to the same server" packet to the
 *                    client before disconnecting.
 *
 * @return IPRT status code.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEDisconnect(HVRDESERVER hServer, uint32_t u32ClientId,
                                                 bool fReconnect)
{
}

/**
 * Inform the server that the display was resized.
 * The server will query information about display
 * from the application via callbacks.
 *
 * @param hServer Handle of VRDE server instance.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEResize(HVRDESERVER hServer)
{
    SVPServerImpl *me = (SVPServerImpl *)hServer;
    VRDEFRAMEBUFFERINFO info;
    int rc = me->mCallbacks->VRDECallbackFramebufferQuery(me->mCallback, 0, &info);
    if (!RT_SUCCESS(rc))
    {
        return;
    }

    LogRel(("SVPServerImpl::VRDEResize to %dx%dx%dbpp\n", info.cWidth, info.cHeight, info.cBitsPerPixel));

    // we always alloc an RGBA buffer
    unsigned char *frameBuffer = (unsigned char *)RTMemAlloc(info.cWidth * info.cHeight * SVP_SIZEOFRGBA); // RGBA
    if (info.cBitsPerPixel == 16)
    {
        uint32_t i, j;
        for (i = 0, j = 0;
             i < info.cWidth * info.cHeight * SVP_SIZEOFRGBA;
             i += SVP_SIZEOFRGBA, j += info.cBitsPerPixel / 8)
        {
            convert16To32bpp(info.pu8Bits[j],
                             info.pu8Bits[j + 1],
                             frameBuffer[i],
                             frameBuffer[i + 1],
                             frameBuffer[i + 2]);
        }
    }

    VRDE_PIXEL_FORMAT fmt;
    if (info.cBitsPerPixel == 32)
        fmt = VRDE_RGB32;
    else if (info.cBitsPerPixel == 24)
        fmt = VRDE_BGR24;
    else if (info.cBitsPerPixel == 16)
        fmt = VRDE_RGB16;
    else if (info.cBitsPerPixel == 8)
        fmt = VRDE_RGB8;
    else {
        AssertMsg(0, ("bad pixel format for bpp %d", info.cBitsPerPixel));
    }
    vrde_set_framebuffer(&me->m_fbInfo, info.cWidth, info.cHeight, fmt, (uint8_t *)info.pu8Bits);

    if (me->m_frontend.screen_resize)
        me->m_frontend.screen_resize(&me->m_frontend, me->m_fbInfo.width, me->m_fbInfo.height);

    void *temp = me->mFrameBuffer;
    me->mFrameBuffer = frameBuffer;
    me->mScreenBuffer = (unsigned char *)info.pu8Bits;
    me->mFrameInfo = info;
    if (temp)
        RTMemFree(temp);
}

/**
 * Send a update.
 *
 * @param hServer   Handle of VRDE server instance.
 * @param uScreenId The screen index.
 * @param pvUpdate  Pointer to VBoxGuest.h::VRDEORDERHDR structure with extra data.
 * @param cbUpdate  Size of the update data.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEUpdate(HVRDESERVER hServer, unsigned uScreenId,
                                             void *pvUpdate,uint32_t cbUpdate)
{
    SVPServerImpl *me = (SVPServerImpl *)hServer;
    struct in_cmd cmd;

    if (pvUpdate == NULL) {
        /* Inform the VRDE server that the current display update sequence is
         * completed. At this moment the framebuffer memory contains a definite
         * image, that is synchronized with the orders already sent to VRDE client.
         * The server can now process redraw requests from clients or initial
         * fullscreen updates for new clients.
         */
        // TODO: handle screen refresh
    } else {
        vrde_process(&me->m_fbInfo, (uint8_t *)pvUpdate, cbUpdate, me->m_inq);
        if (me->m_frontend.graphic_update) {
            while (inq_pop(me->m_inq, &cmd) == 1)
                me->m_frontend.graphic_update(&me->m_frontend, &cmd);
        }
    }
}

/**
 * Set the mouse pointer shape.
 *
 * @param hServer  Handle of VRDE server instance.
 * @param pPointer The pointer shape information.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEColorPointer(HVRDESERVER hServer,
                                                   const VRDECOLORPOINTER *pPointer)
{
    SVPServerImpl *me = (SVPServerImpl *)hServer;
    svp_cursor *cursor;
    int i, j;
    uint8_t *src, *dst;

    cursor = (svp_cursor *)malloc(sizeof(*cursor));
    cursor->x_hot = pPointer->u16HotX;
    cursor->y_hot = pPointer->u16HotY;
    cursor->w = pPointer->u16Width;
    cursor->h = pPointer->u16Height;
    cursor->mask.format = SPF_MONO;
    cursor->mask.x = cursor->mask.y = 0;
    cursor->mask.w = cursor->w;
    cursor->mask.h = cursor->h;
    cursor->mask.size = pPointer->u16MaskLen;
    cursor->mask.bits = (uint8_t *)malloc(pPointer->u16MaskLen);
    src = (uint8_t *)pPointer + sizeof(VRDECOLORPOINTER);
    dst = cursor->mask.bits;
    // top down flip
    for (i = 0; i < cursor->h; i++)
        for (j = 0; j < cursor->w / 8; j++)
            dst[i * cursor->w / 8 + j] = src[(cursor->h - 1 - i) * cursor->w / 8 + j];

    cursor->pixmap.format = SPF_RGB24;
    cursor->pixmap.x = cursor->pixmap.y = 0;
    cursor->pixmap.w = cursor->w;
    cursor->pixmap.h = cursor->h;
    cursor->pixmap.size = pPointer->u16DataLen;
    cursor->pixmap.bits = (uint8_t *)malloc(pPointer->u16DataLen);
    src = (uint8_t *)pPointer + sizeof(VRDECOLORPOINTER) + pPointer->u16MaskLen;
    dst = cursor->pixmap.bits;
    // top down flip
    for (i = 0; i < cursor->h; i++) {
        for (j = 0; j < cursor->w; j++) {
            dst[(i * cursor->w + j) * 3] = src[((cursor->h - 1 - i) * cursor->w + j) * 3];
            dst[(i * cursor->w + j) * 3 + 1] = src[((cursor->h - 1 - i) * cursor->w + j) * 3 + 1];
            dst[(i * cursor->w + j) * 3 + 2] = src[((cursor->h - 1 - i) * cursor->w + j) * 3 + 2];
        }
    }
    if (me->m_frontend.cursor_update)
        me->m_frontend.cursor_update(&me->m_frontend, cursor);
}

/**
 * Hide the mouse pointer.
 *
 * @param hServer Handle of VRDE server instance.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEHidePointer(HVRDESERVER hServer)
{
    SVPServerImpl *instance = (SVPServerImpl *)hServer;

    // TODO: what's behavior for this. hide doesn't seems right
}

/**
 * Queues the samples to be sent to clients.
 *
 * @param hServer    Handle of VRDE server instance.
 * @param pvSamples  Address of samples to be sent.
 * @param cSamples   Number of samples.
 * @param format     Encoded audio format for these samples.
 *
 * @note Initialized to NULL when the application audio callbacks are NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEAudioSamples(HVRDESERVER hServer,
                                                   const void *pvSamples,
                                                   uint32_t cSamples,
                                                   VRDEAUDIOFORMAT format)
{
}

/**
 * Sets the sound volume on clients.
 *
 * @param hServer    Handle of VRDE server instance.
 * @param left       0..0xFFFF volume level for left channel.
 * @param right      0..0xFFFF volume level for right channel.
 *
 * @note Initialized to NULL when the application audio callbacks are NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEAudioVolume(HVRDESERVER hServer,
                                                  uint16_t u16Left,
                                                  uint16_t u16Right)
{
}

/**
 * Sends a USB request.
 *
 * @param hServer      Handle of VRDE server instance.
 * @param u32ClientId  An identifier that allows the server to find the corresponding client.
 *                     The identifier is always passed by the server as a parameter
 *                     of the FNVRDEUSBCALLBACK. Note that the value is the same as
 *                     in the VRDESERVERCALLBACK functions.
 * @param pvParm       Function specific parameters buffer.
 * @param cbParm       Size of the buffer.
 *
 * @note Initialized to NULL when the application USB callbacks are NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEUSBRequest(HVRDESERVER hServer,
                                                 uint32_t u32ClientId,
                                                 void *pvParm,
                                                 uint32_t cbParm)
{
    SVPServerImpl *me = (SVPServerImpl *)hServer;
    struct in_cmd cmd;

    vrde_process_usb(u32ClientId, pvParm, cbParm, me->m_usbq);
    if (me->m_frontend.usb_update) {
        while (inq_pop(me->m_usbq, &cmd) == 1)
            me->m_frontend.usb_update(&me->m_frontend, &cmd);
    }
}

/**
 * Called by the application when (VRDE_CLIPBOARD_FUNCTION_*):
 *   - (0) guest announces available clipboard formats;
 *   - (1) guest requests clipboard data;
 *   - (2) guest responds to the client's request for clipboard data.
 *
 * @param hServer     The VRDE server handle.
 * @param u32Function The cause of the call.
 * @param u32Format   Bitmask of announced formats or the format of data.
 * @param pvData      Points to: (1) buffer to be filled with clients data;
 *                               (2) data from the host.
 * @param cbData      Size of 'pvData' buffer in bytes.
 * @param pcbActualRead Size of the copied data in bytes.
 *
 * @note Initialized to NULL when the application clipboard callbacks are NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEClipboard(HVRDESERVER hServer,
                                                uint32_t u32Function,
                                                uint32_t u32Format,
                                                void *pvData,
                                                uint32_t cbData,
                                                uint32_t *pcbActualRead)
{
}

/**
 * Query various information from the VRDE server.
 *
 * @param hServer   The VRDE server handle.
 * @param index     VRDE_QI_* identifier of information to be returned.
 * @param pvBuffer  Address of memory buffer to which the information must be written.
 * @param cbBuffer  Size of the memory buffer in bytes.
 * @param pcbOut    Size in bytes of returned information value.
 *
 * @remark The caller must check the *pcbOut. 0 there means no information was returned.
 *         A value greater than cbBuffer means that information is too big to fit in the
 *         buffer, in that case no information was placed to the buffer.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEQueryInfo(HVRDESERVER hServer,
                                                uint32_t index,
                                                void *pvBuffer,
                                                uint32_t cbBuffer,
                                                uint32_t *pcbOut)
{
    SVPServerImpl *instance = (SVPServerImpl *)hServer;
    *pcbOut = 0;

    switch (index)
    {
        case VRDE_QI_ACTIVE:    /* # of active clients */
        case VRDE_QI_NUMBER_OF_CLIENTS: /* # of connected clients */
        {
            uint32_t cbOut = sizeof(uint32_t);
            if (cbBuffer >= cbOut)
            {
                *pcbOut = cbOut;
                *(uint32_t *)pvBuffer = instance->mClients;
            }
            break;
        }
        ///@todo lots more queries to implement
        default:
            break;
    }
}


/**
 * The server should redirect the client to the specified server.
 *
 * @param hServer       The server instance handle.
 * @param u32ClientId   The client identifier.
 * @param pszServer     The server to redirect the client to.
 * @param pszUser       The username to use for the redirection.
 *                      Can be NULL.
 * @param pszDomain     The domain. Can be NULL.
 * @param pszPassword   The password. Can be NULL.
 * @param u32SessionId  The ID of the session to redirect to.
 * @param pszCookie     The routing token used by a load balancer to
 *                      route the redirection. Can be NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDERedirect(HVRDESERVER hServer,
                                               uint32_t u32ClientId,
                                               const char *pszServer,
                                               const char *pszUser,
                                               const char *pszDomain,
                                               const char *pszPassword,
                                               uint32_t u32SessionId,
                                               const char *pszCookie)
{
}

/**
 * Audio input open request.
 *
 * @param hServer      Handle of VRDE server instance.
 * @param pvCtx        To be used in VRDECallbackAudioIn.
 * @param u32ClientId  An identifier that allows the server to find the corresponding client.
 * @param audioFormat  Preferred format of audio data.
 * @param u32SamplesPerBlock Preferred number of samples in one block of audio input data.
 *
 * @note Initialized to NULL when the VRDECallbackAudioIn callback is NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEAudioInOpen(HVRDESERVER hServer,
                                                  void *pvCtx,
                                                  uint32_t u32ClientId,
                                                  VRDEAUDIOFORMAT audioFormat,
                                                  uint32_t u32SamplesPerBlock)
{
}

/**
 * Audio input close request.
 *
 * @param hServer      Handle of VRDE server instance.
 * @param u32ClientId  An identifier that allows the server to find the corresponding client.
 *
 * @note Initialized to NULL when the VRDECallbackAudioIn callback is NULL.
 */
DECLCALLBACK(void) SVPServerImpl::VRDEAudioInClose(HVRDESERVER hServer,
                                                   uint32_t u32ClientId)
{
}

/**
 * Generic interface query. An interface is a set of entry points and callbacks.
 * It is not a reference counted interface.
 *
 * @param hServer    Handle of VRDE server instance.
 * @param pszId      String identifier of the interface, like uuid.
 * @param pInterface The interface structure to be initialized by the VRDE server.
 *                   Only VRDEINTERFACEHDR is initialized by the caller.
 * @param pCallbacks Callbacks required by the interface. The server makes a local copy.
 *                   VRDEINTERFACEHDR version must correspond to the requested interface version.
 * @param pvContext  The context to be used in callbacks.
 */
DECLCALLBACK(int) SVPServerImpl::VRDEGetInterface(HVRDESERVER hServer,
                                   const char *pszId,
                                   VRDEINTERFACEHDR *pInterface,
                                   const VRDEINTERFACEHDR *pCallbacks,
                                   void *pvContext)
{
    if (strcmp(pszId, "SVP"))
        return -1;
    return 0;
}

int SVPServerImpl::Init(const VRDEINTERFACEHDR *pCallbacks,
                        void *pvCallback)
{
    if (pCallbacks->u64Version == VRDE_INTERFACE_VERSION_4)
    {
        mCallbacks = (VRDECALLBACKS_4 *)pCallbacks;
        mCallback = pvCallback;
    }
    else if (pCallbacks->u64Version == VRDE_INTERFACE_VERSION_1)
    {
        ///@todo: this is incorrect and it will cause crash if client call unsupport func.
        mCallbacks = (VRDECALLBACKS_4 *)pCallbacks;
        mCallback = pvCallback;


        // since they are same in order, let's just change header
        Entries.header.u64Version = VRDE_INTERFACE_VERSION_1;
        Entries.header.u64Size = sizeof(VRDEENTRYPOINTS_1);
    }
    else
        return VERR_VERSION_MISMATCH;

    return VINF_SUCCESS;
}

void SVPServerImpl::svpKeyboardInput(struct svp_backend_operations *backend, int down, uint32_t scancode)
{
    SVPServerImpl *me = static_cast<SVPServerImpl *>(backend->priv);
    VRDEINPUTSCANCODE point;

    if (scancode > 0xff) {
        point.uScancode = (scancode >> 8) & 0xff;
        me->mCallbacks->VRDECallbackInput(me->mCallback, VRDE_INPUT_SCANCODE, &point, sizeof(point));
    }
    point.uScancode = (scancode & 0xff) | (down ? 0 : 0x80);
    me->mCallbacks->VRDECallbackInput(me->mCallback, VRDE_INPUT_SCANCODE, &point, sizeof(point));
}

void SVPServerImpl::svpMouseInput(struct svp_backend_operations *backend, int x, int y, int buttonFlag)
{
    SVPServerImpl *me = static_cast<SVPServerImpl *>(backend->priv);

    VRDEINPUTPOINT point;
    unsigned button = 0;
    if (buttonFlag & SMB_LEFT) button |= VRDE_INPUT_POINT_BUTTON1;
    if (buttonFlag & SMB_RIGHT) button |= VRDE_INPUT_POINT_BUTTON2;
    if (buttonFlag & SMB_MIDDLE) button |= VRDE_INPUT_POINT_BUTTON3;
    if (buttonFlag & SMB_WHEEL_UP) button |= VRDE_INPUT_POINT_WHEEL_UP;
    if (buttonFlag & SMB_WHEEL_DOWN) button |= VRDE_INPUT_POINT_WHEEL_DOWN;
    point.uButtons = button;
    point.x = x;
    point.y = y;
    me->mCallbacks->VRDECallbackInput(me->mCallback, VRDE_INPUT_POINT, &point, sizeof(point));
}

void SVPServerImpl::svpClientLogon(struct svp_backend_operations *backend, int id)
{
    SVPServerImpl *instance = static_cast<SVPServerImpl *>(backend->priv);
    int rc;

    ///@todo: we need auth user here

    instance->mCallbacks->VRDECallbackClientConnect(instance->mCallback, id);
    rc = instance->mCallbacks->VRDECallbackIntercept(instance->mCallback, id, VRDE_CLIENT_INTERCEPT_USB, &instance->mUSBIntercept);
    Log(("vrde intercept usb rc = %Rrc\n", rc));
    instance->mClients++;
}

void SVPServerImpl::svpClientLogout(struct svp_backend_operations *backend, int id)
{
    SVPServerImpl *instance = static_cast<SVPServerImpl *>(backend->priv);

    instance->mClients--;
    instance->mCallbacks->VRDECallbackClientDisconnect(instance->mCallback, id, 0);
}

void SVPServerImpl::svpVideoModeSet(struct svp_backend_operations *backend, int w, int h, int depth)
{
    SVPServerImpl *instance = static_cast<SVPServerImpl *>(backend->priv);

    instance->mCallbacks->VRDECallbackVideoModeHint(instance->mCallback, w, h, depth, 0);
}

void SVPServerImpl::svpUsbInput(struct svp_backend_operations *backend, int client_id, int req, const void *data, int size)
{
    SVPServerImpl *instance = static_cast<SVPServerImpl *>(backend->priv);
    uint8_t tcode;
    void *tdata;
    uint32_t tsize;
    if (vrde_translate_usb_input(req, data, size, &tcode, &tdata, &tsize) == 0)
        instance->mCallbacks->VRDECallbackUSB(instance->mCallback, instance->mUSBIntercept, client_id, tcode, tdata, tsize);
}

SVPServerImpl *g_SVPServer = 0;
DECLEXPORT(int) VRDECreateServer(const VRDEINTERFACEHDR *pCallbacks,
                                 void *pvCallback,
                                 VRDEINTERFACEHDR **ppEntryPoints,
                                 HVRDESERVER *phServer)
{
    if (!g_SVPServer)
    {
        g_SVPServer = new SVPServerImpl();
    }
    int rc = g_SVPServer->Init(pCallbacks, pvCallback);
    if (RT_SUCCESS(rc))
    {
        *ppEntryPoints = g_SVPServer->GetInterface();
        *phServer = (HVRDESERVER)g_SVPServer;
    }
    return rc;
}

static const char * const supportedProperties[] =
{
    "TCP/Ports",
    "TCP/Address",
    NULL
};

DECLEXPORT(const char * const *) VRDESupportedProperties(void)
{
    LogFlowFunc(("enter\n"));
    return supportedProperties;
}
