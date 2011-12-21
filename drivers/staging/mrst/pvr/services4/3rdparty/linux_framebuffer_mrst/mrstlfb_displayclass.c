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
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <asm/intel_scu_ipc.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "mrstlfb.h"

#include "psb_fb.h"
#include "psb_drv.h"
#include "psb_powermgmt.h"

#if !defined(SUPPORT_DRI_DRM)
#error "SUPPORT_DRI_DRM must be set"
#endif

IMG_UINT32 gui32MRSTDisplayDeviceID;

PVRSRV_ERROR MRSTLFBPrePowerState(IMG_HANDLE 		 hDevHandle,
				  PVRSRV_DEV_POWER_STATE eNewPowerState,
				  PVRSRV_DEV_POWER_STATE eCurrentPowerState);

PVRSRV_ERROR MRSTLFBPostPowerState(IMG_HANDLE 		  hDevHandle,
				   PVRSRV_DEV_POWER_STATE eNewPowerState,
				   PVRSRV_DEV_POWER_STATE eCurrentPowerState);

struct psbfb_par {
  struct drm_device *dev;
  void *psbfb;

  int dpms_state;

  int crtc_count;

  uint32_t crtc_ids[2];
};

static void *gpvAnchor;


#define MRSTLFB_COMMAND_COUNT		1

static PFN_DC_GET_PVRJTABLE pfnGetPVRJTable = 0;

static MRSTLFB_DEVINFO * GetAnchorPtr(void)
{
	return (MRSTLFB_DEVINFO *)gpvAnchor;
}

static void SetAnchorPtr(MRSTLFB_DEVINFO *psDevInfo)
{
	gpvAnchor = (void*)psDevInfo;
}

static void MRSTLFBFlip(MRSTLFB_DEVINFO *psDevInfo, MRSTLFB_BUFFER *psBuffer)
{
	unsigned long ulAddr = (unsigned long)psBuffer->sDevVAddr.uiAddr;

	if (!psDevInfo->bSuspended && !psDevInfo->bLeaveVT)
	{
		MRSTLFBFlipToSurface(psDevInfo, ulAddr);
	}

	psDevInfo->ulLastFlipAddr = ulAddr;
	psDevInfo->bLastFlipAddrValid = MRST_TRUE;
}

static void MRSTLFBRestoreLastFlip(MRSTLFB_DEVINFO *psDevInfo)
{
	if (!psDevInfo->bSuspended && !psDevInfo->bLeaveVT)
	{
		if (psDevInfo->bLastFlipAddrValid)
		{
			MRSTLFBFlipToSurface(psDevInfo, psDevInfo->ulLastFlipAddr);
		}
	}
}

static void MRSTLFBClearSavedFlip(MRSTLFB_DEVINFO *psDevInfo)
{
	psDevInfo->bLastFlipAddrValid = MRST_FALSE;
}

static void FlushInternalVSyncQueue(MRSTLFB_SWAPCHAIN *psSwapChain, MRST_BOOL bFlip)
{
	MRSTLFB_VSYNC_FLIP_ITEM *psFlipItem;
	unsigned long            ulMaxIndex;
	unsigned long            i;

	
	psFlipItem = &psSwapChain->psVSyncFlips[psSwapChain->ulRemoveIndex];
	ulMaxIndex = psSwapChain->ulSwapChainLength - 1;

	for(i = 0; i < psSwapChain->ulSwapChainLength; i++)
	{
		if (psFlipItem->bValid == MRST_FALSE)
		{
			continue;
		}

		DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX ": FlushInternalVSyncQueue: Flushing swap buffer (index %lu)\n", psSwapChain->ulRemoveIndex));

		if(psFlipItem->bFlipped == MRST_FALSE)
		{
			if (bFlip)
			{
				
				MRSTLFBFlip(psSwapChain->psDevInfo, psFlipItem->psBuffer);
			}
		}
		
		if(psFlipItem->bCmdCompleted == MRST_FALSE)
		{
			DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX ": FlushInternalVSyncQueue: Calling command complete for swap buffer (index %lu)\n", psSwapChain->ulRemoveIndex));

			psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete((IMG_HANDLE)psFlipItem->hCmdComplete, MRST_TRUE);
		}

		
		psSwapChain->ulRemoveIndex++;
		
		if(psSwapChain->ulRemoveIndex > ulMaxIndex)
		{
			psSwapChain->ulRemoveIndex = 0;
		}

		
		psFlipItem->bFlipped = MRST_FALSE;
		psFlipItem->bCmdCompleted = MRST_FALSE;
		psFlipItem->bValid = MRST_FALSE;
		
		
		psFlipItem = &psSwapChain->psVSyncFlips[psSwapChain->ulRemoveIndex];
	}

	psSwapChain->ulInsertIndex = 0;
	psSwapChain->ulRemoveIndex = 0;
}

static void DRMLFBFlipBuffer(MRSTLFB_DEVINFO *psDevInfo, MRSTLFB_SWAPCHAIN *psSwapChain, MRSTLFB_BUFFER *psBuffer) 
{
	if(psSwapChain != NULL) 
	{
		if(psDevInfo->psCurrentSwapChain != NULL)
		{
			
			if(psDevInfo->psCurrentSwapChain != psSwapChain) 
				FlushInternalVSyncQueue(psDevInfo->psCurrentSwapChain, MRST_FALSE);
		}
		psDevInfo->psCurrentSwapChain = psSwapChain;
	}

	
	MRSTLFBFlip(psDevInfo, psBuffer);
}

static void SetFlushStateNoLock(MRSTLFB_DEVINFO* psDevInfo,
                                        MRST_BOOL bFlushState)
{
	if (bFlushState)
	{
		if (psDevInfo->ulSetFlushStateRefCount == 0)
		{
			psDevInfo->bFlushCommands = MRST_TRUE;
			if (psDevInfo->psCurrentSwapChain != NULL)
			{
				FlushInternalVSyncQueue(psDevInfo->psCurrentSwapChain, MRST_TRUE);
			}
		}
		psDevInfo->ulSetFlushStateRefCount++;
	}
	else
	{
		if (psDevInfo->ulSetFlushStateRefCount != 0)
		{
			psDevInfo->ulSetFlushStateRefCount--;
			if (psDevInfo->ulSetFlushStateRefCount == 0)
			{
				psDevInfo->bFlushCommands = MRST_FALSE;
			}
		}
	}
}

static IMG_VOID SetFlushState(MRSTLFB_DEVINFO* psDevInfo,
                                      MRST_BOOL bFlushState)
{
	unsigned long ulLockFlags;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	SetFlushStateNoLock(psDevInfo, bFlushState);

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);
}

static IMG_VOID SetDCState(IMG_HANDLE hDevice, IMG_UINT32 ui32State)
{
	MRSTLFB_DEVINFO *psDevInfo = (MRSTLFB_DEVINFO *)hDevice;

	switch (ui32State)
	{
		case DC_STATE_FLUSH_COMMANDS:
			SetFlushState(psDevInfo, MRST_TRUE);
			break;
		case DC_STATE_NO_FLUSH_COMMANDS:
			SetFlushState(psDevInfo, MRST_FALSE);
			break;
		default:
			break;
	}

	return;
}

static int FrameBufferEvents(struct notifier_block *psNotif,
                             unsigned long event, void *data)
{
	MRSTLFB_DEVINFO *psDevInfo;
	struct fb_event *psFBEvent = (struct fb_event *)data;
	MRST_BOOL bBlanked;

	
	if (event != FB_EVENT_BLANK)
	{
		return 0;
	}

	psDevInfo = GetAnchorPtr();

	bBlanked = (*(IMG_INT *)psFBEvent->data != 0) ? MRST_TRUE: MRST_FALSE;

	if (bBlanked != psDevInfo->bBlanked)
	{
		psDevInfo->bBlanked = bBlanked;

		SetFlushState(psDevInfo, bBlanked);
	}

	return 0;
}


