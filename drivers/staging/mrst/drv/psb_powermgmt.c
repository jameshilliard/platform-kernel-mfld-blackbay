/**************************************************************************
 * Copyright (c) 2009, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Benjamin Defnet <benjamin.r.defnet@intel.com>
 *    Rajesh Poornachandran <rajesh.poornachandran@intel.com>
 *
 */
#include "psb_powermgmt.h"
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "pnw_topaz.h"
#include "mdfld_gl3.h"

#include <linux/mutex.h>
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"
#include <asm/intel_scu_ipc.h>

#undef OSPM_GFX_DPK

extern IMG_UINT32 gui32SGXDeviceID;
extern IMG_UINT32 gui32MRSTDisplayDeviceID;
extern IMG_UINT32 gui32MRSTMSVDXDeviceID;
extern IMG_UINT32 gui32MRSTTOPAZDeviceID;

struct drm_device *gpDrmDevice = NULL;
static struct mutex g_ospm_mutex;
static bool gbSuspendInProgress = false;
static bool gbResumeInProgress = false;
static int g_hw_power_status_mask;
static atomic_t g_display_access_count;
static atomic_t g_graphics_access_count;
static atomic_t g_videoenc_access_count;
static atomic_t g_videodec_access_count;

void ospm_power_island_up(int hw_islands);
void ospm_power_island_down(int hw_islands);
static bool gbSuspended = false;
bool gbdispstatus = true;

#if 1
static int ospm_runtime_check_msvdx_hw_busy(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int ret = 1;

	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO DEC HW is not on\n", __func__);
		ret = -1;
		goto out;
	}

	msvdx_priv->msvdx_hw_busy = REG_READ(0x20D0) & (0x1 << 9);
	if (psb_check_msvdx_idle(dev)) {
		//printk(KERN_ALERT "%s video decode hw busy\n", __func__);
		ret = 1;
	} else {
		//printk(KERN_ALERT "%s video decode hw idle\n", __func__);
		ret = 0;
	}
out:
	return ret;
}

static int ospm_runtime_check_topaz_hw_busy(struct drm_device *dev)
{
	//struct drm_psb_private *dev_priv = dev->dev_private;
	//struct topaz_private *topaz_priv = dev_priv->topaz_private;
	int ret = 1;

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO ENC HW is not on\n", __func__);
		ret = -1;
		goto out;
	}

	//topaz_priv->topaz_hw_busy = REG_READ(0x20D0) & (0x1 << 11);

	if (pnw_check_topaz_idle(dev)) {
		//printk(KERN_ALERT "%s video encode hw busy %d\n", __func__,
		//       topaz_priv->topaz_hw_busy);
		ret = 1;
	} else {
		//printk(KERN_ALERT "%s video encode hw idle\n", __func__);
		ret = 0;
	}
out:
	return ret;
}

static int ospm_runtime_pm_msvdx_suspend(struct drm_device *dev)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	//printk(KERN_ALERT "enter %s\n", __func__);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO DEC HW is not on\n", __func__);
		goto out;
	}

	if (atomic_read(&g_videodec_access_count)) {
		//printk(KERN_ALERT "%s videodec access count exit\n", __func__);
		ret = -1;
		goto out;
	}

	msvdx_priv->msvdx_hw_busy = REG_READ(0x20D0) & (0x1 << 9);
	if (psb_check_msvdx_idle(dev)) {
		//printk(KERN_ALERT "%s video decode hw busy exit\n", __func__);
		ret = -2;
		goto out;
	}

	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN);
	psb_irq_uninstall_islands(dev, OSPM_VIDEO_DEC_ISLAND);
	psb_msvdx_save_context(dev);
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
	//printk(KERN_ALERT "%s done\n", __func__);
out:
	return ret;
}

static int ospm_runtime_pm_msvdx_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	//printk(KERN_ALERT "ospm_runtime_pm_msvdx_resume\n");

	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERUP);

	psb_msvdx_restore_context(dev);

	return 0;
}

static int ospm_runtime_pm_topaz_suspend(struct drm_device *dev)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	int encode_ctx = 0, encode_running = 0;

	//printk(KERN_ALERT "enter %s\n", __func__);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture) {
			encode_ctx = 1;
			break;
		}
	}

	/* have encode context, but not started, or is just closed */
	if (encode_ctx && dev_priv->topaz_ctx)
		encode_running = 1;

	if (encode_ctx)
		PSB_DEBUG_PM("Topaz: has encode context, running=%d\n",
			     encode_running);
	else
		PSB_DEBUG_PM("Topaz: no encode context\n");

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO ENC HW is not on\n", __func__);
		goto out;
	}

	if (atomic_read(&g_videoenc_access_count)) {
		//printk(KERN_ALERT "%s videoenc access count exit\n", __func__);
		ret = -1;
		goto out;
	}

	if (pnw_check_topaz_idle(dev)) {
		//printk(KERN_ALERT "%s video encode hw busy exit\n", __func__);
		ret = -2;
		goto out;
	}

	psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);

	if (encode_running) /* has encode session running */
		pnw_topaz_save_mtx_state(dev);
	PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERDOWN);

	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
	//printk(KERN_ALERT "%s done\n", __func__);
out:
	return ret;
}

static int ospm_runtime_pm_topaz_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	int encode_ctx = 0, encode_running = 0;

	//printk(KERN_ALERT "ospm_runtime_pm_topaz_resume\n");
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture) {
			encode_ctx = 1;
			break;
		}
	}

	/* have encode context, but not started, or is just closed */
	if (encode_ctx && dev_priv->topaz_ctx)
		encode_running = 1;

	if (encode_ctx)
		PSB_DEBUG_PM("Topaz: has encode context, running=%d\n",
			     encode_running);
	else
		PSB_DEBUG_PM("Topaz: no encode running\n");

	if (encode_running) { /* has encode session running */
		psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
		pnw_topaz_restore_mtx_state(dev);
	}
	PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERUP);

	return 0;
}
#endif

#ifdef FIX_OSPM_POWER_DOWN
void ospm_apm_power_down_msvdx(struct drm_device *dev)
{
	return;
	mutex_lock(&g_ospm_mutex);

	if (atomic_read(&g_videodec_access_count))
		goto out;
	if (psb_check_msvdx_idle(dev))
		goto out;

	gbSuspendInProgress = true;
	psb_msvdx_save_context(dev);
#ifdef FIXME_MRST_VIDEO_DEC
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
#endif
	gbSuspendInProgress = false;
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}

void ospm_apm_power_down_topaz(struct drm_device *dev)
{
	return; /* todo for OSPM */

	mutex_lock(&g_ospm_mutex);

	if (atomic_read(&g_videoenc_access_count))
		goto out;
	if (lnc_check_topaz_idle(dev))
		goto out;

	gbSuspendInProgress = true;
	lnc_topaz_save_mtx_state(dev);
	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
	gbSuspendInProgress = false;
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}
#else
void ospm_apm_power_down_msvdx(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	mutex_lock(&g_ospm_mutex);
	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
		goto out;

	if (atomic_read(&g_videodec_access_count))
		goto out;
	if (psb_check_msvdx_idle(dev))
		goto out;

	gbSuspendInProgress = true;
	psb_msvdx_save_context(dev);
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
	gbSuspendInProgress = false;
	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN);
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}

void ospm_apm_power_down_topaz(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;

	mutex_lock(&g_ospm_mutex);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
		goto out;
	if (atomic_read(&g_videoenc_access_count))
		goto out;
	if (pnw_check_topaz_idle(dev))
		goto out;

	gbSuspendInProgress = true;
	psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
	pnw_topaz_save_mtx_state(dev);
	PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERDOWN);

	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
	gbSuspendInProgress = false;
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}
#endif
/*
 * ospm_power_init
 *
 * Description: Initialize this ospm power management module
 */
