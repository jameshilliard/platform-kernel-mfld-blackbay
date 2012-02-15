/*
 * Copyright (c) 2011-2012, Intel Corporation.
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
 * Ville Syrjälä <ville.syrjala@linux.intel.com>
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

#include "drm_flip.h"

struct pending_flip {
	struct drm_crtc *crtc;
	struct drm_pending_vblank_event *event;
	PVRSRV_KERNEL_MEM_INFO *mem_info;
	PVRSRV_KERNEL_MEM_INFO *old_mem_info;
	uint32_t offset;
	struct list_head uncompleted;
	struct pvr_pending_sync pending_sync;
	struct drm_flip base;
	u32 vbl_count;
	u32 tgid;
	bool vblank_ref;
};

static const u32 dspsurf_reg[] = {
	DSPASURF, DSPBSURF, DSPCSURF,
};

static const u32 pipe_dsl_reg[] = {
	PIPEA_DSL, PIPEB_DSL, PIPEC_DSL,
};

static const u32 pipe_frame_pixel_reg[] = {
	PIPEAFRAMEPIXEL, PIPEBFRAMEPIXEL, PIPECFRAMEPIXEL,
};

static const u32 pipe_frame_high_reg[] = {
	PIPEAFRAMEHIGH, PIPEBFRAMEHIGH, PIPECFRAMEHIGH,
};

static const u32 pipeconf_reg[] = {
	PIPEACONF, PIPEBCONF, PIPECCONF,
};

enum {
	/* somwehat arbitrary value */
	PSB_VBL_CNT_TIMEOUT = 5,
};

static u32 get_vbl_count(struct drm_crtc *crtc)
{
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	struct drm_psb_private *dev_priv = crtc->dev->dev_private;
	int pipe = psb_intel_crtc->pipe;
	u32 high, low1, low2, dsl;
	unsigned int timeout = 0;

	/* All reads must be satisfied during the same frame */
	do {
		low1 = ioread32(dev_priv->vdc_reg + pipe_frame_pixel_reg[pipe]) >> PIPE_FRAME_LOW_SHIFT;
		high = ioread32(dev_priv->vdc_reg + pipe_frame_high_reg[pipe]) << 8;
		dsl = ioread32(dev_priv->vdc_reg + pipe_dsl_reg[pipe]);
		low2 = ioread32(dev_priv->vdc_reg + pipe_frame_pixel_reg[pipe]) >> PIPE_FRAME_LOW_SHIFT;
	} while (low1 != low2 && timeout++ < PSB_VBL_CNT_TIMEOUT);

	WARN_ON(timeout >= PSB_VBL_CNT_TIMEOUT);

	/*
	 * The frame counter seems to increment at the beginning of the
	 * last scanline. The hardware performs the flip at the start
	 * of the vblank, so we want to count those events instead.
	 * Cook up a vblank counter from the frame counter and scanline
	 * counter.
	 */
	return ((high | low2) +
		((dsl >= crtc->hwmode.crtc_vdisplay) &&
		 (dsl < crtc->hwmode.crtc_vtotal - 1))) & 0xffffff;
}

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

static void crtc_flip_cleanup(struct drm_flip *flip)
{
	struct pending_flip *crtc_flip =
		container_of(flip, struct pending_flip, base);
	struct drm_device *dev = crtc_flip->crtc->dev;

	mutex_lock(&dev->mode_config.mutex);
	psb_fb_gtt_unref(dev, crtc_flip->mem_info, crtc_flip->tgid);
	mutex_unlock(&dev->mode_config.mutex);

	kfree(crtc_flip);
}

static void crtc_flip_finish(struct drm_flip *flip)
{
	struct pending_flip *crtc_flip =
		container_of(flip, struct pending_flip, base);

	psb_fb_unref(crtc_flip->old_mem_info);
}

static void psb_flip_driver_flush(struct drm_flip_driver *driver)
{
	struct drm_psb_private *dev_priv =
		container_of(driver, struct drm_psb_private, flip_driver);

	/* Flush posted writes */
	(void)ioread32(dev_priv->vdc_reg + PIPEASTAT);
}