static MRST_ERROR UnblankDisplay(MRSTLFB_DEVINFO *psDevInfo)
{
	int res;

	console_lock();
	res = fb_blank(psDevInfo->psLINFBInfo, 0);
	console_unlock();
	if (res != 0)
	{
		printk(KERN_WARNING DRIVER_PREFIX
			": fb_blank failed (%d)", res);
		return (MRST_ERROR_GENERIC);
	}

	return (MRST_OK);
}

static MRST_ERROR EnableLFBEventNotification(MRSTLFB_DEVINFO *psDevInfo)
{
	int                res;
	MRST_ERROR         eError;

	
	memset(&psDevInfo->sLINNotifBlock, 0, sizeof(psDevInfo->sLINNotifBlock));

	psDevInfo->sLINNotifBlock.notifier_call = FrameBufferEvents;
	psDevInfo->bBlanked = MRST_FALSE;

	res = fb_register_client(&psDevInfo->sLINNotifBlock);
	if (res != 0)
	{
		printk(KERN_WARNING DRIVER_PREFIX
			": fb_register_client failed (%d)", res);

		return (MRST_ERROR_GENERIC);
	}

	eError = UnblankDisplay(psDevInfo);
	if (eError != MRST_OK)
	{
		DEBUG_PRINTK((KERN_WARNING DRIVER_PREFIX
			": UnblankDisplay failed (%d)", eError));
		return eError;
	}

	return (MRST_OK);
}

static MRST_ERROR DisableLFBEventNotification(MRSTLFB_DEVINFO *psDevInfo)
{
	int res;


	res = fb_unregister_client(&psDevInfo->sLINNotifBlock);
	if (res != 0)
	{
		printk(KERN_WARNING DRIVER_PREFIX
			": fb_unregister_client failed (%d)", res);
		return (MRST_ERROR_GENERIC);
	}

	return (MRST_OK);
}

static PVRSRV_ERROR OpenDCDevice(IMG_UINT32 ui32DeviceID,
                                 IMG_HANDLE *phDevice,
                                 PVRSRV_SYNC_DATA* psSystemBufferSyncData)
{
	MRSTLFB_DEVINFO *psDevInfo;
	MRST_ERROR eError;

	UNREFERENCED_PARAMETER(ui32DeviceID);

	psDevInfo = GetAnchorPtr();

	
	psDevInfo->sSystemBuffer.psSyncData = psSystemBufferSyncData;

	psDevInfo->ulSetFlushStateRefCount = 0;
	psDevInfo->bFlushCommands = MRST_FALSE;

	eError = EnableLFBEventNotification(psDevInfo);
	if (eError != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": Couldn't enable framebuffer event notification\n");
		return PVRSRV_ERROR_UNABLE_TO_OPEN_DC_DEVICE;
	}

	
	*phDevice = (IMG_HANDLE)psDevInfo;
	
	return (PVRSRV_OK);
}

static PVRSRV_ERROR CloseDCDevice(IMG_HANDLE hDevice)
{
	MRSTLFB_DEVINFO *psDevInfo = (MRSTLFB_DEVINFO *)hDevice;
	MRST_ERROR eError;

	eError = DisableLFBEventNotification(psDevInfo);
	if (eError != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": Couldn't disable framebuffer event notification\n");
		return PVRSRV_ERROR_UNABLE_TO_REMOVE_DEVICE;
	}

	return (PVRSRV_OK);
}

static PVRSRV_ERROR EnumDCFormats(IMG_HANDLE hDevice,
                                  IMG_UINT32 *pui32NumFormats,
                                  DISPLAY_FORMAT *psFormat)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !pui32NumFormats)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*pui32NumFormats = 1;

	if(psFormat)
	{
		psFormat[0] = psDevInfo->sDisplayFormat;
	}

	return (PVRSRV_OK);
}

static PVRSRV_ERROR EnumDCDims(IMG_HANDLE hDevice,
                               DISPLAY_FORMAT *psFormat,
                               IMG_UINT32 *pui32NumDims,
                               DISPLAY_DIMS *psDim)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !psFormat || !pui32NumDims)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*pui32NumDims = 1;


	if(psDim)
	{
		psDim[0] = psDevInfo->sDisplayDim;
	}

	return (PVRSRV_OK);
}


static PVRSRV_ERROR GetDCSystemBuffer(IMG_HANDLE hDevice, IMG_HANDLE *phBuffer)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !phBuffer)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	
	
	*phBuffer = (IMG_HANDLE)&psDevInfo->sSystemBuffer;

	return (PVRSRV_OK);
}


static PVRSRV_ERROR GetDCInfo(IMG_HANDLE hDevice, DISPLAY_INFO *psDCInfo)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !psDCInfo)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*psDCInfo = psDevInfo->sDisplayInfo;

	return (PVRSRV_OK);
}

static PVRSRV_ERROR GetDCBufferAddr(IMG_HANDLE        hDevice,
                                    IMG_HANDLE        hBuffer, 
                                    IMG_SYS_PHYADDR   **ppsSysAddr,
                                    IMG_SIZE_T        *pui32ByteSize,
                                    IMG_VOID          **ppvCpuVAddr,
                                    IMG_HANDLE        *phOSMapInfo,
                                    IMG_BOOL          *pbIsContiguous,
	                            IMG_UINT32	      *pui32TilingStride)
{
	MRSTLFB_DEVINFO	*psDevInfo;
	MRSTLFB_BUFFER *psSystemBuffer;

	UNREFERENCED_PARAMETER(pui32TilingStride);

	if(!hDevice)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	if(!hBuffer)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	psSystemBuffer = (MRSTLFB_BUFFER *)hBuffer;

	if (!ppsSysAddr)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	if( psSystemBuffer->bIsContiguous ) 
		*ppsSysAddr = &psSystemBuffer->uSysAddr.sCont;
	else
		*ppsSysAddr = psSystemBuffer->uSysAddr.psNonCont;

	if (!pui32ByteSize)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	*pui32ByteSize = psSystemBuffer->ui32BufferSize;

	if (ppvCpuVAddr)
	{
		*ppvCpuVAddr = psSystemBuffer->sCPUVAddr;
	}

	if (phOSMapInfo)
	{
		*phOSMapInfo = (IMG_HANDLE)0;
	}

	if (pbIsContiguous)
	{
		*pbIsContiguous = psSystemBuffer->bIsContiguous;
	}

	return (PVRSRV_OK);
}


#if 0
static MRST_ERROR MRSTLFBEnableSwapChains(MRSTLFB_DEVINFO *psDevInfo) 
{
	unsigned long ulLockFlags;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	if(!psDevInfo->bFlushCommands)
		MRSTLFBEnableVSyncInterrupt(psDevInfo);

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

	if (EnableLFBEventNotification(psDevInfo)!= MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": Couldn't enable framebuffer event notification\n");
	}

	return MRST_OK;
}

static MRST_ERROR MRSTLFBDisableSwapChains(MRSTLFB_DEVINFO *psDevInfo) 
{
	MRST_ERROR eError;
	unsigned long ulLockFlags;

	eError = DisableLFBEventNotification(psDevInfo);
	if (eError != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": Couldn't disable framebuffer event notification\n");
	}

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	MRSTLFBDisableVSyncInterrupt(psDevInfo);

	
	MRSTLFBFlip(psDevInfo, &psDevInfo->sSystemBuffer);

	psDevInfo->psCurrentSwapChain = NULL;

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);
	return MRST_OK;
}
#endif