void ospm_power_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *)dev->dev_private;

	gpDrmDevice = dev;

	dev_priv->apm_reg = MDFLD_MSG_READ32(PSB_PUNIT_PORT, PSB_APMBA);
	dev_priv->ospm_base = MDFLD_MSG_READ32(PSB_PUNIT_PORT, PSB_OSPMBA);

	dev_priv->apm_base = dev_priv->apm_reg & 0xffff;
	dev_priv->ospm_base &= 0xffff;

	mutex_init(&g_ospm_mutex);
	g_hw_power_status_mask = OSPM_ALL_ISLANDS;
	atomic_set(&g_display_access_count, 0);
	atomic_set(&g_graphics_access_count, 0);
	atomic_set(&g_videoenc_access_count, 0);
	atomic_set(&g_videodec_access_count, 0);

#ifdef OSPM_STAT
	dev_priv->graphics_state = PSB_PWR_STATE_ON;
	dev_priv->gfx_last_mode_change = jiffies;
	dev_priv->gfx_on_time = 0;
	dev_priv->gfx_off_time = 0;
#endif

	spin_lock_init(&dev_priv->ospm_lock);
}

/*
 * ospm_power_uninit
 *
 * Description: Uninitialize this ospm power management module
 */
void ospm_power_uninit(struct drm_device *drm_dev)
{
	mutex_destroy(&g_ospm_mutex);
	pm_runtime_forbid(&drm_dev->pdev->dev);
	pm_runtime_get_noresume(&drm_dev->pdev->dev);
}


/*
 * save_display_registers
 *
 * Description: We are going to suspend so save current display
 * register state.
 */
static int save_display_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_crtc * crtc;
	struct drm_connector * connector;

	/* Display arbitration control + watermarks */
	dev_priv->saveDSPARB = PSB_RVDC32(DSPARB);
	dev_priv->saveDSPFW1 = PSB_RVDC32(DSPFW1);
	dev_priv->saveDSPFW2 = PSB_RVDC32(DSPFW2);
	dev_priv->saveDSPFW3 = PSB_RVDC32(DSPFW3);
	dev_priv->saveDSPFW4 = PSB_RVDC32(DSPFW4);
	dev_priv->saveDSPFW5 = PSB_RVDC32(DSPFW5);
	dev_priv->saveDSPFW6 = PSB_RVDC32(DSPFW6);
	dev_priv->saveCHICKENBIT = PSB_RVDC32(DSPCHICKENBIT);

	/*save crtc and output state*/
	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (drm_helper_crtc_in_use(crtc)) {
			crtc->funcs->save(crtc);
		}
	}

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		connector->funcs->save(connector);
	}
	mutex_unlock(&dev->mode_config.mutex);

	/* Interrupt state */
	/*
	 * Handled in psb_irq.c
	 */

	return 0;
}

/*
 * restore_display_registers
 *
 * Description: We are going to resume so restore display register state.
 */
static int restore_display_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_crtc * crtc;
	struct drm_connector * connector;

	/* Display arbitration + watermarks */
	PSB_WVDC32(dev_priv->saveDSPARB, DSPARB);
	PSB_WVDC32(dev_priv->saveDSPFW1, DSPFW1);
	PSB_WVDC32(dev_priv->saveDSPFW2, DSPFW2);
	PSB_WVDC32(dev_priv->saveDSPFW3, DSPFW3);
	PSB_WVDC32(dev_priv->saveDSPFW4, DSPFW4);
	PSB_WVDC32(dev_priv->saveDSPFW5, DSPFW5);
	PSB_WVDC32(dev_priv->saveDSPFW6, DSPFW6);
	PSB_WVDC32(dev_priv->saveCHICKENBIT, DSPCHICKENBIT);

	/*make sure VGA plane is off. it initializes to on after reset!*/
	PSB_WVDC32(0x80000000, VGACNTRL);

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (drm_helper_crtc_in_use(crtc))
			crtc->funcs->restore(crtc);
	}

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		connector->funcs->restore(connector);
	}
	mutex_unlock(&dev->mode_config.mutex);

	/*Interrupt state*/
	/*
	 * Handled in psb_irq.c
	 */

	return 0;
}
/*
 * mdfld_save_display_registers
 *
 * Description: We are going to suspend so save current display
 * register state.
 *
 * Notes: FIXME_JLIU7 need to add the support for DPI MIPI & HDMI audio
 */
