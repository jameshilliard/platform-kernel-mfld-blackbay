/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#include <linux/version.h>

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>

#include <drm/drmP.h>

#include <asm/io.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "pvrmodule.h"
#include "pvr_drm.h"
#include "mrstlfb.h"
#include "kerneldisplay.h"
#include "psb_irq.h"

#include "psb_drv.h"

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"

#if !defined(SUPPORT_DRI_DRM)
#error "SUPPORT_DRI_DRM must be set"
#endif

#define	MAKESTRING(x) # x

#if !defined(DISPLAY_CONTROLLER)
#define DISPLAY_CONTROLLER pvrlfb
#endif

//#define MAKENAME_HELPER(x, y) x ## y
//#define	MAKENAME2(x, y) MAKENAME_HELPER(x, y)
//#define	MAKENAME(x) MAKENAME2(DISPLAY_CONTROLLER, x)

#define unref__ __attribute__ ((unused))

void *MRSTLFBAllocKernelMem(unsigned long ulSize)
{
	return kmalloc(ulSize, GFP_KERNEL);
}

void MRSTLFBFreeKernelMem(void *pvMem)
{
	kfree(pvMem);
}


MRST_ERROR MRSTLFBGetLibFuncAddr (char *szFunctionName, PFN_DC_GET_PVRJTABLE *ppfnFuncTable)
{
	if(strcmp("PVRGetDisplayClassJTable", szFunctionName) != 0)
	{
		return (MRST_ERROR_INVALID_PARAMS);
	}


	*ppfnFuncTable = PVRGetDisplayClassJTable;

	return (MRST_OK);
}

static void MRSTLFBVSyncWriteReg(MRSTLFB_DEVINFO *psDevInfo, unsigned long ulOffset, unsigned long ulValue)
{

	void *pvRegAddr = (void *)(psDevInfo->pvRegs + ulOffset);
	mb();
	iowrite32(ulValue, pvRegAddr);
}

unsigned long MRSTLFBVSyncReadReg(MRSTLFB_DEVINFO * psDevinfo, unsigned long ulOffset)
{
	mb();
	return ioread32((char *)psDevinfo->pvRegs + ulOffset);
}

void MRSTLFBEnableVSyncInterrupt(MRSTLFB_DEVINFO * psDevinfo)
{
#if defined(MRST_USING_INTERRUPTS)
    struct drm_psb_private *dev_priv =
	(struct drm_psb_private *) psDevinfo->psDrmDevice->dev_private;
    dev_priv->vblanksEnabledForFlips = true;
    psb_enable_vblank(psDevinfo->psDrmDevice, 0);

#endif
}

void MRSTLFBDisableVSyncInterrupt(MRSTLFB_DEVINFO * psDevinfo)
{
#if defined(MRST_USING_INTERRUPTS)
    struct drm_device * dev = psDevinfo->psDrmDevice;
    struct drm_psb_private *dev_priv =
	(struct drm_psb_private *) psDevinfo->psDrmDevice->dev_private;
    dev_priv->vblanksEnabledForFlips = false;
    //Only turn off if DRM isn't currently using vblanks, otherwise, leave on.
    if (!dev->vblank_enabled[0])
    psb_disable_vblank(psDevinfo->psDrmDevice, 0);
#endif
}

#if defined(MRST_USING_INTERRUPTS)
MRST_ERROR MRSTLFBInstallVSyncISR(MRSTLFB_DEVINFO *psDevInfo, MRSTLFB_VSYNC_ISR_PFN pVsyncHandler)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *) psDevInfo->psDrmDevice->dev_private;
	dev_priv->psb_vsync_handler = pVsyncHandler;
	return (MRST_OK);
}


MRST_ERROR MRSTLFBUninstallVSyncISR(MRSTLFB_DEVINFO	*psDevInfo)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *) psDevInfo->psDrmDevice->dev_private;
	dev_priv->psb_vsync_handler = NULL;
	return (MRST_OK);
}
#endif 


void MRSTLFBFlipToSurface(MRSTLFB_DEVINFO *psDevInfo,  unsigned long uiAddr)
{
	int dspbase = (psDevInfo->ui32MainPipe == 0 ? DSPABASE : DSPBBASE);
	int dspsurf = (psDevInfo->ui32MainPipe == 0 ? DSPASURF : DSPBSURF);
	int panel_type;

	panel_type = is_panel_vid_or_cmd(psDevInfo->psDrmDevice);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
	{
		dspsurf = DSPASURF;
		MRSTLFBVSyncWriteReg(psDevInfo, dspsurf, uiAddr);
#if defined(CONFIG_MDFD_DUAL_MIPI)
		dspsurf = DSPCSURF;
		MRSTLFBVSyncWriteReg(psDevInfo, dspsurf, uiAddr);
#endif

		if (panel_type == MDFLD_DSI_ENCODER_DBI) {
#if defined(CONFIG_MDFLD_DSI_DPU)
			mdfld_dbi_dpu_report_fullscreen_damage(psDevInfo->psDrmDevice);
#elif defined(CONFIG_MDFLD_DSI_DSR)
			/*if in DSR mode, exit it!*/
			mdfld_dsi_dbi_exit_dsr (psDevInfo->psDrmDevice, MDFLD_DSR_2D_3D);
#endif
		}

		dspsurf = DSPBSURF;
		MRSTLFBVSyncWriteReg(psDevInfo, dspsurf, uiAddr);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}
}


int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init)(struct drm_device unref__ *dev)
{
	if(MRSTLFBInit(dev) != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": MRSTLFB_Init: MRSTLFBInit failed\n");
		return -ENODEV;
	}

	return 0;
}

void PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup)(struct drm_device unref__ *dev)
{    
	if(MRSTLFBDeinit() != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX "%s: can't deinit device\n", __FUNCTION__);
	}
}

int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Suspend)(struct drm_device unref__ *dev)
{
	MRSTLFBSuspend();

	return 0;
}

int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Resume)(struct drm_device unref__ *dev)
{
	MRSTLFBResume();

	return 0;
}