static PVRSRV_ERROR CreateDCSwapChain(IMG_HANDLE hDevice,
                                      IMG_UINT32 ui32Flags,
                                      DISPLAY_SURF_ATTRIBUTES *psDstSurfAttrib,
                                      DISPLAY_SURF_ATTRIBUTES *psSrcSurfAttrib,
                                      IMG_UINT32 ui32BufferCount,
                                      PVRSRV_SYNC_DATA **ppsSyncData,
                                      IMG_UINT32 ui32OEMFlags,
                                      IMG_HANDLE *phSwapChain,
                                      IMG_UINT32 *pui32SwapChainID)
{
	MRSTLFB_DEVINFO	*psDevInfo;
	MRSTLFB_SWAPCHAIN *psSwapChain;
	MRSTLFB_BUFFER **ppsBuffer;
	MRSTLFB_VSYNC_FLIP_ITEM *psVSyncFlips;
	IMG_UINT32 i;
	IMG_UINT32 iSCId = MAX_SWAPCHAINS;
	PVRSRV_ERROR eError = PVRSRV_ERROR_NOT_SUPPORTED;
	unsigned long ulLockFlags;
	struct drm_device* psDrmDev;
	unsigned long ulSwapChainLength;

	UNREFERENCED_PARAMETER(ui32OEMFlags);
	
	
	if(!hDevice
	|| !psDstSurfAttrib
	|| !psSrcSurfAttrib
	|| !ppsSyncData
	|| !phSwapChain)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;
		
	
	if(ui32BufferCount > psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers)
	{
		return (PVRSRV_ERROR_TOOMANYBUFFERS);
	}
	
	
	ulSwapChainLength = ui32BufferCount + 1;
	

	
	if(psDstSurfAttrib->pixelformat != psDevInfo->sDisplayFormat.pixelformat
	|| psDstSurfAttrib->sDims.ui32ByteStride != psDevInfo->sDisplayDim.ui32ByteStride
	|| psDstSurfAttrib->sDims.ui32Width != psDevInfo->sDisplayDim.ui32Width
	|| psDstSurfAttrib->sDims.ui32Height != psDevInfo->sDisplayDim.ui32Height)
	{

		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	if(psDstSurfAttrib->pixelformat != psSrcSurfAttrib->pixelformat
	|| psDstSurfAttrib->sDims.ui32ByteStride != psSrcSurfAttrib->sDims.ui32ByteStride
	|| psDstSurfAttrib->sDims.ui32Width != psSrcSurfAttrib->sDims.ui32Width
	|| psDstSurfAttrib->sDims.ui32Height != psSrcSurfAttrib->sDims.ui32Height)
	{

		return (PVRSRV_ERROR_INVALID_PARAMS);
	}


	UNREFERENCED_PARAMETER(ui32Flags);


	psSwapChain = (MRSTLFB_SWAPCHAIN*)MRSTLFBAllocKernelMem(sizeof(MRSTLFB_SWAPCHAIN));
	if(!psSwapChain)
	{
		return (PVRSRV_ERROR_OUT_OF_MEMORY);
	}

	for(iSCId = 0;iSCId < MAX_SWAPCHAINS;++iSCId) 
	{
		if( psDevInfo->apsSwapChains[iSCId] == NULL )
		{
			psDevInfo->apsSwapChains[iSCId] = psSwapChain;
			break;
		}
	}

	if(iSCId == MAX_SWAPCHAINS) 
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreeSwapChain;
	}

	ppsBuffer = (MRSTLFB_BUFFER**)MRSTLFBAllocKernelMem(sizeof(MRSTLFB_BUFFER*) * ui32BufferCount);
	if(!ppsBuffer)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreeSwapChain;
	}

	psVSyncFlips = (MRSTLFB_VSYNC_FLIP_ITEM *)MRSTLFBAllocKernelMem(sizeof(MRSTLFB_VSYNC_FLIP_ITEM) * ulSwapChainLength);
	if (!psVSyncFlips)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreeBuffers;
	}

	psSwapChain->ulSwapChainLength = ulSwapChainLength;
	psSwapChain->ulBufferCount = (unsigned long)ui32BufferCount;
	psSwapChain->ppsBuffer = ppsBuffer;
	psSwapChain->psVSyncFlips = psVSyncFlips;
	psSwapChain->ulInsertIndex = 0;
	psSwapChain->ulRemoveIndex = 0;
	psSwapChain->psPVRJTable = &psDevInfo->sPVRJTable;

	
	
	for (i = 0; i < ui32BufferCount; i++)
	{
		unsigned long bufSize = psDevInfo->sDisplayDim.ui32ByteStride * psDevInfo->sDisplayDim.ui32Height;
		MRSTLFBAllocBuffer(psDevInfo, bufSize, &ppsBuffer[i] );
		ppsBuffer[i]->psSyncData = ppsSyncData[i];		
	}

	
	for (i = 0; i < ulSwapChainLength; i++)
	{
		psVSyncFlips[i].bValid = MRST_FALSE;
		psVSyncFlips[i].bFlipped = MRST_FALSE;
		psVSyncFlips[i].bCmdCompleted = MRST_FALSE;
	}


	psDrmDev = psDevInfo->psDrmDevice;

	psSwapChain->psDevInfo = psDevInfo;
	psSwapChain->psDrmDev = psDrmDev;
	psSwapChain->psDrmDriver = psDrmDev->driver;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);
   
	psSwapChain->ui32SwapChainID = *pui32SwapChainID = iSCId+1;

	if(psDevInfo->psCurrentSwapChain == NULL)
		psDevInfo->psCurrentSwapChain = psSwapChain;

	psDevInfo->ui32SwapChainNum++;
	if(psDevInfo->ui32SwapChainNum == 1)
	{
		MRSTLFBEnableVSyncInterrupt(psDevInfo);
	}

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

	
	*phSwapChain = (IMG_HANDLE)psSwapChain;

	return (PVRSRV_OK);

	MRSTLFBFreeKernelMem(psVSyncFlips);
ErrorFreeBuffers:
	MRSTLFBFreeKernelMem(ppsBuffer);
ErrorFreeSwapChain:
	if(iSCId != MAX_SWAPCHAINS && psDevInfo->apsSwapChains[iSCId] == psSwapChain ) 
		psDevInfo->apsSwapChains[iSCId] = NULL;	
	MRSTLFBFreeKernelMem(psSwapChain);

	return eError;
}

static PVRSRV_ERROR DestroyDCSwapChain(IMG_HANDLE hDevice,
	IMG_HANDLE hSwapChain)
{
	MRSTLFB_DEVINFO	*psDevInfo;
	MRSTLFB_SWAPCHAIN *psSwapChain;
	unsigned long ulLockFlags;
	int i;

	
	if(!hDevice || !hSwapChain)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	
	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;
	psSwapChain = (MRSTLFB_SWAPCHAIN*)hSwapChain;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	psDevInfo->ui32SwapChainNum--;

	if(psDevInfo->ui32SwapChainNum == 0) 
	{
		MRSTLFBDisableVSyncInterrupt(psDevInfo);
		psDevInfo->psCurrentSwapChain = NULL;
	}

	psDevInfo->apsSwapChains[ psSwapChain->ui32SwapChainID -1] = NULL;

	
	FlushInternalVSyncQueue(psSwapChain, psDevInfo->ui32SwapChainNum == 0);

	if (psDevInfo->ui32SwapChainNum == 0)
	{
		
		DRMLFBFlipBuffer(psDevInfo, NULL, &psDevInfo->sSystemBuffer);
		MRSTLFBClearSavedFlip(psDevInfo);
	}

	if(psDevInfo->psCurrentSwapChain == psSwapChain)
		psDevInfo->psCurrentSwapChain = NULL;

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

	
	for (i = 0; i < psSwapChain->ulBufferCount; i++)
	{
		MRSTLFBFreeBuffer(psDevInfo, &psSwapChain->ppsBuffer[i] );
	}
	MRSTLFBFreeKernelMem(psSwapChain->psVSyncFlips);
	MRSTLFBFreeKernelMem(psSwapChain->ppsBuffer);
	MRSTLFBFreeKernelMem(psSwapChain);

	return (PVRSRV_OK);
}

