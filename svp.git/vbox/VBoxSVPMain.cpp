/* $Id: VBoxSVPMain.cpp $ */
/** @file
 * SVP main module.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/ExtPack/ExtPack.h>
#define LOG_GROUP LOG_GROUP_MISC
#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/log.h>
#include <VBox/vmm/vm.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include <iprt/path.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the extension pack helpers. */
static PCVBOXEXTPACKHLP g_pHlp;


// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnInstalled}
//  */
// static DECLCALLBACK(void) vboxSVPExtPack_Installed(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnUninstall}
//  */
// static DECLCALLBACK(int)  vboxSVPExtPack_Uninstall(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVirtualBoxReady}
//  */
// static DECLCALLBACK(void)  vboxSVPExtPack_VirtualBoxReady(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnUnload}
//  */
// static DECLCALLBACK(void) vboxSVPExtPack_Unload(PCVBOXEXTPACKREG pThis);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMCreated}
//  */
// static DECLCALLBACK(int)  vboxSVPExtPack_VMCreated(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox, IMachine *pMachine);
//
/**
 * @interface_method_impl{VBOXEXTPACKREG,pfnVMConfigureVMM}
 */
static DECLCALLBACK(int) vboxSVPExtPack_VMConfigureVMM(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMPowerOn}
//  */
// static DECLCALLBACK(int)  vboxSVPExtPack_VMPowerOn(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMPowerOff}
//  */
// static DECLCALLBACK(void) vboxSVPExtPack_VMPowerOff(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMPowerOff}
//  */
// static DECLCALLBACK(void) vboxSVPExtPack_QueryObject(PCVBOXEXTPACKREG pThis, PCRTUUID pObjectId);


static const VBOXEXTPACKREG g_vboxSVPExtPackReg =
{
    VBOXEXTPACKREG_VERSION,
    /* .pfnInstalled =      */  NULL,
    /* .pfnUninstall =      */  NULL,
    /* .pfnVirtualBoxReady =*/  NULL,
    /* .pfnConsoleReady =   */  NULL,
    /* .pfnUnload =         */  NULL,
    /* .pfnVMCreated =      */  NULL,
    /* .pfnVMConfigureVMM = */  vboxSVPExtPack_VMConfigureVMM,
    /* .pfnVMPowerOn =      */  NULL,
    /* .pfnVMPowerOff =     */  NULL,
    /* .pfnQueryObject =    */  NULL,
    VBOXEXTPACKREG_VERSION
};


/** @callback_method_impl{FNVBOXEXTPACKREGISTER}  */
extern "C" DECLEXPORT(int) VBoxExtPackRegister(PCVBOXEXTPACKHLP pHlp, PCVBOXEXTPACKREG *ppReg, PRTERRINFO pErrInfo)
{
    /*
     * Check the VirtualBox version.
     */
    if (!VBOXEXTPACK_IS_VER_COMPAT(pHlp->u32Version, VBOXEXTPACKHLP_VERSION))
        return RTErrInfoSetF(pErrInfo, VERR_VERSION_MISMATCH,
                             "Helper version mismatch - expected %#x got %#x",
                             VBOXEXTPACKHLP_VERSION, pHlp->u32Version);
    if (   VBOX_FULL_VERSION_GET_MAJOR(pHlp->uVBoxFullVersion) != VBOX_VERSION_MAJOR
        || VBOX_FULL_VERSION_GET_MINOR(pHlp->uVBoxFullVersion) != VBOX_VERSION_MINOR)
        return RTErrInfoSetF(pErrInfo, VERR_VERSION_MISMATCH,
                             "VirtualBox version mismatch - expected %u.%u got %u.%u",
                             VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR,
                             VBOX_FULL_VERSION_GET_MAJOR(pHlp->uVBoxFullVersion),
                             VBOX_FULL_VERSION_GET_MINOR(pHlp->uVBoxFullVersion));

    /*
     * We're good, save input and return the registration structure.
     */
    g_pHlp = pHlp;
    *ppReg = &g_vboxSVPExtPackReg;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vboxSVPExtPack_VMConfigureVMM(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM)
{
    Log(("SVP: configure VMM\n"));
    /*
     * Find the bus mouse module and tell PDM to load it.
     * ASSUME /PDM/Devices exists.
     */
    char szPath[RTPATH_MAX];
    int rc = g_pHlp->pfnFindModule(g_pHlp, "CvmEhci", NULL, VBOXEXTPACKMODKIND_R3, szPath, sizeof(szPath), NULL);
    if (RT_FAILURE(rc))
        return rc;

    PCFGMNODE pCfgDevices = CFGMR3GetChild(CFGMR3GetRoot(pVM), "PDM/Devices");
    AssertReturn(pCfgDevices, VERR_INTERNAL_ERROR_3);

    PCFGMNODE pCfgMine;
    rc = CFGMR3InsertNode(pCfgDevices, "CvmEhci", &pCfgMine);
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertString(pCfgMine, "Path", szPath);
    AssertRCReturn(rc, rc);

    PCFGMNODE pDevices = CFGMR3GetChild(CFGMR3GetRoot(pVM), "Devices");
    PCFGMNODE pDev = 0;
    PCFGMNODE pInst = 0;
    PCFGMNODE pCfg = 0;

    rc = CFGMR3InsertNode(pDevices, "cvm-usb-ehci", &pDev);
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertNode(pDev, "0", &pInst);
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertNode(pInst, "Config", &pCfg);
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertInteger(pInst, "Trusted", 1); /* boolean */
    AssertRCReturn(rc, rc);

    /*
    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertString(pLunL0, "Driver", "VUSBRootHub");
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);
    AssertRCReturn(rc, rc);
    */

    return VINF_SUCCESS;
}