static void crtc_flip_complete(struct drm_flip *flip)
{
	struct pending_flip *crtc_flip =
		container_of(flip, struct pending_flip, base);
	struct drm_device *dev = crtc_flip->crtc->dev;
	int pipe = to_psb_intel_crtc(crtc_flip->crtc)->pipe;

	send_page_flip_event(dev, pipe, crtc_flip);

	if (crtc_flip->vblank_ref)
		drm_vblank_put(dev, pipe);

	psb_fb_increase_read_ops_completed(crtc_flip->old_mem_info);
	pvr_trcmd_check_syn_completions(PVR_TRCMD_FLPCOMP);
	PVRSRVScheduleDeviceCallbacks();
}

static const struct drm_flip_driver_funcs psb_flip_driver_funcs = {
	.flush = psb_flip_driver_flush,
};

void psb_page_flip_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	drm_flip_driver_init(&dev_priv->flip_driver,
			     &psb_flip_driver_funcs);
}

void psb_page_flip_fini(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	drm_flip_driver_fini(&dev_priv->flip_driver);
}

static bool vbl_count_after_eq(u32 a, u32 b)
{
	return !((a - b) & 0x800000);
}

static bool crtc_check(struct drm_flip *pending_flip,
		       u32 vbl_count)
{
	struct pending_flip *crtc_flip =
		container_of(pending_flip, struct pending_flip, base);

	return vbl_count_after_eq(vbl_count, crtc_flip->vbl_count);
}

static bool crtc_flip(struct drm_flip *flip,
		      struct drm_flip *pending_flip)
{
	struct pending_flip *crtc_flip = container_of(flip, struct pending_flip, base);
	struct drm_crtc *crtc = crtc_flip->crtc;
	struct drm_device *dev = crtc->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	int pipe = to_psb_intel_crtc(crtc)->pipe;
	u32 vbl_count;

	crtc_flip->vblank_ref = drm_vblank_get(dev, pipe) == 0;

	vbl_count = get_vbl_count(crtc);

	iowrite32(crtc_flip->offset, dev_priv->vdc_reg + dspsurf_reg[pipe]);

	/* This flip will happen on the next vblank */
	crtc_flip->vbl_count = (vbl_count + 1) & 0xffffff;

	if (pending_flip)
		return crtc_check(pending_flip, vbl_count);

	return false;
}

static bool crtc_vblank(struct drm_flip *pending_flip)
{
	struct pending_flip *crtc_flip =
		container_of(pending_flip, struct pending_flip, base);
	u32 vbl_count = get_vbl_count(crtc_flip->crtc);

	return crtc_check(pending_flip, vbl_count);
}

static const struct drm_flip_helper_funcs crtc_flip_funcs = {
	.flip = crtc_flip,
	.vblank = crtc_vblank,
	.complete = crtc_flip_complete,
	.finish = crtc_flip_finish,
	.cleanup = crtc_flip_cleanup,
};

void psb_page_flip_crtc_init(struct psb_intel_crtc *psb_intel_crtc)
{
	struct drm_device *dev = psb_intel_crtc->base.dev;
	struct drm_psb_private *dev_priv = dev->dev_private;

	drm_flip_helper_init(&psb_intel_crtc->flip_helper,
			     &dev_priv->flip_driver,
			     &crtc_flip_funcs);
}

void psb_page_flip_crtc_fini(struct psb_intel_crtc *psb_intel_crtc)
{
	drm_flip_helper_fini(&psb_intel_crtc->flip_helper);
}

void
psb_intel_crtc_process_vblank(struct drm_crtc *crtc)
{
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);

	drm_flip_helper_vblank(&psb_intel_crtc->flip_helper);
}

