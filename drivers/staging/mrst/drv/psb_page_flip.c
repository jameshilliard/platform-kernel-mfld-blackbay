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
#include "psb_drv.h"
#include "psb_fb.h"
#include "psb_intel_reg.h"
#include "psb_page_flip.h"
#include "psb_pvr_glue.h"
#include "pvr_trace_cmd.h"

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
	struct list_head uncompleted;
	struct pvr_pending_sync pending_sync;
};

void
psb_cleanup_pending_events(struct drm_device *dev, struct psb_fpriv *priv)
{
	struct drm_pending_vblank_event *e;
	struct pending_flip *pending_flip, *temp;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	list_for_each_entry_safe(pending_flip, temp, &priv->pending_flips,
			uncompleted) {
		e = pending_flip->event;
		pending_flip->event = NULL;
		e->base.destroy(&e->base);
		list_del_init(&pending_flip->uncompleted);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void
send_page_flip_event(struct drm_device *dev, int pipe,
		     struct pending_flip *pending_flip)
{
	struct drm_pending_vblank_event *e;
	struct timeval now;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);

	if (!pending_flip->event)
		goto unlock;

	list_del(&pending_flip->uncompleted);
	e = pending_flip->event;
	do_gettimeofday(&now);
	e->event.sequence = drm_vblank_count(dev, pipe);
	e->event.tv_sec = now.tv_sec;
	e->event.tv_usec = now.tv_usec;
	list_add_tail(&e->base.link,
			&e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);

unlock:
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

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
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
		pvr_trcmd_check_syn_completions(PVR_TRCMD_FLPCOMP);
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
	PVRSRV_KERNEL_MEM_INFO *current_fb_mem_info;
	struct pending_flip *new_pending_flip;
	struct psb_fpriv *priv;
	struct drm_device *dev = crtc->dev;
	struct psb_fbdev *fbdev = NULL;
	unsigned long flags;
	struct pvr_trcmd_flpreq *fltrace;

	if (!psbfb->pvrBO)
		return -EINVAL;

	new_pending_flip = kmalloc(sizeof *new_pending_flip, GFP_KERNEL);
	if (!new_pending_flip)
		return -ENOMEM;

	new_pending_flip->crtc = crtc;
	new_pending_flip->event = event;
	new_pending_flip->offset = psbfb->offset;

	if (event) {
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		priv = psb_fpriv(event->base.file_priv);
		list_add(&new_pending_flip->uncompleted, &priv->pending_flips);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	} else {
		INIT_LIST_HEAD(&new_pending_flip->uncompleted);
	}

	current_fb_mem_info = to_psb_fb(crtc->fb)->pvrBO;

	/* In page flip, change the psb_fb_helper.fb to the swapped fb.*/
	if (dev->dev_private)
		fbdev = ((struct drm_psb_private *)dev->dev_private)->fbdev;
	if (fbdev)
		fbdev->psb_fb_helper.fb = fb;
	else
		printk(KERN_ALERT "%s cannot find the fb\n", __func__);

	crtc->fb = fb;

	new_pending_flip->old_mem_info = current_fb_mem_info;

	increase_read_ops_pending(current_fb_mem_info);

	fltrace = pvr_trcmd_reserve(PVR_TRCMD_FLPREQ, task_tgid_nr(current),
				  current->comm, sizeof(*fltrace));
	if (current_fb_mem_info && current_fb_mem_info->psKernelSyncInfo)
		pvr_trcmd_set_syn(&fltrace->old_syn,
				current_fb_mem_info->psKernelSyncInfo);
	else
		pvr_trcmd_clear_syn(&fltrace->old_syn);
	pvr_trcmd_set_syn(&fltrace->new_syn, new_fb_mem_info->psKernelSyncInfo);
	pvr_trcmd_commit(fltrace);


	PVRSRVCallbackOnSync(psbfb->pvrBO->psKernelSyncInfo,
			     PVRSRV_SYNC_WRITE, sync_callback,
			     &new_pending_flip->pending_sync);

	return 0;
}