static int mdfld_save_display_registers(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int i;

	/* regester */
	u32 dpll_reg = MRST_DPLL_A;
	u32 fp_reg = MRST_FPA0;
	u32 pipeconf_reg = PIPEACONF;
	u32 htot_reg = HTOTAL_A;
	u32 hblank_reg = HBLANK_A;
	u32 hsync_reg = HSYNC_A;
	u32 vtot_reg = VTOTAL_A;
	u32 vblank_reg = VBLANK_A;
	u32 vsync_reg = VSYNC_A;
	u32 pipesrc_reg = PIPEASRC;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dsptileoff_reg = DSPATILEOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dsppos_reg = DSPAPOS;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;
	u32 dspcntr_reg = DSPACNTR;
	u32 dspstatus_reg = PIPEASTAT;
	u32 palette_reg = PALETTE_A;

	/* pointer to values */
	u32 *dpll_val = &dev_priv->saveDPLL_A;
	u32 *fp_val = &dev_priv->saveFPA0;
	u32 *pipeconf_val = &dev_priv->savePIPEACONF;
	u32 *htot_val = &dev_priv->saveHTOTAL_A;
	u32 *hblank_val = &dev_priv->saveHBLANK_A;
	u32 *hsync_val = &dev_priv->saveHSYNC_A;
	u32 *vtot_val = &dev_priv->saveVTOTAL_A;
	u32 *vblank_val = &dev_priv->saveVBLANK_A;
	u32 *vsync_val = &dev_priv->saveVSYNC_A;
	u32 *pipesrc_val = &dev_priv->savePIPEASRC;
	u32 *dspstride_val = &dev_priv->saveDSPASTRIDE;
	u32 *dsplinoff_val = &dev_priv->saveDSPALINOFF;
	u32 *dsptileoff_val = &dev_priv->saveDSPATILEOFF;
	u32 *dspsize_val = &dev_priv->saveDSPASIZE;
	u32 *dsppos_val = &dev_priv->saveDSPAPOS;
	u32 *dspsurf_val = &dev_priv->saveDSPASURF;
	u32 *mipi_val = &dev_priv->saveMIPI;
	u32 *dspcntr_val = &dev_priv->saveDSPACNTR;
	u32 *dspstatus_val = &dev_priv->saveDSPASTATUS;
	u32 *palette_val = dev_priv->save_palette_a;
	PSB_DEBUG_ENTRY("\n");

	switch (pipe) {
	case 0:
		break;
	case 1:
		/* regester */
		dpll_reg = MDFLD_DPLL_B;
		fp_reg = MDFLD_DPLL_DIV0;
		pipeconf_reg = PIPEBCONF;
		htot_reg = HTOTAL_B;
		hblank_reg = HBLANK_B;
		hsync_reg = HSYNC_B;
		vtot_reg = VTOTAL_B;
		vblank_reg = VBLANK_B;
		vsync_reg = VSYNC_B;
		pipesrc_reg = PIPEBSRC;
		dspstride_reg = DSPBSTRIDE;
		dsplinoff_reg = DSPBLINOFF;
		dsptileoff_reg = DSPBTILEOFF;
		dspsize_reg = DSPBSIZE;
		dsppos_reg = DSPBPOS;
		dspsurf_reg = DSPBSURF;
		dspcntr_reg = DSPBCNTR;
		dspstatus_reg = PIPEBSTAT;
		palette_reg = PALETTE_B;

		/* values */
		dpll_val = &dev_priv->saveDPLL_B;
		fp_val = &dev_priv->saveFPB0;
		pipeconf_val = &dev_priv->savePIPEBCONF;
		htot_val = &dev_priv->saveHTOTAL_B;
		hblank_val = &dev_priv->saveHBLANK_B;
		hsync_val = &dev_priv->saveHSYNC_B;
		vtot_val = &dev_priv->saveVTOTAL_B;
		vblank_val = &dev_priv->saveVBLANK_B;
		vsync_val = &dev_priv->saveVSYNC_B;
		pipesrc_val = &dev_priv->savePIPEBSRC;
		dspstride_val = &dev_priv->saveDSPBSTRIDE;
		dsplinoff_val = &dev_priv->saveDSPBLINOFF;
		dsptileoff_val = &dev_priv->saveDSPBTILEOFF;
		dspsize_val = &dev_priv->saveDSPBSIZE;
		dsppos_val = &dev_priv->saveDSPBPOS;
		dspsurf_val = &dev_priv->saveDSPBSURF;
		dspcntr_val = &dev_priv->saveDSPBCNTR;
		dspstatus_val = &dev_priv->saveDSPBSTATUS;
		palette_val = dev_priv->save_palette_b;
		break;
	case 2:
		/* regester */
		pipeconf_reg = PIPECCONF;
		htot_reg = HTOTAL_C;
		hblank_reg = HBLANK_C;
		hsync_reg = HSYNC_C;
		vtot_reg = VTOTAL_C;
		vblank_reg = VBLANK_C;
		vsync_reg = VSYNC_C;
		pipesrc_reg = PIPECSRC;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dsptileoff_reg = DSPCTILEOFF;
		dspsize_reg = DSPCSIZE;
		dsppos_reg = DSPCPOS;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		dspcntr_reg = DSPCCNTR;
		dspstatus_reg = PIPECSTAT;
		palette_reg = PALETTE_C;

		/* pointer to values */
		pipeconf_val = &dev_priv->savePIPECCONF;
		htot_val = &dev_priv->saveHTOTAL_C;
		hblank_val = &dev_priv->saveHBLANK_C;
		hsync_val = &dev_priv->saveHSYNC_C;
		vtot_val = &dev_priv->saveVTOTAL_C;
		vblank_val = &dev_priv->saveVBLANK_C;
		vsync_val = &dev_priv->saveVSYNC_C;
		pipesrc_val = &dev_priv->savePIPECSRC;
		dspstride_val = &dev_priv->saveDSPCSTRIDE;
		dsplinoff_val = &dev_priv->saveDSPCLINOFF;
		dsptileoff_val = &dev_priv->saveDSPCTILEOFF;
		dspsize_val = &dev_priv->saveDSPCSIZE;
		dsppos_val = &dev_priv->saveDSPCPOS;
		dspsurf_val = &dev_priv->saveDSPCSURF;
		mipi_val = &dev_priv->saveMIPI_C;
		dspcntr_val = &dev_priv->saveDSPCCNTR;
		dspstatus_val = &dev_priv->saveDSPCSTATUS;
		palette_val = dev_priv->save_palette_c;
		break;
	default:
		DRM_ERROR("%s, invalid pipe number. \n", __FUNCTION__);
		return -EINVAL;
	}

	/* Pipe & plane A info */
	*dpll_val = PSB_RVDC32(dpll_reg);
	*fp_val = PSB_RVDC32(fp_reg);
	*pipeconf_val = PSB_RVDC32(pipeconf_reg);
	*htot_val = PSB_RVDC32(htot_reg);
	*hblank_val = PSB_RVDC32(hblank_reg);
	*hsync_val = PSB_RVDC32(hsync_reg);
	*vtot_val = PSB_RVDC32(vtot_reg);
	*vblank_val = PSB_RVDC32(vblank_reg);
	*vsync_val = PSB_RVDC32(vsync_reg);
	*pipesrc_val = PSB_RVDC32(pipesrc_reg);
	*dspstride_val = PSB_RVDC32(dspstride_reg);
	*dsplinoff_val = PSB_RVDC32(dsplinoff_reg);
	*dsptileoff_val = PSB_RVDC32(dsptileoff_reg);
	*dspsize_val = PSB_RVDC32(dspsize_reg);
	*dsppos_val = PSB_RVDC32(dsppos_reg);
	*dspsurf_val = PSB_RVDC32(dspsurf_reg);
	*dspcntr_val = PSB_RVDC32(dspcntr_reg);
	*dspstatus_val = PSB_RVDC32(dspstatus_reg);

	/*save palette (gamma) */
	for (i = 0; i < 256; i++)
		palette_val[i] = PSB_RVDC32(palette_reg + (i << 2));

	if (pipe == 1) {
		dev_priv->savePFIT_CONTROL = PSB_RVDC32(PFIT_CONTROL);
		dev_priv->savePFIT_PGM_RATIOS = PSB_RVDC32(PFIT_PGM_RATIOS);

		dev_priv->saveHDMIPHYMISCCTL = PSB_RVDC32(HDMIPHYMISCCTL);
		dev_priv->saveHDMIB_CONTROL = PSB_RVDC32(HDMIB_CONTROL);
		return 0;
	}

	*mipi_val = PSB_RVDC32(mipi_reg);
	return 0;
}
/*
 * mdfld_save_cursor_overlay_registers
 *
 * Description: We are going to suspend so save current cursor and overlay display
 * register state.
 */