static void
sync_callback(struct pvr_pending_sync *pending_sync, bool from_misr)
{
	struct pending_flip *pending_flip =
		container_of(pending_sync, struct pending_flip, pending_sync);
	struct drm_crtc* crtc = pending_flip->crtc;
	struct drm_device *dev = crtc->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	LIST_HEAD(flips);
	bool pipe_enabled = false;

	list_add_tail(&pending_flip->base.list, &flips);

	/* prevent DPMS and whatnot from shooting us in the foot */
	if (from_misr)
		mutex_lock(&dev->mode_config.mutex);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
		int pipe = to_psb_intel_crtc(crtc)->pipe;
		u32 val;

		val = ioread32(dev_priv->vdc_reg + pipeconf_reg[pipe]);

		pipe_enabled = val & PIPEACONF_ENABLE;

		if (!pipe_enabled)
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	if (pipe_enabled) {
		drm_flip_driver_prepare_flips(&dev_priv->flip_driver, &flips);

		drm_flip_driver_schedule_flips(&dev_priv->flip_driver, &flips);

		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	} else
		/* powered off, just complete all pending and new flips */
		drm_flip_driver_complete_flips(&dev_priv->flip_driver, &flips);

	if (from_misr)
		mutex_unlock(&dev->mode_config.mutex);
}

int
psb_intel_crtc_page_flip(struct drm_crtc *crtc,
                         struct drm_framebuffer *fb,
                         struct drm_pending_vblank_event *event)
{
	struct drm_device *dev = crtc->dev;
	struct psb_framebuffer *psbfb = to_psb_fb(fb);
	PVRSRV_KERNEL_MEM_INFO *current_fb_mem_info;
	struct pending_flip *new_pending_flip;
	struct psb_fpriv *priv;
	struct psb_fbdev *fbdev = NULL;
	unsigned long flags;
	struct pvr_trcmd_flpreq *fltrace;
	u32 tgid = psb_get_tgid();
	int ret;

	if (!psbfb->pvrBO)
		return -EINVAL;

	new_pending_flip = kzalloc(sizeof *new_pending_flip, GFP_KERNEL);
	if (!new_pending_flip)
		return -ENOMEM;

	/* keep a reference to the new fb, until it's no longer scanned out. */
	ret = psb_fb_gtt_ref(dev, psbfb->pvrBO);
	if (ret) {
		kfree(new_pending_flip);
		return ret;
	}

	current_fb_mem_info = to_psb_fb(crtc->fb)->pvrBO;

	/* keep a reference to the old fb, for read ops manipulations */
	psb_fb_ref(current_fb_mem_info);

	drm_flip_init(&new_pending_flip->base, &to_psb_intel_crtc(crtc)->flip_helper);

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

	/* In page flip, change the psb_fb_helper.fb to the swapped fb.*/
	if (dev->dev_private)
		fbdev = ((struct drm_psb_private *)dev->dev_private)->fbdev;
	if (fbdev)
		fbdev->psb_fb_helper.fb = fb;
	else
		printk(KERN_ALERT "%s cannot find the fb\n", __func__);

	crtc->fb = fb;

	new_pending_flip->mem_info = psbfb->pvrBO;
	new_pending_flip->old_mem_info = current_fb_mem_info;

	psb_fb_increase_read_ops_pending(current_fb_mem_info);

	fltrace = pvr_trcmd_reserve(PVR_TRCMD_FLPREQ, task_tgid_nr(current),
				  current->comm, sizeof(*fltrace));
	if (current_fb_mem_info && current_fb_mem_info->psKernelSyncInfo)
		pvr_trcmd_set_syn(&fltrace->old_syn,
				current_fb_mem_info->psKernelSyncInfo);
	else
		pvr_trcmd_clear_syn(&fltrace->old_syn);
	pvr_trcmd_set_syn(&fltrace->new_syn, new_fb_mem_info->psKernelSyncInfo);
	pvr_trcmd_commit(fltrace);

	new_pending_flip->tgid = tgid;

	PVRSRVCallbackOnSync(psbfb->pvrBO->psKernelSyncInfo,
			     PVRSRV_SYNC_WRITE, sync_callback,
			     &new_pending_flip->pending_sync);

	return 0;
}