static PVRSRV_ERROR SetDCDstRect(IMG_HANDLE hDevice,
	IMG_HANDLE hSwapChain,
	IMG_RECT *psRect)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(psRect);



	return (PVRSRV_ERROR_NOT_SUPPORTED);
}

static PVRSRV_ERROR SetDCSrcRect(IMG_HANDLE hDevice,
                                 IMG_HANDLE hSwapChain,
                                 IMG_RECT *psRect)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(psRect);



	return (PVRSRV_ERROR_NOT_SUPPORTED);
}

static PVRSRV_ERROR SetDCDstColourKey(IMG_HANDLE hDevice,
                                      IMG_HANDLE hSwapChain,
                                      IMG_UINT32 ui32CKColour)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(ui32CKColour);



	return (PVRSRV_ERROR_NOT_SUPPORTED);
}

static PVRSRV_ERROR SetDCSrcColourKey(IMG_HANDLE hDevice,
                                      IMG_HANDLE hSwapChain,
                                      IMG_UINT32 ui32CKColour)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(ui32CKColour);



	return (PVRSRV_ERROR_NOT_SUPPORTED);
}

static PVRSRV_ERROR GetDCBuffers(IMG_HANDLE hDevice,
                                 IMG_HANDLE hSwapChain,
                                 IMG_UINT32 *pui32BufferCount,
                                 IMG_HANDLE *phBuffer)
{
	MRSTLFB_DEVINFO   *psDevInfo;
	MRSTLFB_SWAPCHAIN *psSwapChain;
	unsigned long      i;


	if(!hDevice
	|| !hSwapChain
	|| !pui32BufferCount
	|| !phBuffer)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;
	psSwapChain = (MRSTLFB_SWAPCHAIN*)hSwapChain;


	*pui32BufferCount = (IMG_UINT32)psSwapChain->ulBufferCount;


	for(i=0; i<psSwapChain->ulBufferCount; i++)
	{
		phBuffer[i] = (IMG_HANDLE)psSwapChain->ppsBuffer[i];
	}
	
	return (PVRSRV_OK);
}

static PVRSRV_ERROR SwapToDCBuffer(IMG_HANDLE hDevice,
                                   IMG_HANDLE hBuffer,
                                   IMG_UINT32 ui32SwapInterval,
                                   IMG_HANDLE hPrivateTag,
                                   IMG_UINT32 ui32ClipRectCount,
                                   IMG_RECT *psClipRect)
{
	MRSTLFB_DEVINFO *psDevInfo;

	UNREFERENCED_PARAMETER(ui32SwapInterval);
	UNREFERENCED_PARAMETER(hPrivateTag);
	UNREFERENCED_PARAMETER(psClipRect);

	if(!hDevice
	|| !hBuffer
	|| (ui32ClipRectCount != 0))
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;


	return (PVRSRV_OK);
}

static PVRSRV_ERROR SwapToDCSystem(IMG_HANDLE hDevice,
                                   IMG_HANDLE hSwapChain)
{
	if(!hDevice || !hSwapChain)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	
	return (PVRSRV_OK);
}

static MRST_BOOL MRSTLFBVSyncIHandler(MRSTLFB_DEVINFO *psDevInfo)
{
	MRST_BOOL bStatus = MRST_TRUE;
	MRSTLFB_VSYNC_FLIP_ITEM *psFlipItem;
	unsigned long ulMaxIndex;
	unsigned long ulLockFlags;
	MRSTLFB_SWAPCHAIN *psSwapChain;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	
	psSwapChain = psDevInfo->psCurrentSwapChain;
	if (psSwapChain == NULL)
	{
		goto ExitUnlock;
	}

	
	if (psDevInfo->bFlushCommands || psDevInfo->bSuspended || psDevInfo->bLeaveVT)
	{
		goto ExitUnlock;
	}

	psFlipItem = &psSwapChain->psVSyncFlips[psSwapChain->ulRemoveIndex];
	ulMaxIndex = psSwapChain->ulSwapChainLength - 1;

	while(psFlipItem->bValid)
	{	
		
		if(psFlipItem->bFlipped)
		{
			
			if(!psFlipItem->bCmdCompleted)
			{
				
				MRST_BOOL bScheduleMISR;
				
				bScheduleMISR = MRST_TRUE;

				
				psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete((IMG_HANDLE)psFlipItem->hCmdComplete, bScheduleMISR);

				
				psFlipItem->bCmdCompleted = MRST_TRUE;
			}

			
			/* DGA: fix wrap bug with unsigned int */
			if (psFlipItem->ulSwapInterval-- <= 1)
			{	
				
				psSwapChain->ulRemoveIndex++;
				
				if(psSwapChain->ulRemoveIndex > ulMaxIndex)
				{
					psSwapChain->ulRemoveIndex = 0;
				}
				
				
				psFlipItem->bCmdCompleted = MRST_FALSE;
				psFlipItem->bFlipped = MRST_FALSE;
	
				
				psFlipItem->bValid = MRST_FALSE;
			}
			else
			{
				
				break;
			}
		}
		else
		{
			
			DRMLFBFlipBuffer(psDevInfo, psSwapChain, psFlipItem->psBuffer);
			
			
			psFlipItem->bFlipped = MRST_TRUE;
			
			
			break;
		}
		
		
		psFlipItem = &psSwapChain->psVSyncFlips[psSwapChain->ulRemoveIndex];
	}
		
ExitUnlock:
	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

	return bStatus;
}

#if defined(MRST_USING_INTERRUPTS)
static int
MRSTLFBVSyncISR(struct drm_device *psDrmDevice, int iPipe)
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();	
	struct drm_psb_private *dev_priv =
			(struct drm_psb_private *)psDrmDevice->dev_private;

	/* DGA: lets update the DRM device structure's swapchain flag */
	dev_priv->swap_chain_flag = (int)psDevInfo->psCurrentSwapChain;
	MRSTLFBVSyncIHandler(psDevInfo);

 	
	return 0;
}
#endif