static int mdfld_save_cursor_overlay_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	/*save cursor regs*/
	dev_priv->saveDSPACURSOR_CTRL = PSB_RVDC32(CURACNTR);
	dev_priv->saveDSPACURSOR_BASE = PSB_RVDC32(CURABASE);
	dev_priv->saveDSPACURSOR_POS = PSB_RVDC32(CURAPOS);

	dev_priv->saveDSPBCURSOR_CTRL = PSB_RVDC32(CURBCNTR);
	dev_priv->saveDSPBCURSOR_BASE = PSB_RVDC32(CURBBASE);
	dev_priv->saveDSPBCURSOR_POS = PSB_RVDC32(CURBPOS);

	dev_priv->saveDSPCCURSOR_CTRL = PSB_RVDC32(CURCCNTR);
	dev_priv->saveDSPCCURSOR_BASE = PSB_RVDC32(CURCBASE);
	dev_priv->saveDSPCCURSOR_POS = PSB_RVDC32(CURCPOS);

	/* HW overlay */
	dev_priv->saveOV_OVADD = PSB_RVDC32(OV_OVADD);
	dev_priv->saveOV_OGAMC0 = PSB_RVDC32(OV_OGAMC0);
	dev_priv->saveOV_OGAMC1 = PSB_RVDC32(OV_OGAMC1);
	dev_priv->saveOV_OGAMC2 = PSB_RVDC32(OV_OGAMC2);
	dev_priv->saveOV_OGAMC3 = PSB_RVDC32(OV_OGAMC3);
	dev_priv->saveOV_OGAMC4 = PSB_RVDC32(OV_OGAMC4);
	dev_priv->saveOV_OGAMC5 = PSB_RVDC32(OV_OGAMC5);

	dev_priv->saveOV_OVADD_C = PSB_RVDC32(OV_OVADD + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC0_C = PSB_RVDC32(OV_OGAMC0 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC1_C = PSB_RVDC32(OV_OGAMC1 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC2_C = PSB_RVDC32(OV_OGAMC2 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC3_C = PSB_RVDC32(OV_OGAMC3 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC4_C = PSB_RVDC32(OV_OGAMC4 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC5_C = PSB_RVDC32(OV_OGAMC5 + OV_C_OFFSET);

	return 0;
}
/*
 * mdfld_restore_display_registers
 *
 * Description: We are going to resume so restore display register state.
 *
 * Notes: FIXME_JLIU7 need to add the support for DPI MIPI & HDMI audio
 */
static int mdfld_restore_display_registers(struct drm_device *dev, int pipe)
{
	//to get  panel out of ULPS mode.
	u32 temp = 0;
	u32 device_ready_reg = DEVICE_READY_REG;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dsi_dbi_output * dsi_output = dev_priv->dbi_output;
	struct mdfld_dsi_config * dsi_config = NULL;
	u32 i = 0;
	u32 dpll = 0;
	u32 timeout = 0;

	/* regester */
	u32 dpll_reg = MRST_DPLL_A;
	u32 fp_reg = MRST_FPA0;
	u32 pipeconf_reg = PIPEACONF;
	u32 htot_reg = HTOTAL_A;
	u32 hblank_reg = HBLANK_A;
	u32 hsync_reg = HSYNC_A;
	u32 vtot_reg = VTOTAL_A;
	u32 vblank_reg = VBLANK_A;
	u32 vsync_reg = VSYNC_A;
	u32 pipesrc_reg = PIPEASRC;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dsptileoff_reg = DSPATILEOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dsppos_reg = DSPAPOS;
	u32 dspsurf_reg = DSPASURF;
	u32 dspstatus_reg = PIPEASTAT;
	u32 mipi_reg = MIPI;
	u32 dspcntr_reg = DSPACNTR;
	u32 palette_reg = PALETTE_A;

	/* values */
	u32 dpll_val = dev_priv->saveDPLL_A & ~DPLL_VCO_ENABLE;
	u32 fp_val = dev_priv->saveFPA0;
	u32 pipeconf_val = dev_priv->savePIPEACONF;
	u32 htot_val = dev_priv->saveHTOTAL_A;
	u32 hblank_val = dev_priv->saveHBLANK_A;
	u32 hsync_val = dev_priv->saveHSYNC_A;
	u32 vtot_val = dev_priv->saveVTOTAL_A;
	u32 vblank_val = dev_priv->saveVBLANK_A;
	u32 vsync_val = dev_priv->saveVSYNC_A;
	u32 pipesrc_val = dev_priv->savePIPEASRC;
	u32 dspstride_val = dev_priv->saveDSPASTRIDE;
	u32 dsplinoff_val = dev_priv->saveDSPALINOFF;
	u32 dsptileoff_val = dev_priv->saveDSPATILEOFF;
	u32 dspsize_val = dev_priv->saveDSPASIZE;
	u32 dsppos_val = dev_priv->saveDSPAPOS;
	u32 dspsurf_val = dev_priv->saveDSPASURF;
	u32 dspstatus_val = dev_priv->saveDSPASTATUS;
	u32 mipi_val = dev_priv->saveMIPI;
	u32 dspcntr_val = dev_priv->saveDSPACNTR;
	u32 *palette_val = dev_priv->save_palette_a;
	PSB_DEBUG_ENTRY("\n");

	switch (pipe) {
	case 0:
		dsi_config = dev_priv->dsi_configs[0];
		break;
	case 1:
		/* regester */
		dpll_reg = MDFLD_DPLL_B;
		fp_reg = MDFLD_DPLL_DIV0;
		pipeconf_reg = PIPEBCONF;
		htot_reg = HTOTAL_B;
		hblank_reg = HBLANK_B;
		hsync_reg = HSYNC_B;
		vtot_reg = VTOTAL_B;
		vblank_reg = VBLANK_B;
		vsync_reg = VSYNC_B;
		pipesrc_reg = PIPEBSRC;
		dspstride_reg = DSPBSTRIDE;
		dsplinoff_reg = DSPBLINOFF;
		dsptileoff_reg = DSPBTILEOFF;
		dspsize_reg = DSPBSIZE;
		dsppos_reg = DSPBPOS;
		dspsurf_reg = DSPBSURF;
		dspcntr_reg = DSPBCNTR;
		dspstatus_reg = PIPEBSTAT;
		palette_reg = PALETTE_B;

		/* values */
		dpll_val = dev_priv->saveDPLL_B & ~DPLL_VCO_ENABLE;
		fp_val = dev_priv->saveFPB0;
		pipeconf_val = dev_priv->savePIPEBCONF;
		htot_val = dev_priv->saveHTOTAL_B;
		hblank_val = dev_priv->saveHBLANK_B;
		hsync_val = dev_priv->saveHSYNC_B;
		vtot_val = dev_priv->saveVTOTAL_B;
		vblank_val = dev_priv->saveVBLANK_B;
		vsync_val = dev_priv->saveVSYNC_B;
		pipesrc_val = dev_priv->savePIPEBSRC;
		dspstride_val = dev_priv->saveDSPBSTRIDE;
		dsplinoff_val = dev_priv->saveDSPBLINOFF;
		dsptileoff_val = dev_priv->saveDSPBTILEOFF;
		dspsize_val = dev_priv->saveDSPBSIZE;
		dsppos_val = dev_priv->saveDSPBPOS;
		dspsurf_val = dev_priv->saveDSPBSURF;
		dspcntr_val = dev_priv->saveDSPBCNTR;
		dspstatus_val = dev_priv->saveDSPBSTATUS;
		palette_val = dev_priv->save_palette_b;
		break;
	case 2:
		dsi_output = dev_priv->dbi_output2;

		/* regester */
		pipeconf_reg = PIPECCONF;
		htot_reg = HTOTAL_C;
		hblank_reg = HBLANK_C;
		hsync_reg = HSYNC_C;
		vtot_reg = VTOTAL_C;
		vblank_reg = VBLANK_C;
		vsync_reg = VSYNC_C;
		pipesrc_reg = PIPECSRC;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dsptileoff_reg = DSPCTILEOFF;
		dspsize_reg = DSPCSIZE;
		dsppos_reg = DSPCPOS;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		dspcntr_reg = DSPCCNTR;
		dspstatus_reg = PIPECSTAT;
		palette_reg = PALETTE_C;

		/* values */
		pipeconf_val = dev_priv->savePIPECCONF;
		htot_val = dev_priv->saveHTOTAL_C;
		hblank_val = dev_priv->saveHBLANK_C;
		hsync_val = dev_priv->saveHSYNC_C;
		vtot_val = dev_priv->saveVTOTAL_C;
		vblank_val = dev_priv->saveVBLANK_C;
		vsync_val = dev_priv->saveVSYNC_C;
		pipesrc_val = dev_priv->savePIPECSRC;
		dspstride_val = dev_priv->saveDSPCSTRIDE;
		dsplinoff_val = dev_priv->saveDSPCLINOFF;
		dsptileoff_val = dev_priv->saveDSPCTILEOFF;
		dspsize_val = dev_priv->saveDSPCSIZE;
		dsppos_val = dev_priv->saveDSPCPOS;
		dspsurf_val = dev_priv->saveDSPCSURF;
		mipi_val = dev_priv->saveMIPI_C;
		dspcntr_val = dev_priv->saveDSPCCNTR;
		dspstatus_val = dev_priv->saveDSPCSTATUS;
		palette_val = dev_priv->save_palette_c;

		dsi_config = dev_priv->dsi_configs[1];
		break;
	default:
		DRM_ERROR("%s, invalid pipe number. \n", __FUNCTION__);
		return -EINVAL;
	}

	/*make sure VGA plane is off. it initializes to on after reset!*/
	PSB_WVDC32(0x80000000, VGACNTRL);

	if (pipe == 1) {
		PSB_WVDC32(dpll_val & ~DPLL_VCO_ENABLE, dpll_reg);
		PSB_RVDC32(dpll_reg);

		PSB_WVDC32(fp_val, fp_reg);
	} else {

		dpll = PSB_RVDC32(dpll_reg);

		if (!(dpll & DPLL_VCO_ENABLE)) {

			/* When ungating power of DPLL, needs to wait 0.5us before enable the VCO */
			if (dpll & MDFLD_PWR_GATE_EN) {
				dpll &= ~MDFLD_PWR_GATE_EN;
				PSB_WVDC32(dpll, dpll_reg);
				/* FIXME_MDFLD PO - change 500 to 1 after PO */
				udelay(500);
			}

			PSB_WVDC32(fp_val, fp_reg);
			PSB_WVDC32(dpll_val, dpll_reg);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);

			dpll_val |= DPLL_VCO_ENABLE;
			PSB_WVDC32(dpll_val, dpll_reg);
			PSB_RVDC32(dpll_reg);

			/* wait for DSI PLL to lock */
			while ((timeout < 20000) && !(PSB_RVDC32(pipeconf_reg) & PIPECONF_DSIPLL_LOCK)) {
				udelay(150);
				timeout ++;
			}

			if (timeout == 20000) {
				DRM_ERROR("%s, can't lock DSIPLL. \n", __FUNCTION__);
				return -EINVAL;
			}
		}
	}
	/* Restore mode */
	PSB_WVDC32(htot_val, htot_reg);
	PSB_WVDC32(hblank_val, hblank_reg);
	PSB_WVDC32(hsync_val, hsync_reg);
	PSB_WVDC32(vtot_val, vtot_reg);
	PSB_WVDC32(vblank_val, vblank_reg);
	PSB_WVDC32(vsync_val, vsync_reg);
	PSB_WVDC32(pipesrc_val, pipesrc_reg);
	PSB_WVDC32(dspstatus_val, dspstatus_reg);

	/*set up the plane*/
	PSB_WVDC32(dspstride_val, dspstride_reg);
	PSB_WVDC32(dsplinoff_val, dsplinoff_reg);
	PSB_WVDC32(dsptileoff_val, dsptileoff_reg);
	PSB_WVDC32(dspsize_val, dspsize_reg);
	PSB_WVDC32(dsppos_val, dsppos_reg);
	PSB_WVDC32(dspsurf_val, dspsurf_reg);

	if (pipe == 1) {
		/* restore palette (gamma) */
		/*DRM_UDELAY(50000); */
		for (i = 0; i < 256; i++)
			PSB_WVDC32(palette_val[i], palette_reg + (i << 2));

		PSB_WVDC32(dev_priv->savePFIT_CONTROL, PFIT_CONTROL);
		PSB_WVDC32(dev_priv->savePFIT_PGM_RATIOS, PFIT_PGM_RATIOS);

		/*TODO: resume HDMI port */

		/*TODO: resume pipe*/

		/*enable the plane*/
		PSB_WVDC32(dspcntr_val & ~DISPLAY_PLANE_ENABLE, dspcntr_reg);

		return 0;
	}

	/*set up pipe related registers*/
	PSB_WVDC32(mipi_val, mipi_reg);

	/*setup MIPI adapter + MIPI IP registers*/
	if (dsi_config)
		mdfld_dsi_controller_init(dsi_config, pipe);

	if (in_atomic() || in_interrupt())
		mdelay(20);
	else
		msleep(20);

	/*enable the plane*/
	PSB_WVDC32(dspcntr_val, dspcntr_reg);

	if (in_atomic() || in_interrupt())
		mdelay(20);
	else
		msleep(20);

	/* LP Hold Release */
	temp = REG_READ(mipi_reg);
	temp |= LP_OUTPUT_HOLD_RELEASE;
	REG_WRITE(mipi_reg, temp);
	mdelay(1);


	/* Set DSI host to exit from Utra Low Power State */
	temp = REG_READ(device_ready_reg);
	temp &= ~ULPS_MASK;
	temp |= 0x3;
	temp |= EXIT_ULPS_DEV_READY;
	REG_WRITE(device_ready_reg, temp);
	mdelay(1);

	temp = REG_READ(device_ready_reg);
	temp &= ~ULPS_MASK;
	temp |= EXITING_ULPS;
	REG_WRITE(device_ready_reg, temp);
	mdelay(1);

	/*enable the pipe*/
	PSB_WVDC32(pipeconf_val, pipeconf_reg);

	/* restore palette (gamma) */
	/*DRM_UDELAY(50000); */
	for (i = 0; i < 256; i++)
		PSB_WVDC32(palette_val[i], palette_reg + (i << 2));

	return 0;
}

/*
 * mdfld_restore_cursor_overlay_registers
 *
 * Description: We are going to resume so restore cursor and overlay register state.
 */
static int mdfld_restore_cursor_overlay_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	/*Enable Cursor A*/
	PSB_WVDC32(dev_priv->saveDSPACURSOR_CTRL, CURACNTR);
	PSB_WVDC32(dev_priv->saveDSPACURSOR_POS, CURAPOS);
	PSB_WVDC32(dev_priv->saveDSPACURSOR_BASE, CURABASE);

	PSB_WVDC32(dev_priv->saveDSPBCURSOR_CTRL, CURBCNTR);
	PSB_WVDC32(dev_priv->saveDSPBCURSOR_POS, CURBPOS);
	PSB_WVDC32(dev_priv->saveDSPBCURSOR_BASE, CURBBASE);

	PSB_WVDC32(dev_priv->saveDSPCCURSOR_CTRL, CURCCNTR);
	PSB_WVDC32(dev_priv->saveDSPCCURSOR_POS, CURCPOS);
	PSB_WVDC32(dev_priv->saveDSPCCURSOR_BASE, CURCBASE);

	/* restore HW overlay */
	PSB_WVDC32(dev_priv->saveOV_OVADD, OV_OVADD);
	PSB_WVDC32(dev_priv->saveOV_OGAMC0, OV_OGAMC0);
	PSB_WVDC32(dev_priv->saveOV_OGAMC1, OV_OGAMC1);
	PSB_WVDC32(dev_priv->saveOV_OGAMC2, OV_OGAMC2);
	PSB_WVDC32(dev_priv->saveOV_OGAMC3, OV_OGAMC3);
	PSB_WVDC32(dev_priv->saveOV_OGAMC4, OV_OGAMC4);
	PSB_WVDC32(dev_priv->saveOV_OGAMC5, OV_OGAMC5);

	PSB_WVDC32(dev_priv->saveOV_OVADD_C, OV_OVADD + OV_C_OFFSET);
	PSB_WVDC32(dev_priv->saveOV_OGAMC0_C, OV_OGAMC0 + OV_C_OFFSET);
	PSB_WVDC32(dev_priv->saveOV_OGAMC1_C, OV_OGAMC1 + OV_C_OFFSET);
	PSB_WVDC32(dev_priv->saveOV_OGAMC2_C, OV_OGAMC2 + OV_C_OFFSET);
	PSB_WVDC32(dev_priv->saveOV_OGAMC3_C, OV_OGAMC3 + OV_C_OFFSET);
	PSB_WVDC32(dev_priv->saveOV_OGAMC4_C, OV_OGAMC4 + OV_C_OFFSET);
	PSB_WVDC32(dev_priv->saveOV_OGAMC5_C, OV_OGAMC5 + OV_C_OFFSET);

	return 0;
}

/*
 *  mdfld_save_display
 *
 * Description: Save display status before DPMS OFF for RuntimePM
 */
static void mdfld_save_display(struct drm_device *dev)
{
#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "ospm_save_display\n");
#endif
	mdfld_save_cursor_overlay_registers(dev);

	mdfld_save_display_registers(dev, 0);

	mdfld_save_display_registers(dev, 2);
}
/*
 * powermgmt_suspend_display
 *
 * Description: Suspend the display hardware saving state and disabling
 * as necessary.
 */
static void ospm_suspend_display(struct drm_device *dev)
{
	//to put panel into ULPS mode.
	u32 temp = 0;
	u32 device_ready_reg = DEVICE_READY_REG;
	u32 mipi_reg = MIPI;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "%s \n", __func__);
#endif
	if (!(g_hw_power_status_mask & OSPM_DISPLAY_ISLAND))
		return;

	mdfld_save_cursor_overlay_registers(dev);

	mdfld_save_display_registers(dev, 0);
	mdfld_save_display_registers(dev, 2);
	mdfld_save_display_registers(dev, 1);

	mdfld_disable_crtc(dev, 0);
	mdfld_disable_crtc(dev, 2);
	mdfld_disable_crtc(dev, 1);

	/* Put the panel in ULPS mode for S0ix. */
	temp = REG_READ(device_ready_reg);
	temp &= ~ULPS_MASK;
	temp |= ENTERING_ULPS;
	REG_WRITE(device_ready_reg, temp);

	//LP Hold
	temp = REG_READ(mipi_reg);
	temp &= ~LP_OUTPUT_HOLD;
	REG_WRITE(mipi_reg, temp);
	mdelay(1);

	ospm_power_island_down(OSPM_DISPLAY_ISLAND);
}

/*
 * ospm_resume_display
 *
 * Description: Resume the display hardware restoring state and enabling
 * as necessary.
 */
static void ospm_resume_display(struct drm_device *drm_dev)
{
	struct drm_psb_private *dev_priv = drm_dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "%s \n", __func__);
#endif
	if (g_hw_power_status_mask & OSPM_DISPLAY_ISLAND)
		return;

	/* turn on the display power island */
	ospm_power_island_up(OSPM_DISPLAY_ISLAND);

	PSB_WVDC32(pg->pge_ctl | _PSB_PGETBL_ENABLED, PSB_PGETBL_CTL);
	pci_write_config_word(drm_dev->pdev, PSB_GMCH_CTRL,
			      pg->gmch_ctrl | _PSB_GMCH_ENABLED);

	/* Don't reinitialize the GTT as it is unnecessary.  The gtt is
	 * stored in memory so it will automatically be restored.  All
	 * we need to do is restore the PGETBL_CTL which we already do
	 * above.
	 */
	/*psb_gtt_init(dev_priv->pg, 1);*/

	mdfld_restore_display_registers(drm_dev, 1);
	mdfld_restore_display_registers(drm_dev, 0);
	mdfld_restore_display_registers(drm_dev, 2);
	mdfld_restore_cursor_overlay_registers(drm_dev);
}

#if 1
/*
 * ospm_suspend_pci
 *
 * Description: Suspend the pci device saving state and disabling
 * as necessary.
 */
static void ospm_suspend_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;
	int bsm, vbt;

	if (gbSuspended)
		return;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "ospm_suspend_pci\n");
