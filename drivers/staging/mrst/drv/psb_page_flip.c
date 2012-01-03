/*
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,·
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 * Ander Conselvan de Oliveira <ander.conselvan.de.oliveira@intel.com>
 * Pauli Nieminen <pauli.nieminen@intel.com>
 *
 */

#include <linux/spinlock.h>
#include <linux/list.h>

#include <drm/drmP.h>
#include "psb_fb.h"
#include "psb_intel_reg.h"
#include "psb_page_flip.h"
#include "psb_pvr_glue.h"

#include "mdfld_output.h"
#include "mdfld_dsi_output.h"

#if defined(CONFIG_MDFLD_DUAL_DSI_DPU)
#include "mdfld_dsi_dbi_dpu.h"
#elif defined(CONFIG_MDFLD_DSI_DSR)
#include "mdfld_dsi_dbi.h"
#endif

struct pending_flip {
	struct drm_crtc *crtc;
	struct drm_pending_vblank_event *event;
	PVRSRV_KERNEL_MEM_INFO *old_mem_info;
	uint32_t offset;
	struct pvr_pending_sync pending_sync;
};

static void
send_page_flip_event(struct drm_device *dev, int pipe,
		     struct pending_flip *pending_flip)
{
	struct drm_pending_vblank_event *e;
	struct timeval now;
	unsigned long flags;

	if (!pending_flip->event)
		return;

	spin_lock_irqsave(&dev->event_lock, flags);

	e = pending_flip->event;
	do_gettimeofday(&now);
	e->event.sequence = drm_vblank_count(dev, pipe);
	e->event.tv_sec = now.tv_sec;
	e->event.tv_usec = now.tv_usec;
	list_add_tail(&e->base.link,
			&e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);

	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void
write_scanout_regs(struct pending_flip *pending_flip, uint32_t offset)
{
	struct drm_device *dev = pending_flip->crtc->dev;
	struct psb_intel_crtc *psb_intel_crtc =
		to_psb_intel_crtc(pending_flip->crtc);

	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	int reg_offset;

	switch (psb_intel_crtc->pipe) {
	case 0:
		reg_offset = DSPASURF;
		break;
	case 1:
		reg_offset = DSPBSURF;
		break;
	case 2:
		reg_offset = DSPCSURF;
		break;
	default:
		WARN_ON(1);
		return;
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				       OSPM_UHB_FORCE_POWER_ON))
		return;

	iowrite32(offset, dev_priv->vdc_reg + reg_offset);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
}

static void
increase_read_ops_pending(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	if (psKernelMemInfo && psKernelMemInfo->psKernelSyncInfo)
		psKernelMemInfo->psKernelSyncInfo
			->psSyncData->ui32ReadOpsPending++;
}

static void
increase_read_ops_completed(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	if (psKernelMemInfo && psKernelMemInfo->psKernelSyncInfo)
		psKernelMemInfo->psKernelSyncInfo
			->psSyncData->ui32ReadOpsComplete++;
}

static PVRSRV_KERNEL_MEM_INFO *
get_fb_meminfo(struct drm_framebuffer *fb)
{
	struct psb_framebuffer *psbfb = to_psb_fb(fb);
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo;

	if (psb_get_meminfo_by_handle(psbfb->hKernelMemInfo, &psKernelMemInfo))
		return NULL;

	return psKernelMemInfo;
}

static void
psb_intel_flip_complete(struct pending_flip *pending_flip,
		bool failed_vblank_get)
{
	if (pending_flip) {
		struct drm_crtc *crtc = pending_flip->crtc;
		struct drm_device *dev = crtc->dev;
		struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
		int pipe = psb_intel_crtc->pipe;

		send_page_flip_event(dev, pipe, pending_flip);
		if (!failed_vblank_get)
			drm_vblank_put(dev, pipe);
		increase_read_ops_completed(pending_flip->old_mem_info);
		PVRSRVScheduleDeviceCallbacks();

		kfree(pending_flip);
	}
}

void
psb_intel_crtc_process_vblank(struct drm_crtc *crtc)
{
	struct pending_flip *pending_flip = NULL;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);

	pending_flip = xchg(&psb_intel_crtc->pending_flip, NULL);

	psb_intel_flip_complete(pending_flip, false);
}

static void
sync_callback(struct pvr_pending_sync *pending_sync)
{
	struct pending_flip *pending_flip =
		container_of(pending_sync, struct pending_flip, pending_sync);
	struct drm_crtc* crtc = pending_flip->crtc;
	struct drm_device *dev = crtc->dev;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);

	write_scanout_regs(pending_flip, pending_flip->offset);

	if (drm_vblank_get(dev, psb_intel_crtc->pipe)) {
		psb_intel_flip_complete(pending_flip, true);
		pending_flip = NULL;
	}

	pending_flip = xchg(&psb_intel_crtc->pending_flip, pending_flip);

	psb_intel_flip_complete(pending_flip, false);
}

int
psb_intel_crtc_page_flip(struct drm_crtc *crtc,
                         struct drm_framebuffer *fb,
                         struct drm_pending_vblank_event *event)
{
	struct psb_framebuffer *psbfb = to_psb_fb(fb);
	PVRSRV_KERNEL_MEM_INFO *new_fb_mem_info, *current_fb_mem_info;
	struct pending_flip *new_pending_flip;

	if (psb_get_meminfo_by_handle(psbfb->hKernelMemInfo, &new_fb_mem_info))
		return -EINVAL;

	new_pending_flip = kmalloc(sizeof *new_pending_flip, GFP_KERNEL);
	if (!new_pending_flip)
		return -ENOMEM;

	new_pending_flip->crtc = crtc;
	new_pending_flip->event = event;
	new_pending_flip->offset = psbfb->offset;

	current_fb_mem_info = get_fb_meminfo(crtc->fb);

	crtc->fb = fb;

	new_pending_flip->old_mem_info = current_fb_mem_info;

	increase_read_ops_pending(current_fb_mem_info);

	PVRSRVCallbackOnSync(new_fb_mem_info->psKernelSyncInfo,
			     PVRSRV_SYNC_WRITE, sync_callback,
			     &new_pending_flip->pending_sync);

	return 0;
}