#if 0 /* defined(MRST_USING_INTERRUPTS) */
static IMG_BOOL
MRSTLFBISRHandler(IMG_VOID* pvDevInfo)
{
        MRSTLFB_DEVINFO *psDevInfo = (MRSTLFB_DEVINFO *)pvDevInfo;
#if 0
#ifdef MRST_USING_INTERRUPTS
	MRSTLFB_SWAPCHAIN *psSwapChain;
#endif
#endif
	unsigned long vdc_stat;
	struct drm_psb_private *dev_priv;
#if defined(SUPPORT_DRI_DRM)
	uint32_t pipea_stat = 0;
#endif

        if (!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
                DRM_ERROR("ERROR: interrupt arrived but Display HW is power off\n");
                return IMG_FALSE;
        }
	
#if defined(SUPPORT_DRI_DRM)
	dev_priv = (struct drm_psb_private *) psDevInfo->psDrmDevice->dev_private;

	pipea_stat = PSB_RVDC32(PIPEASTAT);
	//write back to clear all interrupt status bits and reset interrupts.
	PSB_WVDC32(pipea_stat, PIPEASTAT);
	
	vdc_stat = PSB_RVDC32(PSB_INT_IDENTITY_R);
	vdc_stat &= dev_priv->vdc_irq_mask;
	if (vdc_stat & _PSB_VSYNC_PIPEA_FLAG)
	{
		drm_handle_vblank(psDevInfo->psDrmDevice, 0);
	}
#endif

/* Use drm_handle_vblank() as the VSync handler, otherwise kernel would panic if handle 
 * the VSync event again. */
#if 0
#ifdef MRST_USING_INTERRUPTS

	psSwapChain = psDevInfo->psSwapChain;
	vdc_stat = MRSTLFBVSyncReadReg(psDevInfo, PSB_INT_IDENTITY_R);

	if (vdc_stat & _PSB_VSYNC_PIPEA_FLAG)
	{
		if(!psDevInfo->psSwapChain)
		{
			psSwapChain = psDevInfo->psSwapChain;
			(void) MRSTLFBVSyncIHandler(psSwapChain);
		}
	}
#endif
#endif

#if defined(SUPPORT_DRI_DRM)
	vdc_stat = PSB_RVDC32(PSB_INT_IDENTITY_R);
	vdc_stat &= dev_priv->vdc_irq_mask;
	if (vdc_stat & _PSB_DPST_PIPEA_FLAG) {

		/* Check for DPST related interrupts */
		if((pipea_stat & PIPE_DPST_EVENT_STATUS) &&
		   (dev_priv->psb_dpst_state != NULL)) {
			uint32_t pwm_reg = 0;
			uint32_t hist_reg = 0;
			u32 irqCtrl = 0;
			struct dpst_guardband guardband_reg;
			struct dpst_ie_histogram_control ie_hist_cont_reg;

			hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);

			/* Determine if this is histogram or pwm interrupt */
			if(hist_reg & HISTOGRAM_INT_CTRL_CLEAR) {
				/* Notify UM of histogram interrupt */
				psb_dpst_notify_change_um(DPST_EVENT_HIST_INTERRUPT,
				dev_priv->psb_dpst_state);

				/* disable dpst interrupts */
				guardband_reg.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
				guardband_reg.interrupt_enable = 0;
				guardband_reg.interrupt_status = 1;
				PSB_WVDC32(guardband_reg.data, HISTOGRAM_INT_CONTROL);

				ie_hist_cont_reg.data = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
				ie_hist_cont_reg.ie_histogram_enable = 0;
				PSB_WVDC32(ie_hist_cont_reg.data, HISTOGRAM_LOGIC_CONTROL);

				irqCtrl = PSB_RVDC32(PIPEASTAT);
				irqCtrl &= ~PIPE_DPST_EVENT_ENABLE;
				PSB_WVDC32(irqCtrl, PIPEASTAT);
			}
			pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
			if((pwm_reg & PWM_PHASEIN_INT_ENABLE) &&
			   !(pwm_reg & PWM_PHASEIN_ENABLE)) {
				/* Notify UM of the phase complete */
				psb_dpst_notify_change_um(DPST_EVENT_PHASE_COMPLETE,
				dev_priv->psb_dpst_state);

				/* Temporarily get phase mngr ready to generate
				 * another interrupt until this can be moved to
				 * user mode */
				/* PSB_WVDC32(pwm_reg | 0x80010100 | PWM_PHASEIN_ENABLE,
					   PWM_CONTROL_LOGIC); */
			}
		}
	}
#endif
	return IMG_TRUE;
}
#endif

static IMG_BOOL ProcessFlip(IMG_HANDLE  hCmdCookie,
                            IMG_UINT32  ui32DataSize,
                            IMG_VOID   *pvData)
{
	DISPLAYCLASS_FLIP_COMMAND *psFlipCmd;
	MRSTLFB_DEVINFO *psDevInfo;
	MRSTLFB_BUFFER *psBuffer;
	MRSTLFB_SWAPCHAIN *psSwapChain;
#if defined(MRST_USING_INTERRUPTS)
	MRSTLFB_VSYNC_FLIP_ITEM* psFlipItem;
#endif
	unsigned long ulLockFlags;


	if(!hCmdCookie || !pvData)
	{
		return IMG_FALSE;
	}


	psFlipCmd = (DISPLAYCLASS_FLIP_COMMAND*)pvData;

	if (psFlipCmd == IMG_NULL || sizeof(DISPLAYCLASS_FLIP_COMMAND) != ui32DataSize)
	{
		return IMG_FALSE;
	}


	psDevInfo = (MRSTLFB_DEVINFO*)psFlipCmd->hExtDevice;

	psBuffer = (MRSTLFB_BUFFER*)psFlipCmd->hExtBuffer;
	psSwapChain = (MRSTLFB_SWAPCHAIN*) psFlipCmd->hExtSwapChain;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

#if defined(MRST_USING_INTERRUPTS)
	
	if(psFlipCmd->ui32SwapInterval == 0 || psDevInfo->bFlushCommands)
	{
#endif
		DRMLFBFlipBuffer(psDevInfo, psSwapChain, psBuffer);

		
	
		psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete(hCmdCookie, IMG_TRUE);

#if defined(MRST_USING_INTERRUPTS)
		goto ExitTrueUnlock;
	}

	psFlipItem = &psSwapChain->psVSyncFlips[psSwapChain->ulInsertIndex];

	
	if(psFlipItem->bValid == MRST_FALSE)
	{
		unsigned long ulMaxIndex = psSwapChain->ulSwapChainLength - 1;
		
		if(psSwapChain->ulInsertIndex == psSwapChain->ulRemoveIndex)
		{
			
			DRMLFBFlipBuffer(psDevInfo, psSwapChain, psBuffer);

			psFlipItem->bFlipped = MRST_TRUE;
		}
		else
		{
			psFlipItem->bFlipped = MRST_FALSE;
		}

		psFlipItem->hCmdComplete = (MRST_HANDLE)hCmdCookie;
		psFlipItem->ulSwapInterval = (unsigned long)psFlipCmd->ui32SwapInterval;
		psFlipItem->psBuffer = psBuffer;
		psFlipItem->bValid = MRST_TRUE;

		psSwapChain->ulInsertIndex++;
		if(psSwapChain->ulInsertIndex > ulMaxIndex)
		{
			psSwapChain->ulInsertIndex = 0;
		}

		goto ExitTrueUnlock;
	}
	
	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);
	return IMG_FALSE;

ExitTrueUnlock:
#endif
	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);
	return IMG_TRUE;
}


#if defined(PVR_MRST_FB_SET_PAR_ON_INIT)
static void MRSTFBSetPar(struct fb_info *psLINFBInfo)
{
	console_lock();

	if (psLINFBInfo->fbops->fb_set_par != NULL)
	{
		int res;

		res = psLINFBInfo->fbops->fb_set_par(psLINFBInfo);
		if (res != 0)
		{
			printk(KERN_WARNING DRIVER_PREFIX
				": fb_set_par failed: %d\n", res);

		}
	}
	else
	{
		printk(KERN_WARNING DRIVER_PREFIX
			": fb_set_par not set - HW cursor may not work\n");
	}

	console_unlock();
}
#endif

void MRSTLFBSuspend(void)
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
	unsigned long ulLockFlags;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	if (!psDevInfo->bSuspended)
	{
#if !defined(PVR_MRST_STYLE_PM)
		if(psDevInfo->ui32SwapChainNum != 0)
		{
			MRSTLFBDisableVSyncInterrupt(psDevInfo);
		}
#endif
		psDevInfo->bSuspended = MRST_TRUE;
	}

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);
}