#endif

#ifdef CONFIG_MDFD_GL3
	/* Power off GL3 after all GFX sub-systems are powered off. */
	gl3_invalidate();
	ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);
#endif

	pci_save_state(pdev);
	pci_read_config_dword(pdev, 0x5C, &bsm);
	dev_priv->saveBSM = bsm;
	pci_read_config_dword(pdev, 0xFC, &vbt);
	dev_priv->saveVBT = vbt;
	pci_read_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, &dev_priv->msi_addr);
	pci_read_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, &dev_priv->msi_data);

	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	gbSuspended = true;
}

/*
 * ospm_resume_pci
 *
 * Description: Resume the pci device restoring state and enabling
 * as necessary.
 */
static bool ospm_resume_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;
	int ret = 0;

	if (!gbSuspended)
		return true;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "ospm_resume_pci\n");
#endif

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	pci_write_config_dword(pdev, 0x5c, dev_priv->saveBSM);
	pci_write_config_dword(pdev, 0xFC, dev_priv->saveVBT);
	/* retoring MSI address and data in PCIx space */
	pci_write_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, dev_priv->msi_addr);
	pci_write_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, dev_priv->msi_data);
	ret = pci_enable_device(pdev);

	if (ret != 0)
		printk(KERN_ALERT "ospm_resume_pci: pci_enable_device failed: %d\n", ret);
	else
		gbSuspended = false;

