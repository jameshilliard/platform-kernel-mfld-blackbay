/*
 *  psb backlight using HAL
 *
 * Copyright (c) 2009, Intel Corporation.
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
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors: Eric Knopp
 *
 */

#include <linux/backlight.h>
#include <linux/version.h>
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_drv.h"
#include "psb_intel_bios.h"
#include "psb_powermgmt.h"
#include "mdfld_dsi_dbi.h"

#define MRST_BLC_MAX_PWM_REG_FREQ	    0xFFFF
#define BLC_PWM_PRECISION_FACTOR 100	/* 10000000 */
#define BLC_PWM_FREQ_CALC_CONSTANT 32
#define MHz 1000000
#define BRIGHTNESS_MIN_LEVEL 1
#define BRIGHTNESS_MAX_LEVEL 100
#define BRIGHTNESS_MASK	0xFF
#define BLC_POLARITY_NORMAL 0
#define BLC_POLARITY_INVERSE 1
#define BLC_ADJUSTMENT_MAX 100

#define PSB_BLC_PWM_PRECISION_FACTOR    10
#define PSB_BLC_MAX_PWM_REG_FREQ        0xFFFE
#define PSB_BLC_MIN_PWM_REG_FREQ        0x2

#define PSB_BACKLIGHT_PWM_POLARITY_BIT_CLEAR (0xFFFE)
#define PSB_BACKLIGHT_PWM_CTL_SHIFT	(16)

int psb_brightness;
static struct backlight_device *psb_backlight_device;
static u8 blc_brightnesscmd;
u8 blc_pol;
u8 blc_type;

int lastFailedBrightness = -1;

int psb_set_brightness(struct backlight_device *bd)
{
	struct drm_device *dev = (struct drm_device *)bl_get_data(psb_backlight_device);
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	int level;
	u32 blc_pwm_ctl;
	u32 max_pwm_blc;

	if(bd != NULL)
		level = bd->props.brightness;
	else
		level = lastFailedBrightness;
    DRM_DEBUG_DRIVER("backlight level set to %d\n", level);
	PSB_DEBUG_ENTRY( "[DISPLAY] %s: level is %d\n", __func__, level);  //DIV5-MM-DISPLAY-NC-LCM_INIT-00

	/* Perform value bounds checking */
	if (level < BRIGHTNESS_MIN_LEVEL)
		level = BRIGHTNESS_MIN_LEVEL;

	if(!gbdispstatus){
		PSB_DEBUG_ENTRY( "[DISPLAY]: already OFF ignoring brighness request \n");
		//! there may exist concurrent racing, the gbdispstatus may haven't been set in gfx_late_resume yet.
		//! record here, and we may call brightness setting at the end of gfx_late_resume
		lastFailedBrightness = level;
		return 0;
	}

	lastFailedBrightness = -1;

	if (IS_POULSBO(dev)) {
		psb_intel_lvds_set_brightness(dev, level);
		psb_brightness = level;
		return 0;
	}

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		if (IS_MRST(dev)) {
			/* Calculate and set the brightness value */
			max_pwm_blc = REG_READ(BLC_PWM_CTL) >> MRST_BACKLIGHT_MODULATION_FREQ_SHIFT;
			blc_pwm_ctl = level * max_pwm_blc / BRIGHTNESS_MAX_LEVEL;

			/* Adjust the backlight level with the percent in
			 * dev_priv->blc_adj1;
			 */
			blc_pwm_ctl = blc_pwm_ctl * dev_priv->blc_adj1;
			blc_pwm_ctl = blc_pwm_ctl / BLC_ADJUSTMENT_MAX;

			/* Adjust the backlight level with the percent in
			 * dev_priv->blc_adj2;
			 */
			blc_pwm_ctl = blc_pwm_ctl * dev_priv->blc_adj2;
			blc_pwm_ctl = blc_pwm_ctl / BLC_ADJUSTMENT_MAX;


			if (blc_pol == BLC_POLARITY_INVERSE)
				blc_pwm_ctl = max_pwm_blc - blc_pwm_ctl;

			/* force PWM bit on */
			REG_WRITE(BLC_PWM_CTL2, (0x80000000 | REG_READ(BLC_PWM_CTL2)));
			REG_WRITE(BLC_PWM_CTL, (max_pwm_blc << MRST_BACKLIGHT_MODULATION_FREQ_SHIFT) |
				  blc_pwm_ctl);
		} else if (IS_MDFLD(dev)) {
			u32 adjusted_level = 0;

			/* Adjust the backlight level with the percent in
			 * dev_priv->blc_adj2;
			 */
			adjusted_level = level * dev_priv->blc_adj2;
			adjusted_level = adjusted_level / BLC_ADJUSTMENT_MAX;
			dev_priv->brightness_adjusted = adjusted_level;

#ifndef CONFIG_MDFLD_DSI_DPU
			if(!(dev_priv->dsr_fb_update & MDFLD_DSR_MIPI_CONTROL) && 
				(dev_priv->dbi_panel_on || dev_priv->dbi_panel_on2)){
				mdfld_dsi_dbi_exit_dsr(dev,MDFLD_DSR_MIPI_CONTROL, 0, 0);
				PSB_DEBUG_ENTRY("Out of DSR before set brightness to %d.\n",adjusted_level);
			}
#endif

				mdfld_dsi_brightness_control(dev, 0, adjusted_level);
				mdfld_dsi_brightness_control(dev, 2, adjusted_level);
		}
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	/* cache the brightness for later use */
	psb_brightness = level;
	return 0;
}