void MRSTLFBResume(void)
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
	unsigned long ulLockFlags;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	if (psDevInfo->bSuspended)
	{
#if !defined(PVR_MRST_STYLE_PM)
		if(psDevInfo->ui32SwapChainNum != 0)
		{
			MRSTLFBEnableVSyncInterrupt(psDevInfo);
		}
#endif
		psDevInfo->bSuspended = MRST_FALSE;

		MRSTLFBRestoreLastFlip(psDevInfo);
	}

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

#if !defined(PVR_MRST_STYLE_PM)
	(void) UnblankDisplay(psDevInfo);
#endif
}

#ifdef DRM_PVR_USE_INTEL_FB
#include "mm.h"
int MRSTLFBHandleChangeFB(struct drm_device* dev, struct psb_framebuffer *psbfb)
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
	int i;
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
	struct psb_gtt * pg = dev_priv->pg;
	LinuxMemArea *psLinuxMemArea;

	if( !psDevInfo->sSystemBuffer.bIsContiguous )
		MRSTLFBFreeKernelMem( psDevInfo->sSystemBuffer.uSysAddr.psNonCont );

	psDevInfo->sDisplayFormat.pixelformat = (psbfb->base.depth == 16) ? PVRSRV_PIXEL_FORMAT_RGB565 : PVRSRV_PIXEL_FORMAT_ARGB8888;

	psDevInfo->sDisplayDim.ui32ByteStride = psbfb->base.pitches[0];
	psDevInfo->sDisplayDim.ui32Width = psbfb->base.width;
	psDevInfo->sDisplayDim.ui32Height = psbfb->base.height;

	psDevInfo->sSystemBuffer.ui32BufferSize = psbfb->size;

	if (psbfb->pvrBO != NULL)
	{
		psDevInfo->sSystemBuffer.sCPUVAddr = psbfb->fbdev->screen_base;
		psDevInfo->sSystemBuffer.sDevVAddr.uiAddr = psbfb->offset;
	}
	else
	{
		psDevInfo->sSystemBuffer.sCPUVAddr = pg->vram_addr;
		psDevInfo->sSystemBuffer.sDevVAddr.uiAddr = 0;
	}

	psDevInfo->sSystemBuffer.bIsAllocated = MRST_FALSE;	

	if (psbfb->pvrBO != NULL)
	{
		psLinuxMemArea = (LinuxMemArea *)psbfb->pvrBO->sMemBlk.hOSMemHandle;
	}

	if ((psbfb->pvrBO == NULL) || (psLinuxMemArea->eAreaType != LINUX_MEM_AREA_ALLOC_PAGES))
	{
		psDevInfo->sSystemBuffer.bIsContiguous = MRST_TRUE;
		psDevInfo->sSystemBuffer.uSysAddr.sCont.uiAddr = pg->stolen_base;
	} else {
		struct page **page_list = psLinuxMemArea->uData.sPageList.pvPageList;

		psDevInfo->sSystemBuffer.bIsContiguous = MRST_FALSE;
		psDevInfo->sSystemBuffer.uSysAddr.psNonCont = MRSTLFBAllocKernelMem( sizeof( IMG_SYS_PHYADDR ) * (psbfb->size >> PAGE_SHIFT));	

		for (i = 0; i < psbfb->size >> PAGE_SHIFT; i++) 
		{
			psDevInfo->sSystemBuffer.uSysAddr.psNonCont[i].uiAddr = page_to_pfn( page_list[i] ) << PAGE_SHIFT;
		}
	} 

	return 0;
}

#else

int MRSTLFBHandleChangeFB(struct drm_device* dev, struct psb_framebuffer *psbfb)
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
	int i;
	struct drm_psb_private * dev_priv;
	struct psb_gtt * pg;
	
	if( !psDevInfo->sSystemBuffer.bIsContiguous )
		MRSTLFBFreeKernelMem( psDevInfo->sSystemBuffer.uSysAddr.psNonCont );
	
	dev_priv = (struct drm_psb_private *)dev->dev_private;
	pg = dev_priv->pg;

	psDevInfo->sDisplayFormat.pixelformat = (psbfb->base.depth == 16) ? PVRSRV_PIXEL_FORMAT_RGB565 : PVRSRV_PIXEL_FORMAT_ARGB8888;

	psDevInfo->sDisplayDim.ui32ByteStride = psbfb->base.pitches[0];
	psDevInfo->sDisplayDim.ui32Width = psbfb->base.width;
	psDevInfo->sDisplayDim.ui32Height = psbfb->base.height;

	psDevInfo->sSystemBuffer.ui32BufferSize = psbfb->size;
	//psDevInfo->sSystemBuffer.sCPUVAddr = psbfb->pvKMAddr;
	psDevInfo->sSystemBuffer.sCPUVAddr = pg->vram_addr;
	//psDevInfo->sSystemBuffer.sDevVAddr.uiAddr = psbfb->offsetGTT;
	psDevInfo->sSystemBuffer.sDevVAddr.uiAddr = 0;
	psDevInfo->sSystemBuffer.bIsAllocated = IMG_FALSE;	

	if(psbfb->bo ) 
	{
		
		psDevInfo->sSystemBuffer.bIsContiguous = IMG_FALSE;
		psDevInfo->sSystemBuffer.uSysAddr.psNonCont = MRSTLFBAllocKernelMem( sizeof( IMG_SYS_PHYADDR ) * psbfb->bo->ttm->num_pages);	
		for(i = 0;i < psbfb->bo->ttm->num_pages;++i) 
		{
			/* struct page *p = ttm_tt_get_page( psbfb->bo->ttm, i);

			psDevInfo->sSystemBuffer.uSysAddr.psNonCont[i].uiAddr = page_to_pfn(p) << PAGE_SHIFT;   
			*/
		}
	} 
	else 
	{
		
		//struct drm_device * psDrmDevice = psDevInfo->psDrmDevice;	
		//struct drm_psb_private * dev_priv = (struct drm_psb_private *)psDrmDevice->dev_private;
		//struct psb_gtt * pg = dev_priv->pg;

		psDevInfo->sSystemBuffer.bIsContiguous = IMG_TRUE;
		psDevInfo->sSystemBuffer.uSysAddr.sCont.uiAddr = pg->stolen_base;
	}

	return 0;
}
#endif

static int MRSTLFBFindMainPipe(struct drm_device *dev)  
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)  
	{
		if ( drm_helper_crtc_in_use(crtc) ) 
		{
			struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
			return psb_intel_crtc->pipe;
		}
	}	
	
	return 0;
}

#if 0
static int DRMLFBLeaveVTHandler(struct drm_device *dev)  
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
  	unsigned long ulLockFlags;
 
	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	if (!psDevInfo->bLeaveVT)
	{
		if(psDevInfo->psCurrentSwapChain != NULL)
		{
			FlushInternalVSyncQueue(psDevInfo->psCurrentSwapChain, MRST_TRUE);
			SetFlushStateNoLock(psDevInfo, MRST_TRUE);
		}

		DRMLFBFlipBuffer(psDevInfo, NULL, &psDevInfo->sSystemBuffer);

		psDevInfo->bLeaveVT = MRST_TRUE;
	}

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

	return 0;
}

static int DRMLFBEnterVTHandler(struct drm_device *dev)  
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
  	unsigned long ulLockFlags;

	spin_lock_irqsave(&psDevInfo->sSwapChainLock, ulLockFlags);

	if (psDevInfo->bLeaveVT)
	{
		if(psDevInfo->psCurrentSwapChain != NULL)
		{
			SetFlushStateNoLock(psDevInfo, MRST_FALSE);
		}

		psDevInfo->bLeaveVT = MRST_FALSE;

		MRSTLFBRestoreLastFlip(psDevInfo);
	}

	spin_unlock_irqrestore(&psDevInfo->sSwapChainLock, ulLockFlags);

	return 0;
}
#endif

