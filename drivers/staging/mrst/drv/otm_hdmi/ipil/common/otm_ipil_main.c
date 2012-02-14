/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include "ipil_internal.h"

#include "otm_hdmi.h"
#include "otm_hdmi_types.h"

#include "ips_hdmi.h"
#include "ipil_hdmi.h"

static hdmi_device_t *hdmi_dev;

otm_hdmi_ret_t ipil_hdmi_set_hdmi_dev(hdmi_device_t *dev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	if (!dev)
		rc = OTM_HDMI_ERR_NULL_ARG;
	else
		hdmi_dev = dev;
	return rc;
}

uint32_t hdmi_read32(uint32_t reg)
{
	if (hdmi_dev)
		return readl((const void *)(hdmi_dev->io_address + reg));

	return 0;
}

void hdmi_write32(uint32_t reg, uint32_t val)
{
	if (hdmi_dev)
		writel(val, (void *)(hdmi_dev->io_address + reg));
}

otm_hdmi_ret_t ipil_hdmi_decide_I2C_HW(hdmi_context_t *ctx)
{
	return ips_hdmi_decide_I2C_HW(ctx);
}

otm_hdmi_ret_t ipil_hdmi_general_5V_enable(hdmi_device_t *dev)
{
	return ips_hdmi_general_5V_enable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_5V_disable(hdmi_device_t *dev)
{
	return ips_hdmi_general_5V_disable(dev);
}

otm_hdmi_ret_t ipil_hdmi_set_program_clocks(hdmi_context_t *ctx,
					    unsigned int dclk)
{
	return ips_hdmi_set_program_clocks(ctx, dclk);
}

otm_hdmi_ret_t ipil_hdmi_audio_init(hdmi_context_t *ctx)
{
	return ips_hdmi_audio_init(ctx);
}

otm_hdmi_ret_t ipil_hdmi_audio_deinit(hdmi_context_t *ctx)
{
	return ips_hdmi_audio_deinit(ctx);
}

otm_hdmi_ret_t ipil_hdmi_general_unit_enable(hdmi_device_t *dev)
{
	return ips_hdmi_general_unit_enable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_unit_disable(hdmi_device_t *dev)
{
	return ips_hdmi_general_unit_disable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_hdcp_clock_enable(hdmi_device_t *dev)
{
	return ips_hdmi_general_hdcp_clock_enable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_hdcp_clock_disable(hdmi_device_t *dev)
{
	return ips_hdmi_general_hdcp_clock_disable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_audio_clock_enable(hdmi_device_t *dev)
{
	return ips_hdmi_general_audio_clock_enable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_audio_clock_disable(hdmi_device_t *dev)
{
	return ips_hdmi_general_audio_clock_disable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_pixel_clock_enable(hdmi_device_t *dev)
{
	return ips_hdmi_general_pixel_clock_enable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_pixel_clock_disable(hdmi_device_t *dev)
{
	return ips_hdmi_general_pixel_clock_disable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_tdms_clock_enable(hdmi_device_t *dev)
{
	return ips_hdmi_general_tdms_clock_enable(dev);
}

otm_hdmi_ret_t ipil_hdmi_general_tdms_clock_disable(hdmi_device_t *dev)
{
	return ips_hdmi_general_tdms_clock_disable(dev);
}

otm_hdmi_ret_t ipil_hdmi_i2c_disable(hdmi_device_t *dev)
{
	return ips_hdmi_i2c_disable(dev);
}

bool ipil_hdmi_power_rails_on(void)
{
	return ips_hdmi_power_rails_on();
}

/*
 * Description: hdmi interrupt handler (upper half).
 *		handles the interrupts by reading hdmi status register
 *		and waking up bottom half if needed.
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	IRQ_HANDLED on NULL input arguments, and if the
 *			interrupt is not HDMI HPD interrupts.
 *		IRQ_WAKE_THREAD if this is a HDMI HPD interrupt.
 */
irqreturn_t ipil_hdmi_irq_handler(hdmi_device_t *dev)
{
	/* NULL checks */
	if (dev == NULL) {
		pr_debug("\ninvalid argument\n");
		return IRQ_HANDLED;
	}

	return ips_hdmi_irq_handler((void *)dev->irq_io_address);
}

/*
 * Description: enable infoframes
 *
 * @dev:        hdmi_device_t
 * @type:	type of infoframe packet
 * @pkt:	infoframe packet data
 * @freq:	number of times packet needs to be sent
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_ERR_INVAL on invalid packet type
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ipil_hdmi_enable_infoframe(hdmi_device_t *dev,
		unsigned int type, otm_hdmi_packet_t *pkt, unsigned int freq)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	if (!dev || !pkt)
		return OTM_HDMI_ERR_NULL_ARG;

	switch (type) {
	case HDMI_PACKET_AVI:
	case HDMI_PACKET_VS:
	case HDMI_PACKET_SPD:
		rc = ips_hdmi_enable_vid_infoframe(dev, type, pkt, freq);
		break;
	default:/* TODO: Revisit for Other Infoframes */
		rc = OTM_HDMI_ERR_INVAL;
		break;
	}

	return rc;
}

/*
 * Description: disable particular infoframe
 *
 * @dev:        hdmi_device_t
 * @type:	type of infoframe packet
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_ERR_INVAL on invalid packet type
 *		OTM_HDMI_SUCCESS on success
*/
otm_hdmi_ret_t ipil_hdmi_disable_infoframe(hdmi_device_t *dev,
					unsigned int type)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	if (!dev)
		return OTM_HDMI_ERR_NULL_ARG;

	switch (type) {
	case HDMI_PACKET_AVI:
	case HDMI_PACKET_VS:
	case HDMI_PACKET_SPD:
		rc = ips_hdmi_disable_vid_infoframe(dev, type);
		break;
	default:/* TODO: Revisit for Other Infoframes */
		rc = OTM_HDMI_ERR_INVAL;
		break;
	}

	return rc;
}

/*
 * Description: disable all infoframes
 *
 * @dev:        hdmi_device_t
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_SUCCESS on success
*/
otm_hdmi_ret_t ipil_hdmi_disable_all_infoframes(hdmi_device_t *dev)
{
	if (!dev)
		return OTM_HDMI_ERR_NULL_ARG;

	return ips_hdmi_disable_all_infoframes(dev);
}

/*
 * Description: programs hdmi pipe src and size of the input.
 *
 * @dev:		hdmi_device_t
 * @scalingtype:	scaling type (FULL_SCREEN, CENTER, NO_SCALE etc.)
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @fb_width, fb_height:allocated frame buffer dimensions
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_program_dspregs(hdmi_device_t *dev,
					int scalingtype,
					ipil_timings_t *mode,
					ipil_timings_t *adjusted_mode,
					int fb_width, int fb_height)
{
	int sprite_pos_x = 0, sprite_pos_y = 0;
	int sprite_width = 0, sprite_height = 0;
	int src_image_hor = 0, src_image_vert = 0;

	/* NULL checks */
	if (dev == NULL || mode == NULL || adjusted_mode == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/*
	 * TODO: update these values based on scaling type,
	 * rotation and HDMI mode.
	 */
	sprite_width = fb_width;
	sprite_height = fb_height;
	src_image_hor = fb_width;
	src_image_vert = fb_height;
	sprite_pos_x = 0;
	sprite_pos_y = 0;

	/*
	 * pipesrc and dspsize control the size that is scaled from,
	 * which should always be the user's requested size.
	 */
	switch (scalingtype) {
	case IPIL_TIMING_SCALE_NONE:
	case IPIL_TIMING_SCALE_CENTER:
		/* TODO: implement this */
		break;

	case IPIL_TIMING_SCALE_FULLSCREEN:
		if ((adjusted_mode->width != sprite_width) ||
			(adjusted_mode->height != sprite_height))
			hdmi_write32(IPIL_PFIT_CONTROL,
				IPIL_PFIT_ENABLE |
				IPIL_PFIT_PIPE_SELECT_B |
				IPIL_PFIT_SCALING_AUTO);

		break;

	case IPIL_TIMING_SCALE_ASPECT:
		if ((adjusted_mode->width != fb_width) ||
			(adjusted_mode->height != fb_height)) {
			if ((adjusted_mode->width * fb_height) ==
			    (fb_width * adjusted_mode->height))
				hdmi_write32(IPIL_PFIT_CONTROL,
					IPIL_PFIT_ENABLE |
					IPIL_PFIT_PIPE_SELECT_B);
			else if ((adjusted_mode->width *
				fb_height) > (fb_width *
				adjusted_mode->height))
				hdmi_write32(IPIL_PFIT_CONTROL,
					IPIL_PFIT_ENABLE |
					IPIL_PFIT_PIPE_SELECT_B |
					IPIL_PFIT_SCALING_PILLARBOX);
			else
				hdmi_write32(IPIL_PFIT_CONTROL,
					IPIL_PFIT_ENABLE |
					IPIL_PFIT_PIPE_SELECT_B |
					IPIL_PFIT_SCALING_LETTERBOX);
		}
		break;

	default:
		if ((adjusted_mode->width != fb_width) ||
			(adjusted_mode->height != fb_height))
			hdmi_write32(IPIL_PFIT_CONTROL,
					IPIL_PFIT_ENABLE |
					IPIL_PFIT_PIPE_SELECT_B);

		break;
	}

	hdmi_write32(IPIL_DSPBSIZE, ((sprite_height - 1) << 16) |
				 (sprite_width - 1));

	hdmi_write32(IPIL_PIPEBSRC, ((src_image_hor - 1) << 16) |
				(src_image_vert - 1));

	hdmi_write32(IPIL_DSPBPOS, (sprite_pos_y << 16) | sprite_pos_x);

	return OTM_HDMI_SUCCESS;
}

/*
 * Description: this is pre-modeset configuration. This can be
 *		resetting HDMI unit, disabling/enabling dpll etc
 *		on the need basis.
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_prepare(hdmi_device_t *dev)
{
	/* NULL checks */
	if (dev == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* Nothing needed as of now for medfield */

	return OTM_HDMI_SUCCESS;
}

/*
 * Description: programs all the timing registers based on scaling type.
 *
 * @dev:		hdmi_device_t
 * @scalingtype:	scaling type (FULL_SCREEN, CENTER, NO_SCALE etc.)
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_program_timings(hdmi_device_t *dev,
					int scalingtype,
					otm_hdmi_timing_t *mode,
					otm_hdmi_timing_t *adjusted_mode)
{
	/* NULL checks */
	if (dev == NULL || mode == NULL || adjusted_mode == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* TODO: get adjusted htotal */
	/* ips_hdmi_get_htotal(); */

	if (scalingtype == IPIL_TIMING_SCALE_NONE) {
		/* Moorestown doesn't have register support for centering so we
		 * need to  mess with the h/vblank and h/vsync start and ends
		 * to get centering
		 */
		int offsetX = 0, offsetY = 0;

		offsetX = (adjusted_mode->width - mode->width) / 2;
		offsetY = (adjusted_mode->height - mode->height) / 2;

		hdmi_write32(IPIL_HTOTAL_B, (mode->width - 1) |
				((adjusted_mode->htotal - 1) << 16));

		hdmi_write32(IPIL_VTOTAL_B, (mode->height - 1) |
				((adjusted_mode->vtotal - 1) << 16));

		hdmi_write32(IPIL_HBLANK_B,
			(adjusted_mode->hblank_start - offsetX - 1) |
			((adjusted_mode->hblank_end - offsetX - 1) << 16));

		hdmi_write32(IPIL_HSYNC_B,
			(adjusted_mode->hsync_start - offsetX - 1) |
			((adjusted_mode->hsync_end - offsetX - 1) << 16));

		hdmi_write32(IPIL_VBLANK_B,
			(adjusted_mode->vblank_start - offsetY - 1) |
			((adjusted_mode->vblank_end - offsetY - 1) << 16));

		hdmi_write32(IPIL_VSYNC_B,
			(adjusted_mode->vsync_start - offsetY - 1) |
			((adjusted_mode->vsync_end - offsetY - 1) << 16));
	} else {
		hdmi_write32(IPIL_HTOTAL_B,
				(adjusted_mode->width - 1) |
				((adjusted_mode->htotal - 1) << 16));

		hdmi_write32(IPIL_VTOTAL_B,
				(adjusted_mode->height - 1) |
				((adjusted_mode->vtotal - 1) << 16));

		hdmi_write32(IPIL_HBLANK_B,
				(adjusted_mode->hblank_start - 1) |
				((adjusted_mode->hblank_end - 1) << 16));

		hdmi_write32(IPIL_HSYNC_B,
				(adjusted_mode->hsync_start - 1) |
				((adjusted_mode->hsync_end - 1) << 16));

		hdmi_write32(IPIL_VBLANK_B,
				(adjusted_mode->vblank_start - 1) |
				((adjusted_mode->vblank_end - 1) << 16));

		hdmi_write32(IPIL_VSYNC_B,
				(adjusted_mode->vsync_start - 1) |
				((adjusted_mode->vsync_end - 1) << 16));
	}

	return OTM_HDMI_SUCCESS;
}

/*
 * Description: programs dpll clocks, enables dpll and waits
 *		till it locks with DSI PLL
 *
 * @dev:	hdmi_device_t
 * @dclk:	refresh rate dot clock in kHz of current mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t	ipil_hdmi_crtc_mode_set_program_dpll(hdmi_device_t *dev,
							unsigned long dclk)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	u32 dpll_adj, fp;
	u32 dpll;
	int timeout = 0;

	/* NULL checks */
	if (dev == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* get the adjusted clock value */
	rc = ips_hdmi_get_adjusted_clk(dclk, &dpll_adj, &fp, &dev->clock_khz);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to calculate adjusted clock\n");
		return rc;
	}

	dpll = hdmi_read32(IPIL_DPLL_B);
	if (dpll & IPIL_DPLL_VCO_ENABLE) {
		dpll &= ~IPIL_DPLL_VCO_ENABLE;
		hdmi_write32(IPIL_DPLL_B, dpll);
		hdmi_read32(IPIL_DPLL_B);

		/* reset M1, N1 & P1 */
		hdmi_write32(IPIL_DPLL_DIV0, 0);
		dpll &= ~IPIL_P1_MASK;
		hdmi_write32(IPIL_DPLL_B, dpll);
	}

	/*
	 * When ungating power of DPLL, needs to wait 0.5us
	 * before enable the VCO
	 */
	if (dpll & IPIL_PWR_GATE_EN) {
		dpll &= ~IPIL_PWR_GATE_EN;
		hdmi_write32(IPIL_DPLL_B, dpll);
		udelay(1);
	}

	dpll = dpll_adj;
	hdmi_write32(IPIL_DPLL_DIV0, fp);
	hdmi_write32(IPIL_DPLL_B, dpll);
	udelay(1);

	dpll |= IPIL_DPLL_VCO_ENABLE;
	hdmi_write32(IPIL_DPLL_B, dpll);
	hdmi_read32(IPIL_DPLL_B);

	/* wait for DSI PLL to lock */
	while ((timeout < 20000) && !(hdmi_read32(IPIL_PIPEBCONF) &
					IPIL_PIPECONF_PLL_LOCK)) {
		udelay(150);
		timeout++;
	}

	return OTM_HDMI_SUCCESS;
}

/*
 * Description: configures the display plane register and enables
 *		pipeconf.
 *
 * @dev: hdmi_device_t
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_program_pipeconf(hdmi_device_t *dev)
{
	u32 dspcntr;
	u32 pipeconf;

	/* NULL checks */
	if (dev == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* Set up the display plane register */
	dspcntr = hdmi_read32(IPIL_DSPBCNTR);
	dspcntr |= 1 << IPIL_DSP_PLANE_PIPE_POS;
	dspcntr |= IPIL_DSP_PLANE_ENABLE;

	/* setup pipeconf */
	pipeconf = IPIL_PIPEACONF_ENABLE;


	hdmi_write32(IPIL_PIPEBCONF, pipeconf);
	hdmi_read32(IPIL_PIPEBCONF);

	hdmi_write32(IPIL_DSPBCNTR, dspcntr);

	return OTM_HDMI_SUCCESS;
}

/*
 * Description: encoder mode set function for hdmi. enables phy.
 *		set correct polarity for the current mode, sets
 *		correct panel fitting.
 *
 *
 * @dev:		hdmi_device_t
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @is_monitor_hdmi:	is monitor type is hdmi or not
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_enc_mode_set(hdmi_device_t *dev,
					otm_hdmi_timing_t *mode,
					otm_hdmi_timing_t *adjusted_mode,
					bool is_monitor_hdmi)
{
	u32 hdmib, hdmi_phy_misc;
	bool phsync;
	bool pvsync;

	/* NULL checks */
	if (dev == NULL || mode == NULL || adjusted_mode == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	if (is_monitor_hdmi) {
		hdmib = hdmi_read32(IPIL_HDMIB_CONTROL) | IPIL_HDMIB_PORT_EN
						| IPIL_HDMIB_PIPE_B_SELECT
						| IPIL_HDMIB_NULL_PACKET
						| IPIL_HDMIB_AUDIO_ENABLE;
	} else {
		hdmib = hdmi_read32(IPIL_HDMIB_CONTROL) | IPIL_HDMIB_PORT_EN
						| IPIL_HDMIB_PIPE_B_SELECT;
		hdmib &= ~IPIL_HDMIB_NULL_PACKET;
		hdmib &= ~IPIL_HDMIB_AUDIO_ENABLE;
	}

	/* set output polarity */
	phsync = adjusted_mode->mode_info_flags & IPIL_TIMING_FLAG_PHSYNC;
	pvsync = adjusted_mode->mode_info_flags & IPIL_TIMING_FLAG_PVSYNC;
	pr_debug("enc_mode_set %dx%d (%c,%c)\n", adjusted_mode->width,
						adjusted_mode->height,
						phsync ? '+' : '-',
						pvsync ? '+' : '-');
	/* TODO: define macros for hard coded values */
	hdmib &= ~0x18; /* clean bit 3 and 4 */
	hdmib |= phsync ? 0x8  : 0x0; /* bit 3 */
	hdmib |= pvsync ? 0x10 : 0x0; /* bit 4 */

	hdmi_phy_misc = hdmi_read32(IPIL_HDMIPHYMISCCTL) &
					~IPIL_HDMI_PHY_POWER_DOWN;

	hdmi_write32(IPIL_HDMIPHYMISCCTL, hdmi_phy_misc);
	hdmi_write32(IPIL_HDMIB_CONTROL, hdmib);
	hdmi_read32(IPIL_HDMIB_CONTROL);

	return OTM_HDMI_SUCCESS;
}

/*
 * Description: save HDMI display registers
 *
 * @dev:		hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_save_display_registers(hdmi_device_t *dev)
{
	if (NULL != dev)
		ips_hdmi_save_display_registers(dev);
}

/*
 * Description: save HDMI data island packets
 *
 * @dev:		hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_save_data_island(hdmi_device_t *dev)
{
	if (NULL != dev)
		ips_hdmi_save_data_island(dev);
}

/*
 * Description: destroys any saved HDMI data
 *
 * @dev:	hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_destroy_saved_data(hdmi_device_t *dev)
{
	if (NULL != dev)
		ips_hdmi_destroy_saved_data(dev);
}

/*
 * Description: disable HDMI display
 *
 * @dev:		hdmi_device_t
 *
 * Returns: none
 */
void ipil_disable_hdmi(hdmi_device_t *dev)
{
	if (NULL != dev)
		ips_disable_hdmi(dev);
}

/*
 * Description: restore HDMI display registers and enable display
 *
 * @dev:		hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_restore_and_enable_display(hdmi_device_t *dev)
{
	if (NULL != dev)
		ips_hdmi_restore_and_enable_display(dev);
}

/*
 * Description: restore HDMI data island packets
 *
 * @dev:		hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_restore_data_island(hdmi_device_t *dev)
{
	if (NULL != dev)
		ips_hdmi_restore_data_island(dev);
}