#ifdef CONFIG_MDFD_GL3
	if (!ret) {
		// Powerup GL3 - can be used by any GFX-sub-system.
		ospm_power_island_up(OSPM_GL3_CACHE_ISLAND);

	}
#endif

	return !gbSuspended;
}
#endif
/*
 * ospm_power_suspend
 *
 * Description: OSPM is telling our driver to suspend so save state
 * and power down all hardware.
 */
int ospm_power_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int ret = 0;
	int graphics_access_count;
	int videoenc_access_count;
	int videodec_access_count;
	int display_access_count;
	bool suspend_pci = true;

	if (gbSuspendInProgress || gbResumeInProgress) {
#ifdef OSPM_GFX_DPK
		printk(KERN_ALERT "OSPM_GFX_DPK: %s system BUSY \n", __func__);
#endif
		return  -EBUSY;
	}

	mutex_lock(&g_ospm_mutex);

	if (!gbSuspended) {
		graphics_access_count = atomic_read(&g_graphics_access_count);
		videoenc_access_count = atomic_read(&g_videoenc_access_count);
		videodec_access_count = atomic_read(&g_videodec_access_count);
		display_access_count = atomic_read(&g_display_access_count);

		if (graphics_access_count ||
		    videoenc_access_count ||
		    videodec_access_count ||
		    display_access_count)
			ret = -EBUSY;

		if (!ret) {
			gbSuspendInProgress = true;

			psb_irq_uninstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
			ospm_suspend_display(drm_dev);
#if 1
			/* FIXME: video driver support for Linux Runtime PM */
			if (ospm_runtime_pm_msvdx_suspend(drm_dev) != 0) {
				suspend_pci = false;
			}

			if (ospm_runtime_pm_topaz_suspend(drm_dev) != 0) {
				suspend_pci = false;
			}

#endif
			if (suspend_pci == true) {
				ospm_suspend_pci(pdev);
			}
			gbSuspendInProgress = false;
		} else {
			printk(KERN_ALERT "ospm_power_suspend: device busy: graphics %d videoenc %d videodec %d display %d\n", graphics_access_count, videoenc_access_count, videodec_access_count, display_access_count);
		}
	}


	mutex_unlock(&g_ospm_mutex);
	return ret;
}

/*
 * ospm_power_island_up
 *
 * Description: Restore power to the specified island(s) (powergating)
 */
void ospm_power_island_up(int hw_islands)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */
	struct drm_psb_private *dev_priv = drm_dev->dev_private;
	u32 pwr_cnt = 0;
	u32 pwr_sts = 0;
	u32 pwr_mask = 0;
	u32 cnt = 0;
	unsigned long flags;

	if (hw_islands & (OSPM_GRAPHICS_ISLAND | OSPM_VIDEO_ENC_ISLAND |
				OSPM_VIDEO_DEC_ISLAND | OSPM_GL3_CACHE_ISLAND |
				OSPM_ISP_ISLAND)) {
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		pwr_cnt = inl(dev_priv->apm_base + PSB_APM_CMD);

		pwr_mask = 0;


		if (hw_islands & OSPM_GRAPHICS_ISLAND) {
			pwr_cnt &= ~PSB_PWRGT_GFX_MASK;
			pwr_mask |= PSB_PWRGT_GFX_MASK;
#ifdef OSPM_STAT
			if (dev_priv->graphics_state == PSB_PWR_STATE_OFF) {
				dev_priv->gfx_off_time += (jiffies - dev_priv->gfx_last_mode_change) * 1000 / HZ;
				dev_priv->gfx_last_mode_change = jiffies;
				dev_priv->graphics_state = PSB_PWR_STATE_ON;
				dev_priv->gfx_on_cnt++;
			}
#endif
		}
		if (hw_islands & OSPM_GL3_CACHE_ISLAND) {
			pwr_cnt &= ~PSB_PWRGT_GL3_MASK;
			pwr_mask |= PSB_PWRGT_GL3_MASK;
		}
		if (hw_islands & OSPM_VIDEO_ENC_ISLAND) {
			pwr_cnt &= ~PSB_PWRGT_VID_ENC_MASK;
			pwr_mask |= PSB_PWRGT_VID_ENC_MASK;
		}
		if (hw_islands & OSPM_VIDEO_DEC_ISLAND) {
			pwr_cnt &= ~PSB_PWRGT_VID_DEC_MASK;
			pwr_mask |= PSB_PWRGT_VID_DEC_MASK;
		}
		if (hw_islands & OSPM_ISP_ISLAND) {
			pwr_cnt &= ~PSB_PWRGT_ISP_MASK;
			pwr_mask |= PSB_PWRGT_ISP_MASK;
		}

		outl(pwr_cnt, dev_priv->apm_base + PSB_APM_CMD);
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		while (true) {
			pwr_sts = inl(dev_priv->apm_base + PSB_APM_STS);
			if ((pwr_sts & pwr_mask) == 0)
				break;
			else
				udelay(10);
			cnt++;
			if (cnt > 1000) {
				printk(KERN_ALERT "%s: STUCK in INFINITE LOOP - failing on islands:0x%x\n", __func__, hw_islands);
				cnt = 0;
			}
		}
	}
	if (hw_islands & OSPM_DISPLAY_ISLAND) {

		pwr_mask = MDFLD_PWRGT_DISPLAY_CNTR;

		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		pwr_cnt = inl(dev_priv->ospm_base + PSB_PM_SSC);
		pwr_cnt &= ~pwr_mask;
		outl(pwr_cnt, (dev_priv->ospm_base + PSB_PM_SSC));
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		pwr_mask = MDFLD_PWRGT_DISPLAY_STS_B0;

		cnt = 0;
		while (true) {
			pwr_sts = inl(dev_priv->ospm_base + PSB_PM_SSS);
			if ((pwr_sts & pwr_mask) == 0)
				break;
			else
				udelay(10);
			cnt++;
			if (cnt > 1000) {
				printk(KERN_ALERT "%s: STUCK in INFINITE LOOP - failing on DISPLAY \n", __func__);
				cnt = 0;
			}
		}
	}

	g_hw_power_status_mask |= hw_islands;
}