static MRST_ERROR InitDev(MRSTLFB_DEVINFO *psDevInfo)
{
	MRST_ERROR eError = MRST_ERROR_GENERIC;
	struct fb_info *psLINFBInfo;
	struct drm_device * psDrmDevice = psDevInfo->psDrmDevice;
	struct drm_psb_private * psDrmPrivate = (struct drm_psb_private *)psDrmDevice->dev_private;
	struct psb_fbdev * psPsbFBDev = (struct psb_fbdev *)psDrmPrivate->fbdev;
	struct drm_framebuffer * psDrmFB;
	struct psb_framebuffer *psbfb;
	

	int hdisplay;
	int vdisplay;
	int i;
	unsigned long FBSize;

	psDrmFB = psPsbFBDev->psb_fb_helper.fb;
	if(!psDrmFB) {
		printk(KERN_INFO"%s:Cannot find drm FB", __FUNCTION__);
		return eError;
	}
	psbfb = to_psb_fb(psDrmFB);

	hdisplay = psDrmFB->width;
	vdisplay = psDrmFB->height;
	FBSize = psDrmFB->pitches[0] * psDrmFB->height;

	psLINFBInfo = (struct fb_info*)psPsbFBDev->psb_fb_helper.fbdev;

#if defined(PVR_MRST_FB_SET_PAR_ON_INIT)
	MRSTFBSetPar(psLINFBInfo);
#endif

	
	psDevInfo->sSystemBuffer.bIsContiguous = MRST_TRUE;
	psDevInfo->sSystemBuffer.bIsAllocated = MRST_FALSE;

	MRSTLFBHandleChangeFB(psDrmDevice, psbfb);

    
	psDevInfo->sDisplayFormat.pixelformat = PVRSRV_PIXEL_FORMAT_ARGB8888;
	psDevInfo->psLINFBInfo = psLINFBInfo;


	psDevInfo->ui32MainPipe = MRSTLFBFindMainPipe(psDevInfo->psDrmDevice);
	
	for(i = 0;i < MAX_SWAPCHAINS;++i) 
	{
		psDevInfo->apsSwapChains[i] = NULL;
	}

	
	
	
	psDevInfo->pvRegs = psbfb_vdc_reg(psDevInfo->psDrmDevice);

	if (psDevInfo->pvRegs == NULL)
	{
		eError = PVRSRV_ERROR_BAD_MAPPING;
		printk(KERN_WARNING DRIVER_PREFIX ": Couldn't map registers needed for flipping\n");
		return eError;
	}

	return MRST_OK;
}

static void DeInitDev(MRSTLFB_DEVINFO *psDevInfo)
{

}

MRST_ERROR MRSTLFBInit(struct drm_device * dev)
{
	MRSTLFB_DEVINFO		*psDevInfo;
	//struct drm_psb_private *psDrmPriv = (struct drm_psb_private *)dev->dev_private;

	psDevInfo = GetAnchorPtr();

	if (psDevInfo == NULL)
	{
		PFN_CMD_PROC	 		pfnCmdProcList[MRSTLFB_COMMAND_COUNT];
		IMG_UINT32				aui32SyncCountList[MRSTLFB_COMMAND_COUNT][2];

		psDevInfo = (MRSTLFB_DEVINFO *)MRSTLFBAllocKernelMem(sizeof(MRSTLFB_DEVINFO));

		if(!psDevInfo)
		{
			return (MRST_ERROR_OUT_OF_MEMORY);
		}


		memset(psDevInfo, 0, sizeof(MRSTLFB_DEVINFO));


		SetAnchorPtr((void*)psDevInfo);

		psDevInfo->psDrmDevice = dev;
		psDevInfo->ulRefCount = 0;


		if(InitDev(psDevInfo) != MRST_OK)
		{
			return (MRST_ERROR_INIT_FAILURE);
		}

		if(MRSTLFBGetLibFuncAddr ("PVRGetDisplayClassJTable", &pfnGetPVRJTable) != MRST_OK)
		{
			return (MRST_ERROR_INIT_FAILURE);
		}


		if(!(*pfnGetPVRJTable)(&psDevInfo->sPVRJTable))
		{
			return (MRST_ERROR_INIT_FAILURE);
		}


		spin_lock_init(&psDevInfo->sSwapChainLock);

		psDevInfo->psCurrentSwapChain = NULL;
		psDevInfo->bFlushCommands = MRST_FALSE;

		psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers = 3;
		psDevInfo->sDisplayInfo.ui32MaxSwapChains = MAX_SWAPCHAINS;
		psDevInfo->sDisplayInfo.ui32MaxSwapInterval = 3;
		psDevInfo->sDisplayInfo.ui32MinSwapInterval = 0;

		strncpy(psDevInfo->sDisplayInfo.szDisplayName, DISPLAY_DEVICE_NAME, MAX_DISPLAY_NAME_SIZE);
	

		DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX
			": Maximum number of swap chain buffers: %u\n",
			psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers));


		

		psDevInfo->sDCJTable.ui32TableSize = sizeof(PVRSRV_DC_SRV2DISP_KMJTABLE);
		psDevInfo->sDCJTable.pfnOpenDCDevice = OpenDCDevice;
		psDevInfo->sDCJTable.pfnCloseDCDevice = CloseDCDevice;
		psDevInfo->sDCJTable.pfnEnumDCFormats = EnumDCFormats;
		psDevInfo->sDCJTable.pfnEnumDCDims = EnumDCDims;
		psDevInfo->sDCJTable.pfnGetDCSystemBuffer = GetDCSystemBuffer;
		psDevInfo->sDCJTable.pfnGetDCInfo = GetDCInfo;
		psDevInfo->sDCJTable.pfnGetBufferAddr = GetDCBufferAddr;
		psDevInfo->sDCJTable.pfnCreateDCSwapChain = CreateDCSwapChain;
		psDevInfo->sDCJTable.pfnDestroyDCSwapChain = DestroyDCSwapChain;
		psDevInfo->sDCJTable.pfnSetDCDstRect = SetDCDstRect;
		psDevInfo->sDCJTable.pfnSetDCSrcRect = SetDCSrcRect;
		psDevInfo->sDCJTable.pfnSetDCDstColourKey = SetDCDstColourKey;
		psDevInfo->sDCJTable.pfnSetDCSrcColourKey = SetDCSrcColourKey;
		psDevInfo->sDCJTable.pfnGetDCBuffers = GetDCBuffers;
		psDevInfo->sDCJTable.pfnSwapToDCBuffer = SwapToDCBuffer;
		psDevInfo->sDCJTable.pfnSwapToDCSystem = SwapToDCSystem;
		psDevInfo->sDCJTable.pfnSetDCState = SetDCState;


		if(psDevInfo->sPVRJTable.pfnPVRSRVRegisterDCDevice (
			&psDevInfo->sDCJTable,
			&psDevInfo->uiDeviceID ) != PVRSRV_OK)
		{
			return (MRST_ERROR_DEVICE_REGISTER_FAILED);
		}

		printk("Device ID: %d\n", (int)psDevInfo->uiDeviceID);

#if 0
#if defined (SYS_USING_INTERRUPTS)
		if (psDevInfo->sPVRJTable.pfnPVRSRVRegisterSystemISRHandler(MRSTLFBISRHandler,
									    psDevInfo,
									    0,
									    (IMG_UINT32)psDevInfo->uiDeviceID) != PVRSRV_OK)
		{
			DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX "ISR Installation failed\n"));
			return (MRST_ERROR_INIT_FAILURE);
		}