int psb_get_brightness(struct backlight_device *bd)
{
	DRM_DEBUG_DRIVER("brightness = 0x%x \n", psb_brightness);

	/* return locally cached var instead of HW read (due to DPST etc.) */
	return psb_brightness;
}

const struct backlight_ops psb_ops = {
	.get_brightness = psb_get_brightness,
	.update_status  = psb_set_brightness,
};

static int device_backlight_init(struct drm_device *dev)
{
	unsigned long CoreClock;
	/* u32 bl_max_freq; */
	/* unsigned long value; */
	u16 bl_max_freq;
	uint32_t value;
	uint32_t blc_pwm_precision_factor;
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;

	if (IS_MDFLD(dev)) {
		dev_priv->blc_adj1 = BLC_ADJUSTMENT_MAX;
		dev_priv->blc_adj2 = BLC_ADJUSTMENT_MAX;
		return 0;
	}

	if (IS_MRST(dev)) {
		dev_priv->blc_adj1 = BLC_ADJUSTMENT_MAX;
		dev_priv->blc_adj2 = BLC_ADJUSTMENT_MAX;

		/* this needs to come from VBT when available */
		bl_max_freq = 256;
		/* this needs to be set elsewhere */
		blc_pol = BLC_POLARITY_NORMAL;
		blc_pwm_precision_factor = BLC_PWM_PRECISION_FACTOR;

		CoreClock = dev_priv->core_freq;
	} else {
		/* get bl_max_freq and pol from dev_priv*/
		if (!dev_priv->lvds_bl) {
			DRM_ERROR("Has no valid LVDS backlight info\n");
			return 1;
		}
		bl_max_freq = dev_priv->lvds_bl->freq;
		blc_pol = dev_priv->lvds_bl->pol;
		blc_pwm_precision_factor = PSB_BLC_PWM_PRECISION_FACTOR;
		blc_brightnesscmd = dev_priv->lvds_bl->brightnesscmd;
		blc_type = dev_priv->lvds_bl->type;

		CoreClock = dev_priv->core_freq;
	} /*end if(IS_MRST(dev))*/

	value = (CoreClock * MHz) / BLC_PWM_FREQ_CALC_CONSTANT;
	value *= blc_pwm_precision_factor;
	value /= bl_max_freq;
	value /= blc_pwm_precision_factor;

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		if (IS_MRST(dev)) {
			if (value >
				(unsigned long long)MRST_BLC_MAX_PWM_REG_FREQ)
				return 2;
			else {
				REG_WRITE(BLC_PWM_CTL2,
					(0x80000000 | REG_READ(BLC_PWM_CTL2)));
				REG_WRITE(BLC_PWM_CTL, value |
				(value <<
					MRST_BACKLIGHT_MODULATION_FREQ_SHIFT));
			}
		} else {
			if (
			 value > (unsigned long long)PSB_BLC_MAX_PWM_REG_FREQ ||
			 value < (unsigned long long)PSB_BLC_MIN_PWM_REG_FREQ)
				return 2;
			else {
				value &= PSB_BACKLIGHT_PWM_POLARITY_BIT_CLEAR;
				REG_WRITE(BLC_PWM_CTL,
					(value << PSB_BACKLIGHT_PWM_CTL_SHIFT) |
					(value));
			}
		} /*end if(IS_MRST(dev))*/
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}
	return 0;
}

int psb_backlight_init(struct drm_device *dev)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	int ret = 0;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
	psb_backlight_device = backlight_device_register("psb-bl", NULL, (void *)dev, &psb_ops);
#else
	struct backlight_properties props;
	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = BRIGHTNESS_MAX_LEVEL;

	psb_backlight_device = backlight_device_register("psb-bl", NULL, (void *)dev, &psb_ops, &props);
#endif
	if (IS_ERR(psb_backlight_device))
		return PTR_ERR(psb_backlight_device);

	if ((ret = device_backlight_init(dev)) != 0)
		return ret;

	psb_backlight_device->props.brightness = BRIGHTNESS_MAX_LEVEL;
	psb_backlight_device->props.max_brightness = BRIGHTNESS_MAX_LEVEL;
	backlight_update_status(psb_backlight_device);
#endif
	return 0;
}

void psb_backlight_exit(void)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	psb_backlight_device->props.brightness = 0;
	backlight_update_status(psb_backlight_device);
	backlight_device_unregister(psb_backlight_device);
#endif
	return;
}

struct backlight_device * psb_get_backlight_device(void)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	return psb_backlight_device;
#endif
	return NULL;
}