/*
 * ospm_power_resume
 */
int ospm_power_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	if (gbSuspendInProgress || gbResumeInProgress) {
#ifdef OSPM_GFX_DPK
		printk(KERN_ALERT "OSPM_GFX_DPK: %s hw_island: Suspend || gbResumeInProgress!!!! \n", __func__);
#endif
		return 0;
	}

	mutex_lock(&g_ospm_mutex);

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "OSPM_GFX_DPK: ospm_power_resume \n");
#endif

	gbResumeInProgress = true;

	ospm_resume_pci(pdev);

	ospm_resume_display(drm_dev);
	psb_irq_preinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
	psb_irq_postinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);

	gbResumeInProgress = false;

	mutex_unlock(&g_ospm_mutex);

	return 0;
}


/*
 * ospm_power_island_down
 *
 * Description: Cut power to the specified island(s) (powergating)
 */
void ospm_power_island_down(int islands)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */
	struct drm_psb_private *dev_priv = drm_dev->dev_private;
	u32 pwr_cnt = 0;
	u32 pwr_mask = 0;
	u32 pwr_sts = 0;
	u32 cnt = 0;
	unsigned long flags;

	g_hw_power_status_mask &= ~islands;

	if (islands & OSPM_GRAPHICS_ISLAND) {
		pwr_cnt |= PSB_PWRGT_GFX_MASK;
		pwr_mask |= PSB_PWRGT_GFX_MASK;
#ifdef OSPM_STAT
		if (dev_priv->graphics_state == PSB_PWR_STATE_ON) {
			dev_priv->gfx_on_time += (jiffies - dev_priv->gfx_last_mode_change) * 1000 / HZ;
			dev_priv->gfx_last_mode_change = jiffies;
			dev_priv->graphics_state = PSB_PWR_STATE_OFF;
			dev_priv->gfx_off_cnt++;
		}
#endif
	}
	if (islands & OSPM_GL3_CACHE_ISLAND) {
		pwr_cnt |= PSB_PWRGT_GL3_MASK;
		pwr_mask |= PSB_PWRGT_GL3_MASK;
	}
	if (islands & OSPM_VIDEO_ENC_ISLAND) {
		pwr_cnt |= PSB_PWRGT_VID_ENC_MASK;
		pwr_mask |= PSB_PWRGT_VID_ENC_MASK;
	}
	if (islands & OSPM_VIDEO_DEC_ISLAND) {
		pwr_cnt |= PSB_PWRGT_VID_DEC_MASK;
		pwr_mask |= PSB_PWRGT_VID_DEC_MASK;
	}
	if (islands & OSPM_ISP_ISLAND) {
		pwr_cnt |= PSB_PWRGT_ISP_MASK;
		pwr_mask |= PSB_PWRGT_ISP_MASK;
	}
	if (pwr_cnt) {
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		pwr_cnt |= inl(dev_priv->apm_base);
		outl(pwr_cnt, dev_priv->apm_base  + PSB_APM_CMD);
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		while (true) {
			pwr_sts = inl(dev_priv->apm_base + PSB_APM_STS);
			if ((pwr_sts & pwr_mask) == pwr_mask)
				break;
			else
				udelay(10);
			cnt++;
			if (cnt > 1000) {
				printk(KERN_ALERT "%s: STUCK in INFINITE LOOP - failing on islands:0x%x\n", __func__, islands);
				cnt = 0;
			}
		}
	}
	if (islands & OSPM_DISPLAY_ISLAND) {

		pwr_mask = MDFLD_PWRGT_DISPLAY_CNTR;

		outl(pwr_mask, (dev_priv->ospm_base + PSB_PM_SSC));

		pwr_mask = MDFLD_PWRGT_DISPLAY_STS_B0;

		cnt = 0;
		while (true) {
			pwr_sts = inl(dev_priv->ospm_base + PSB_PM_SSS);
			if ((pwr_sts & pwr_mask) == pwr_mask)
				break;
			else
				udelay(10);
			cnt++;
			if (cnt > 1000) {
				printk(KERN_ALERT "%s: STUCK in INFINITE LOOP - failing on Display island\n", __func__);
				cnt = 0;
			}
		}
	}
}


/*
 * ospm_power_is_hw_on
 *
 * Description: do an instantaneous check for if the specified islands
 * are on.  Only use this in cases where you know the g_state_change_mutex
 * is already held such as in irq install/uninstall.  Otherwise, use
 * ospm_power_using_hw_begin().
 */
bool ospm_power_is_hw_on(int hw_islands)
{
	return ((g_hw_power_status_mask & hw_islands) == hw_islands) ? true : false;
}

/*
 * ospm_power_using_hw_begin
 *
 * Description: Notify PowerMgmt module that you will be accessing the
 * specified island's hw so don't power it off.  If force_on is true,
 * this will power on the specified island if it is off.
 * Otherwise, this will return false and the caller is expected to not
 * access the hw.
 *
 * NOTE *** If this is called from and interrupt handler or other atomic
 * context, then it will return false if we are in the middle of a
 * power state transition and the caller will be expected to handle that
 * even if force_on is set to true.
 */
bool ospm_power_using_hw_begin(int hw_island, UHBUsage usage)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */
	bool ret = true;
	bool island_is_off = false;
	bool b_atomic = (in_interrupt() || in_atomic());
	bool locked = true;
	IMG_UINT32 deviceID = 0;
	bool force_on = usage ? true : false;

#ifdef CONFIG_PM_RUNTIME
	/* increment pm_runtime_refcount */
	pm_runtime_get(&drm_dev->pdev->dev);