#endif

		if (psDevInfo->sPVRJTable.pfnPVRSRVRegisterPowerDevice((IMG_UINT32)psDevInfo->uiDeviceID,
								       MRSTLFBPrePowerState, MRSTLFBPostPowerState,
								       IMG_NULL, IMG_NULL,
								       psDevInfo,
								       PVRSRV_DEV_POWER_STATE_ON,
								       PVRSRV_DEV_POWER_STATE_ON) != PVRSRV_OK)
		{
			return (MRST_ERROR_INIT_FAILURE);
		}
#endif

#if defined (MRST_USING_INTERRUPTS)
	
	if(MRSTLFBInstallVSyncISR(psDevInfo,MRSTLFBVSyncISR) != MRST_OK)
	{
		DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX	"ISR Installation failed\n"));
		return (MRST_ERROR_INIT_FAILURE);
	}
#endif

	
	pfnCmdProcList[DC_FLIP_COMMAND] = ProcessFlip;
	
	
	aui32SyncCountList[DC_FLIP_COMMAND][0] = 0; 
	aui32SyncCountList[DC_FLIP_COMMAND][1] = 2; 
	

	if (psDevInfo->sPVRJTable.pfnPVRSRVRegisterCmdProcList (psDevInfo->uiDeviceID,
								&pfnCmdProcList[0],
								aui32SyncCountList,
								MRSTLFB_COMMAND_COUNT) != PVRSRV_OK)
	  {
	    printk(KERN_WARNING DRIVER_PREFIX ": Can't register callback\n");
	    return (MRST_ERROR_CANT_REGISTER_CALLBACK);
	  }


	}

	
	//psDrmPriv->psb_change_fb_handler = MRSTLFBHandleChangeFB;	

	//psDrmPriv->psb_leave_vt_handler = DRMLFBLeaveVTHandler;
	//psDrmPriv->psb_enter_vt_handler = DRMLFBEnterVTHandler;
	
	psDevInfo->ulRefCount++;

	
	return (MRST_OK);	
}

MRST_ERROR MRSTLFBDeinit(void)
{
	MRSTLFB_DEVINFO *psDevInfo, *psDevFirst;

	psDevFirst = GetAnchorPtr();
	psDevInfo = psDevFirst;


	if (psDevInfo == NULL)
	{
		return (MRST_ERROR_GENERIC);
	}


	psDevInfo->ulRefCount--;

	psDevInfo->psDrmDevice = NULL;
	if (psDevInfo->ulRefCount == 0)
	{

		PVRSRV_DC_DISP2SRV_KMJTABLE	*psJTable = &psDevInfo->sPVRJTable;

		if (psDevInfo->sPVRJTable.pfnPVRSRVRemoveCmdProcList (psDevInfo->uiDeviceID, MRSTLFB_COMMAND_COUNT) != PVRSRV_OK)
		{
			return (MRST_ERROR_GENERIC);
		}

		if (psDevInfo->sPVRJTable.pfnPVRSRVRegisterPowerDevice((IMG_UINT32)psDevInfo->uiDeviceID,
								       IMG_NULL, IMG_NULL,
								       IMG_NULL, IMG_NULL, IMG_NULL,
								       PVRSRV_DEV_POWER_STATE_ON,
								       PVRSRV_DEV_POWER_STATE_ON) != PVRSRV_OK)
		{
			return (MRST_ERROR_GENERIC);
		}

#if 0 /* defined (SYS_USING_INTERRUPTS) */
		if (psDevInfo->sPVRJTable.pfnPVRSRVRegisterSystemISRHandler(IMG_NULL, IMG_NULL, 0,
									    (IMG_UINT32)psDevInfo->uiDeviceID) != PVRSRV_OK)
		{
			return (MRST_ERROR_GENERIC);
		}
#endif

#if defined (MRST_USING_INTERRUPTS)
		
		if(MRSTLFBUninstallVSyncISR(psDevInfo) != MRST_OK)
		{
			return (MRST_ERROR_GENERIC);
		}
#endif

		if (psJTable->pfnPVRSRVRemoveDCDevice(psDevInfo->uiDeviceID) != PVRSRV_OK)
		{
			return (MRST_ERROR_GENERIC);
		}

		DeInitDev(psDevInfo);


		MRSTLFBFreeKernelMem(psDevInfo);
	}


	SetAnchorPtr(NULL);


	return (MRST_OK);
}

MRST_ERROR MRSTLFBAllocBuffer(struct MRSTLFB_DEVINFO_TAG *psDevInfo, IMG_UINT32 ui32Size, MRSTLFB_BUFFER **ppBuffer)
{
	IMG_VOID *pvBuf;
	IMG_UINT32 ulPagesNumber;
	IMG_UINT32 ulCounter;
	int i;

	pvBuf = __vmalloc( ui32Size, GFP_KERNEL | __GFP_HIGHMEM, __pgprot((pgprot_val(PAGE_KERNEL ) & ~_PAGE_CACHE_MASK) | _PAGE_CACHE_WC) );
	if( pvBuf == NULL )
	{
		return MRST_ERROR_OUT_OF_MEMORY;
	}

	ulPagesNumber = (ui32Size + PAGE_SIZE -1) / PAGE_SIZE;

	*ppBuffer = MRSTLFBAllocKernelMem( sizeof( MRSTLFB_BUFFER ) );
	(*ppBuffer)->sCPUVAddr = pvBuf;
	(*ppBuffer)->ui32BufferSize = ui32Size;
	(*ppBuffer)->uSysAddr.psNonCont = MRSTLFBAllocKernelMem( sizeof( IMG_SYS_PHYADDR ) * ulPagesNumber);
	(*ppBuffer)->bIsAllocated = MRST_TRUE;
	(*ppBuffer)->bIsContiguous = MRST_FALSE;
	(*ppBuffer)->ui32OwnerTaskID = task_tgid_nr(current);

	i = 0;
	for(ulCounter = 0; ulCounter < ui32Size; ulCounter += PAGE_SIZE)
	{
		(*ppBuffer)->uSysAddr.psNonCont[i++].uiAddr = vmalloc_to_pfn( pvBuf + ulCounter ) << PAGE_SHIFT;
	}

	psb_gtt_map_pvr_memory( psDevInfo->psDrmDevice,
							(unsigned int)*ppBuffer,
							(*ppBuffer)->ui32OwnerTaskID,
							(IMG_CPU_PHYADDR*) (*ppBuffer)->uSysAddr.psNonCont,
							ulPagesNumber,
							&(*ppBuffer)->sDevVAddr.uiAddr, 1);

	(*ppBuffer)->sDevVAddr.uiAddr <<= PAGE_SHIFT;

	return MRST_OK;
}

MRST_ERROR MRSTLFBFreeBuffer(struct MRSTLFB_DEVINFO_TAG *psDevInfo, MRSTLFB_BUFFER **ppBuffer)
{
	if( !(*ppBuffer)->bIsAllocated )
		return MRST_ERROR_INVALID_PARAMS;

	psb_gtt_unmap_pvr_memory( psDevInfo->psDrmDevice,
							  (unsigned int)*ppBuffer,
							  (*ppBuffer)->ui32OwnerTaskID);

	vfree( (*ppBuffer)->sCPUVAddr );

	MRSTLFBFreeKernelMem( (*ppBuffer)->uSysAddr.psNonCont );

	MRSTLFBFreeKernelMem( *ppBuffer);

	*ppBuffer = NULL;

	return MRST_OK;
}