#endif

	/*quick path, not 100% race safe, but should be enough comapre to current other code in this file */
	if (!force_on) {
		if (hw_island & (OSPM_ALL_ISLANDS & ~g_hw_power_status_mask)) {
#ifdef CONFIG_PM_RUNTIME
			/* decrement pm_runtime_refcount */
			pm_runtime_put(&drm_dev->pdev->dev);
#endif
			return false;
		} else {
			locked = false;
			goto increase_count;
		}
	}

	if (!b_atomic)
		mutex_lock(&g_ospm_mutex);

	island_is_off = hw_island & (OSPM_ALL_ISLANDS & ~g_hw_power_status_mask);

	if (b_atomic && (gbSuspendInProgress || gbResumeInProgress || gbSuspended) && force_on && island_is_off)
		ret = false;

	if (ret && island_is_off && !force_on)
		ret = false;

	if (ret && island_is_off && force_on) {
		gbResumeInProgress = true;

		ret = ospm_resume_pci(drm_dev->pdev);

		if (ret) {
			switch (hw_island) {
			case OSPM_DISPLAY_ISLAND:
				deviceID = gui32MRSTDisplayDeviceID;
				ospm_resume_display(drm_dev);
				psb_irq_preinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
				psb_irq_postinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
				break;
			case OSPM_GRAPHICS_ISLAND:
				deviceID = gui32SGXDeviceID;
				ospm_power_island_up(OSPM_GRAPHICS_ISLAND);
				psb_irq_preinstall_islands(drm_dev, OSPM_GRAPHICS_ISLAND);
				psb_irq_postinstall_islands(drm_dev, OSPM_GRAPHICS_ISLAND);
				break;
#if 1
			case OSPM_VIDEO_DEC_ISLAND:
				if (!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
					//printk(KERN_ALERT "%s power on display for video decode use\n", __func__);
					deviceID = gui32MRSTDisplayDeviceID;
					ospm_resume_display(drm_dev);
					psb_irq_preinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
					psb_irq_postinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
				} else {
					//printk(KERN_ALERT "%s display is already on for video decode use\n", __func__);
				}

				if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
					//printk(KERN_ALERT "%s power on video decode\n", __func__);
					deviceID = gui32MRSTMSVDXDeviceID;
					ospm_power_island_up(OSPM_VIDEO_DEC_ISLAND);
					ospm_runtime_pm_msvdx_resume(drm_dev);
					psb_irq_preinstall_islands(drm_dev, OSPM_VIDEO_DEC_ISLAND);
					psb_irq_postinstall_islands(drm_dev, OSPM_VIDEO_DEC_ISLAND);
				} else {
					//printk(KERN_ALERT "%s video decode is already on\n", __func__);
				}

				break;
			case OSPM_VIDEO_ENC_ISLAND:
				if (!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
					//printk(KERN_ALERT "%s power on display for video encode\n", __func__);
					deviceID = gui32MRSTDisplayDeviceID;
					ospm_resume_display(drm_dev);
					psb_irq_preinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
					psb_irq_postinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
				} else {
					//printk(KERN_ALERT "%s display is already on for video encode use\n", __func__);
				}

				if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
					//printk(KERN_ALERT "%s power on video encode\n", __func__);
					deviceID = gui32MRSTTOPAZDeviceID;
					ospm_power_island_up(OSPM_VIDEO_ENC_ISLAND);
					ospm_runtime_pm_topaz_resume(drm_dev);
					psb_irq_preinstall_islands(drm_dev, OSPM_VIDEO_ENC_ISLAND);
					psb_irq_postinstall_islands(drm_dev, OSPM_VIDEO_ENC_ISLAND);
				} else {
					//printk(KERN_ALERT "%s video decode is already on\n", __func__);
				}
#endif
				break;

			default:
				printk(KERN_ALERT "%s unknown island !!!! \n", __func__);
				break;
			}

		}

		if (!ret)
			printk(KERN_ALERT "ospm_power_using_hw_begin: forcing on %d failed\n", hw_island);

		gbResumeInProgress = false;
	}
increase_count:
	if (ret) {
		switch (hw_island) {
		case OSPM_GRAPHICS_ISLAND:
			atomic_inc(&g_graphics_access_count);
			break;
		case OSPM_VIDEO_ENC_ISLAND:
			atomic_inc(&g_videoenc_access_count);
			break;
		case OSPM_VIDEO_DEC_ISLAND:
			atomic_inc(&g_videodec_access_count);
			break;
		case OSPM_DISPLAY_ISLAND:
			atomic_inc(&g_display_access_count);
			break;
		}
	} else {
#ifdef CONFIG_PM_RUNTIME
		/* decrement pm_runtime_refcount */
		pm_runtime_put(&drm_dev->pdev->dev);
#endif
	}

	if (!b_atomic && locked)
		mutex_unlock(&g_ospm_mutex);

	return ret;
}


/*
 * ospm_power_using_hw_end
 *
 * Description: Notify PowerMgmt module that you are done accessing the
 * specified island's hw so feel free to power it off.  Note that this
 * function doesn't actually power off the islands.
 */
void ospm_power_using_hw_end(int hw_island)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */

	switch (hw_island) {
	case OSPM_GRAPHICS_ISLAND:
		atomic_dec(&g_graphics_access_count);
		break;
	case OSPM_VIDEO_ENC_ISLAND:
		atomic_dec(&g_videoenc_access_count);
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		atomic_dec(&g_videodec_access_count);
		break;
	case OSPM_DISPLAY_ISLAND:
		atomic_dec(&g_display_access_count);
		break;
	}

	//decrement runtime pm ref count
	pm_runtime_put(&drm_dev->pdev->dev);

	WARN_ON(atomic_read(&g_graphics_access_count) < 0);
	WARN_ON(atomic_read(&g_videoenc_access_count) < 0);
	WARN_ON(atomic_read(&g_videodec_access_count) < 0);
	WARN_ON(atomic_read(&g_display_access_count) < 0);
}

int psb_runtime_suspend(struct device *dev)
{
	int ret = 0;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "OSPM_GFX_DPK: %s \n", __func__);
#endif
        if (atomic_read(&g_graphics_access_count) || atomic_read(&g_videoenc_access_count)
		|| (gbdispstatus == true)
		|| atomic_read(&g_videodec_access_count) || atomic_read(&g_display_access_count)){
#ifdef OSPM_GFX_DPK
		printk(KERN_ALERT "OSPM_GFX_DPK: GFX: %d VEC: %d VED: %d DC: %d DSR: (N/A) \n", atomic_read(&g_graphics_access_count),
			atomic_read(&g_videoenc_access_count), atomic_read(&g_videodec_access_count), atomic_read(&g_display_access_count));
#endif
                return -EBUSY;
        }
        else
		ret = ospm_power_suspend(dev);

	return ret;
}

int psb_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	//Notify HDMI Audio sub-system about the resume.
#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
	struct drm_psb_private *dev_priv = drm_dev->dev_private;

	if (dev_priv->had_pvt_data)
		dev_priv->had_interface->resume(dev_priv->had_pvt_data);
#endif
	/*Nop for GFX*/
	return 0;
}

int psb_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
	struct drm_psb_private *dev_priv = drm_dev->dev_private;
	int hdmi_audio_busy = 0;
	pm_event_t hdmi_audio_event;
#endif

#if 1
	int msvdx_hw_busy = 0;
	int topaz_hw_busy = 0;

	msvdx_hw_busy = ospm_runtime_check_msvdx_hw_busy(drm_dev);
	topaz_hw_busy = ospm_runtime_check_topaz_hw_busy(drm_dev);
#endif

#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
       if(dev_priv->had_pvt_data){
               hdmi_audio_event.event = 0;
               hdmi_audio_busy = dev_priv->had_interface->suspend(dev_priv->had_pvt_data, hdmi_audio_event);
       }
#endif
	/*printk (KERN_ALERT "lvds:%d,mipi:%d\n", dev_priv->is_lvds_on, dev_priv->is_mipi_on);*/
	if (atomic_read(&g_graphics_access_count) || atomic_read(&g_videoenc_access_count)
		|| atomic_read(&g_videodec_access_count) || atomic_read(&g_display_access_count)
		|| (gbdispstatus == true)
#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
		|| hdmi_audio_busy
#endif

#if 1
		|| (msvdx_hw_busy == 1)
		|| (topaz_hw_busy == 1))
#else
		)
#endif
		return 1;
		else
			return 0;
}

