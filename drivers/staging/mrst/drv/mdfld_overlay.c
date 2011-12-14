/*
 * Copyright (C) 2011 Intel Corporation
 *
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
 */

#include <linux/debugfs.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>

#include "psb_drv.h"
#include "psb_drm.h"
#include "psb_gtt.h"
#include "psb_fb.h"

#include "fp_trig.h"

enum {
	OVL_DIRTY_REGS  = 0x1,
	OVL_DIRTY_COEFS = 0x2,

	/* FIXME IEP */
};

enum {
	OVL_NUM_PHASES = 17,
	OVL_NUM_TAPS_Y_H = 5,
	OVL_NUM_TAPS_Y_V = 3,
	OVL_NUM_TAPS_UV_H = 3,
	OVL_NUM_TAPS_UV_V = 3,
};

struct mfld_overlay_regs {
	u32 OBUF_0Y;
	u32 OBUF_1Y;
	u32 OBUF_0U;
	u32 OBUF_0V;
	u32 OBUF_1U;
	u32 OBUF_1V;
	u32 OSTRIDE;
	u32 YRGB_VPH;
	u32 UV_VPH;
	u32 HORZ_PH;
	u32 INIT_PHS;
	u32 DWINPOS;
	u32 DWINSZ;
	u32 SWIDTH;
	u32 SWIDTHSW;
	u32 SHEIGHT;
	u32 YRGBSCALE;
	u32 UVSCALE;
	u32 OCLRC0;
	u32 OCLRC1;
	u32 DCLRKV;
	u32 DCLRKM;
	u32 SCHRKVH;
	u32 SCHRKVL;
	u32 SCHRKEN;
	u32 OCONFIG;
	u32 OCOMD;
	u32 _RESERVED_0;
	u32 OSTART_0Y;
	u32 OSTART_1Y;
	u32 OSTART_0U;
	u32 OSTART_0V;
	u32 OSTART_1U;
	u32 OSTART_1V;
	u32 OTILEOFF_0Y;
	u32 OTILEOFF_1Y;
	u32 OTILEOFF_0U;
	u32 OTILEOFF_0V;
	u32 OTILEOFF_1U;
	u32 OTILEOFF_1V;
	u32 _RESERVED_1;
	u32 UVSCALEV;
	u32 _RESERVED_2[(0x200 - 0xA8) / 4]; /* 0xA8 - 0x1FC */
	u16 Y_VCOEFS[OVL_NUM_PHASES][OVL_NUM_TAPS_Y_V]; /* 0x200 */
	u16 _RESERVED_3[0x100 / 2 - OVL_NUM_TAPS_Y_V * OVL_NUM_PHASES];
	u16 Y_HCOEFS[OVL_NUM_PHASES][OVL_NUM_TAPS_Y_H]; /* 0x300 */
	u16 _RESERVED_4[0x200 / 2 - OVL_NUM_TAPS_Y_H * OVL_NUM_PHASES];
	u16 UV_VCOEFS[OVL_NUM_PHASES][OVL_NUM_TAPS_UV_V]; /* 0x500 */
	u16 _RESERVED_5[0x100 / 2 - OVL_NUM_TAPS_UV_V * OVL_NUM_PHASES];
	u16 UV_HCOEFS[OVL_NUM_PHASES][OVL_NUM_TAPS_UV_H]; /* 0x600 */
	u16 _RESERVED_6[0x100 / 2 - OVL_NUM_TAPS_UV_H * OVL_NUM_PHASES];
	/* FIXME IEP */
};

struct mfld_overlay {
	struct drm_plane base;

	struct drm_device *dev;

	int id;
	unsigned int mmio_offset;
	int pipe;

	struct {
		struct page *page;
		void *virt;
		u32 gtt;
	} regs;

	unsigned int dirty;

	int y_hscale, y_vscale;
	int uv_hscale, uv_vscale;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry;
#endif
};

#define to_mfld_overlay(plane) container_of(plane, struct mfld_overlay, base)

/* Configuration information */
struct mfld_overlay_config {
	/* how many line buffers are to be used?. */
	int num_line_buffers;
	/* is 'fb' interlaced? */
	bool interlaced;
	/* source window in 16.16 pixels. */
	struct drm_region src;
	/* destination window in pixels. */
	struct drm_region dst;
	/* destination clip in pixels. */
	struct drm_region clip;
	/* the framebuffer to be presented. */
	struct drm_framebuffer *fb;
	/* chroma siting information. */
	uint8_t chroma_siting;
	/* chroma subsampling factors. */
	int hsub, vsub;
	/* scaling ratios (src/dst) in 16.16. */
	int hscale, vscale;
};

/* Per plane configuration information */
struct mfld_overlay_plane {
	/* does this information refer to a chroma plane? */
	bool is_chroma;
	/* scaling factors (src/dst) in 16.16 pixels */
	int hscale, vscale;
	/* starting phases in 16.16 pixels */
	int hphase, vphase0, vphase1;
	/* starting offset in pixels */
	int xoff, yoff;
	/* size in pixels, includes all contributing pixels, even partial ones */
	int width, height;
	/* stride in bytes */
	int stride;
	/* bytes per pixel */
	int cpp;
};


#define OVL_REG_WRITE(ovl, reg, val) REG_WRITE((ovl)->mmio_offset + (reg), (val))
#define OVL_REG_READ(ovl, reg) REG_READ((ovl)->mmio_offset + (reg))

enum {
	OVL_OVADD  = 0x00,
	OVL_DOVSTA = 0x08,
	OVL_OGAMC5 = 0x10,
	OVL_OGAMC4 = 0x14,
	OVL_OGAMC3 = 0x18,
	OVL_OGAMC2 = 0x1c,
	OVL_OGAMC1 = 0x20,
	OVL_OGAMC0 = 0x24,
};

enum {
	OVL_OVADD_COEF			= 0x1 << 0,

	OVL_DOVSTA_OVR_UPDT		= 0x1 << 31,

	OVL_DCLRKM_KEY			= 0x1 << 31,

	OVL_OCONFIG_IEP_BYPASS		= 0x1 << 27,
	OVL_OCONFIG_GAMMA2_ENBL		= 0x1 << 16,
	OVL_OCONFIG_ZORDER		= 0x1 << 15,
	OVL_OCONFIG_CSC_MODE		= 0x1 << 5,
	OVL_OCONFIG_CSC_BYPASS		= 0x1 << 4,
	OVL_OCONFIG_CC_OUT		= 0x1 << 3,
	OVL_OCONFIG_REQUEST		= 0x1 << 2,
	OVL_OCONFIG_LINE_CONFIG		= 0x1 << 0,

	OVL_OCOMD_TILED			= 0x1 << 19,
	OVL_OCOMD_MIRRORING_HORZ	= 0x1 << 17,
	OVL_OCOMD_MIRRORING_VERT	= 0x2 << 17,
	OVL_OCOMD_MIRRORING_BOTH	= 0x3 << 17,
	OVL_OCOMD_MIRRORING		= 0x3 << 17,
	OVL_OCOMD_ORDER422_YUYV		= 0x0 << 14,
	OVL_OCOMD_ORDER422_YVYU		= 0x1 << 14,
	OVL_OCOMD_ORDER422_UYVY		= 0x2 << 14,
	OVL_OCOMD_ORDER422_VYUY		= 0x3 << 14,
	OVL_OCOMD_ORDER422		= 0x3 << 14,
	OVL_OCOMD_FORMAT_YUV422_PACKED	= 0x8 << 10,
	OVL_OCOMD_FORMAT_NV12		= 0xb << 10,
	OVL_OCOMD_FORMAT_YUV420_PLANAR	= 0xc << 10,
	OVL_OCOMD_FORMAT_YUV422_PLANAR	= 0xd << 10,
	OVL_OCOMD_FORMAT_YUV41X_PLANAR	= 0xe << 10,
	OVL_OCOMD_FORMAT		= 0xf << 10,
	OVL_OCOMD_TVSYNCFLIP_ENBL	= 0x1 << 7,
	OVL_OCOMD_BUF_TYPE		= 0x1 << 5,
	OVL_OCOMD_TEST			= 0x1 << 4,
	OVL_OCOMD_ACT_BUF		= 0x3 << 2,
	OVL_OCOMD_ACT_F			= 0x1 << 1,
	OVL_OCOMD_OV_ENBL		= 0x1 << 0,
};

static void write_ovadd(struct mfld_overlay *ovl)
{
	struct drm_device *dev = ovl->dev;
	static const u8 pipe_map[] = { 0, 2, 1, };
	u32 ovadd = ovl->regs.gtt;

	if (ovl->dirty & OVL_DIRTY_COEFS)
		ovadd |= OVL_OVADD_COEF;

	ovadd |= pipe_map[ovl->pipe] << 6;

	/* Guarantee ordering between shadow register memory and write to OVADD. */
	wmb();

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_FORCE_POWER_ON))
		return;

	OVL_REG_WRITE(ovl, OVL_OVADD, ovadd);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	ovl->dirty = 0;
}

enum {
	OVL_UPDATE_TIMEOUT = 20000,
};

static int ovl_wait(struct mfld_overlay *ovl)
{
	struct drm_device *dev = ovl->dev;
	unsigned int timeout = 0;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_FORCE_POWER_ON)) {
		dev_warn(dev->dev, "Failed to power up display island\n");
		/* FIXME appropriate error code? */
		return -ENXIO;
	}

	while (timeout++ < OVL_UPDATE_TIMEOUT && !(OVL_REG_READ(ovl, OVL_DOVSTA) & OVL_DOVSTA_OVR_UPDT))
		cpu_relax();

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	if (timeout >= OVL_UPDATE_TIMEOUT) {
		dev_warn(dev->dev, "Timed out waiting for overlay %c\n", ovl->id == 0 ? 'A' : 'C');
		return -ETIMEDOUT;
	}

	return 0;
}

static void ovl_commit(struct mfld_overlay *ovl)
{
	if (!ovl->dirty)
		return;

	write_ovadd(ovl);
}

static bool ovl_format_is_packed(uint32_t format)
{
	return drm_format_num_planes(format) == 1;
}

/*
 * Calculate the starting phase.
 */
static int ovl_calc_phase(int *start, int phase, int max_phase, int step)
{
	if (*start < 0) {
		phase += *start;
		*start = 0;
		return phase;
	}

	phase += *start & (step - 1);
	*start &= ~(step - 1);

	/*
	 * -1.0 <= phase < max_phase
	 * Maximize it to give the filter more to work with
	 * at the leading edge of the source window.
	 */
	while (*start >= step && phase + step < max_phase) {
		phase += step;
		*start -= step;
	}

	return phase;
}

/*
 * Adjust the source window end.
 */
static void ovl_adjust_end(int *end, int max, int max_trail, int step)
{
	int trail;

	*end = ALIGN(*end, step);

	if (*end >= max) {
		*end = max;
		return;
	}

	/*
	 * 0.0 <= trail <= max_trail
	 * Extend the end to give the filter more to work with
	 * at the trailing edge of the source window.
	 */
	trail = (max - *end) & ~(step - 1);
	trail = min(trail, max_trail);

	*end += trail;
}

/*
 * Calculate plane specific information.
 * All calculations are done in 16.16 pixels format.
 */
static void ovl_calc_plane(struct mfld_overlay_config *c,
			   struct mfld_overlay_plane *p,
			   unsigned int stride)
{
	struct drm_region src = c->src;
	/* Minimum step size for start offset registers. */
	int xstep = ovl_format_is_packed(c->fb->pixel_format) ? 0x20000 : 0x10000;
	int ystep = 0x10000;
	/*
	 * Number of extra samples utilized beyond the source
	 * window trailing edges (NUM_TAPS/2).
	 */
	int max_htrail = p->is_chroma ? 0x10000 : 0x20000;
	int max_vtrail = 0x10000;
	/*
	 * Limits of the starting phase registers. All registers can
	 * be programmed with values between 0.0-3.0, but some wrap
	 * at 1.5 in reality.
	 */
	int max_hphase = p->is_chroma ? 0x18000 : 0x30000;
	int max_vphase = 0x18000;
	int hsub  = p->is_chroma ? c->hsub : 1;
	int vsub  = p->is_chroma ? c->vsub : 1;
	int fb_width = (c->fb->width << 16) / hsub;
	int fb_height = (c->fb->height << (16 - c->interlaced)) / vsub;
	int ylimit;

	p->cpp = drm_format_plane_cpp(c->fb->pixel_format, p->is_chroma ? 1 : 0);

	p->hscale = c->hscale / hsub;
	p->vscale = c->vscale / vsub;

	p->stride = stride << c->interlaced;

	if (p->is_chroma) {
		int xoff, yoff;

		drm_region_subsample(&src, hsub, vsub);

		drm_chroma_phase_offsets(&xoff, &yoff, hsub, vsub, c->chroma_siting, false);
		drm_region_translate(&src, xoff, yoff);
	}

	/* The hardware goes belly up if there are not enough lines to fill the line buffers. */
	/* FIXME do this in a way that doesn't cause an extra phase shift between luma/chroma */
	ylimit = fb_height - (c->num_line_buffers == 3 ? 0x10000 : 0x8000) - 1;
	if (src.y1 > ylimit)
		src.y1 = ylimit;

	/* Use some extra samples beyond the trailing edge, if possible */
	ovl_adjust_end(&src.x2, fb_width, max_htrail, xstep);
	ovl_adjust_end(&src.y2, fb_height, max_vtrail, ystep);

	/* Hardware phase 0 == pixel center, hence the -0.5 luma lines adjustment. */
	p->hphase = ovl_calc_phase(&src.x1, -0x8000 / hsub, max_hphase, xstep);
	/* Apparently the -0.5 lume lines phase offset is wrong when two line buffers are used. */
	p->vphase0 = ovl_calc_phase(&src.y1, c->num_line_buffers == 3 ? -0x8000 / vsub : 0,
				    max_vphase, ystep);

	/* Top and bottom field phase difference is 0.5 luma lines. */
	p->vphase1 = p->vphase0 - c->interlaced * 0x8000 / vsub;

	p->xoff = src.x1 >> 16;
	p->yoff = src.y1 >> 16;

	p->width = (src.x2 >> 16) - p->xoff;
	p->height = (src.y2 >> 16) - p->yoff;

	/* The hardware fails if there are not enough lines to fill the line buffers. */
	if (p->height < c->num_line_buffers)
		p->height = c->num_line_buffers;
}

static int ovl_config_init(struct mfld_overlay_config *c,
			   int crtc_x, int crtc_y,
			   int crtc_w, int crtc_h,
			   unsigned int src_x, unsigned int src_y,
			   unsigned int src_w, unsigned int src_h,
			   unsigned int clip_x, unsigned int clip_y,
			   unsigned int clip_w, unsigned int clip_h,
			   uint8_t chroma_siting,
			   struct drm_framebuffer *fb)
{
	int num_planes = drm_format_num_planes(fb->pixel_format);
	unsigned int stride;

	c->fb = fb;

	/* FIXME what are the bandwidth limitations? */
	if (1)
		c->num_line_buffers = 3;
	else
		c->num_line_buffers = 2;

	/* FIXME */
	c->interlaced = false;

	c->hsub = drm_format_horz_chroma_subsampling(fb->pixel_format);
	c->vsub = drm_format_vert_chroma_subsampling(fb->pixel_format);

	/*
	 * FIXME what's the minimum width? Docs say NUM_TAPS
	 * but the hardware seems happy with less.
	 */
	if (fb->width < 1 || fb->width > 2047)
		return -EINVAL;

	/* FIXME not 100% sure about the chroma plane height limit */
	if (fb->height < c->num_line_buffers * c->vsub || fb->height > 2047)
		return -EINVAL;

	c->src.x1 = src_x;
	c->src.y1 = src_y;
	c->src.x2 = src_x + src_w;
	c->src.y2 = src_y + src_h;

	if (c->interlaced) {
		c->src.y1 >>= 1;
		c->src.y2 >>= 1;

		/*
		 * Top field phase starts at +0.25.
		 * Bottom field phase start at -0.25 (handled later).
		 */
		drm_region_translate(&c->src, 0, 0x4000);
	}

	if (num_planes == 3) {
		/* U and V share the same stride */
		if (fb->pitches[1] != fb->pitches[2])
			return -EINVAL;
	}

	stride = fb->pitches[0] << c->interlaced;
	if (stride & 0x3f)
		return -EINVAL;
	/* FIXME verify these */
	if (stride < 512)
		return -EINVAL;
	if (stride > (ovl_format_is_packed(fb->pixel_format) ? 8192 : 4096))
		return -EINVAL;

	if (num_planes > 1) {
		stride = fb->pitches[1] << c->interlaced;
		if (stride & 0x3f)
			return -EINVAL;
		/* FIXME verify these */
		if (stride < 256)
			return -EINVAL;
		if (stride > 2048)
			return -EINVAL;
	}

	c->dst.x1 = crtc_x;
	c->dst.y1 = crtc_y;
	c->dst.x2 = crtc_x + crtc_w;
	c->dst.y2 = crtc_y + crtc_h;

	c->clip.x1 = clip_x;
	c->clip.y1 = clip_y;
	c->clip.x2 = clip_x + clip_w;
	c->clip.y2 = clip_y + clip_h;

	c->chroma_siting = chroma_siting;

	return 0;
}

static void ovl_calc_scale_factors(struct mfld_overlay_config *c)
{
	int min_hscale, max_hscale, min_vscale, max_vscale;

	/* Upscale limit 1:(4096 / subsampling factor) */
	min_hscale = c->hsub << 4;
	min_vscale = c->vsub << 4;

	/* Downscale limit 16:1 */
	max_hscale = 16 << 16;
	max_vscale = 16 << 16;

	c->hscale = drm_calc_hscale(&c->src, &c->dst, min_hscale, max_hscale);
	c->vscale = drm_calc_vscale(&c->src, &c->dst, min_vscale, max_vscale);

	/* Make sure Y and U/V scaling ratios match exactly */
	c->hscale &= ~((c->hsub << 4) - 1);
	c->vscale &= ~((c->vsub << 4) - 1);

	/* Make the source window size an exact multiple of the scaling factors. */
	drm_region_adjust_size(&c->src,
			       drm_region_width(&c->dst) * c->hscale - drm_region_width(&c->src),
			       drm_region_height(&c->dst) * c->vscale - drm_region_height(&c->src));
}

static void ovl_setup_regs(struct mfld_overlay *ovl)
{
	struct mfld_overlay_regs *regs = ovl->regs.virt;

	regs->OCONFIG = OVL_OCONFIG_IEP_BYPASS;

	regs->DCLRKM = OVL_DCLRKM_KEY | 0xffffff;

	regs->SCHRKEN = 0xff;

	ovl->dirty |= OVL_DIRTY_REGS;
}

static void ovl_setup_coefs(struct mfld_overlay *ovl,
			    struct mfld_overlay_plane *y,
			    struct mfld_overlay_plane *uv)
{
	/* Sinc w/ cutoff=0.25, NUM_TAPS wide Hann window. */
	static const uint16_t down_y_vert_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_Y_V] = {
		[ 0] = { [0] = 0x3000, [1] = 0x1800, [2] = 0x1800, },
		[ 1] = { [0] = 0x3000, [1] = 0x1860, [2] = 0x2f40, },
		[ 2] = { [0] = 0x3000, [1] = 0x18e0, [2] = 0x2e40, },
		[ 3] = { [0] = 0x3040, [1] = 0x1930, [2] = 0x2d80, },
		[ 4] = { [0] = 0x3040, [1] = 0x1990, [2] = 0x2cc0, },
		[ 5] = { [0] = 0x3080, [1] = 0x1a00, [2] = 0x2bc0, },
		[ 6] = { [0] = 0x30c0, [1] = 0x1a50, [2] = 0x2b00, },
		[ 7] = { [0] = 0x3100, [1] = 0x1aa0, [2] = 0x2a40, },
		[ 8] = { [0] = 0x3180, [1] = 0x1ae0, [2] = 0x2980, },
		[ 9] = { [0] = 0x3200, [1] = 0x1b20, [2] = 0x28c0, },
		[10] = { [0] = 0x3280, [1] = 0x1b70, [2] = 0x3fc0, },
		[11] = { [0] = 0x3340, [1] = 0x1ba0, [2] = 0x3e40, },
		[12] = { [0] = 0x3400, [1] = 0x1bd0, [2] = 0x3cc0, },
		[13] = { [0] = 0x34c0, [1] = 0x1bf0, [2] = 0x3b80, },
		[14] = { [0] = 0x35c0, [1] = 0x1c10, [2] = 0x3a00, },
		[15] = { [0] = 0x36c0, [1] = 0x1c10, [2] = 0x3900, },
		[16] = { [0] = 0x37c0, [1] = 0x1c20, [2] = 0x37c0, },
	};
	static const uint16_t down_y_horz_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_Y_H] = {
		[ 0] = { [0] = 0x3000, [1] = 0x33a0, [2] = 0x2e20, [3] = 0x2e40, [4] = 0x33a0, },
		[ 1] = { [0] = 0xb000, [1] = 0x3420, [2] = 0x2e80, [3] = 0x2de0, [4] = 0x3320, },
		[ 2] = { [0] = 0xb000, [1] = 0x34a0, [2] = 0x2ef0, [3] = 0x2d60, [4] = 0x32c0, },
		[ 3] = { [0] = 0xb000, [1] = 0x3520, [2] = 0x2f40, [3] = 0x2d00, [4] = 0x3260, },
		[ 4] = { [0] = 0xb000, [1] = 0x35a0, [2] = 0x2f80, [3] = 0x2ca0, [4] = 0x3220, },
		[ 5] = { [0] = 0xb000, [1] = 0x3640, [2] = 0x2fe0, [3] = 0x2c20, [4] = 0x31c0, },
		[ 6] = { [0] = 0xb020, [1] = 0x36e0, [2] = 0x1810, [3] = 0x2bc0, [4] = 0x3180, },
		[ 7] = { [0] = 0xb020, [1] = 0x3780, [2] = 0x1838, [3] = 0x2b40, [4] = 0x3140, },
		[ 8] = { [0] = 0xb020, [1] = 0x3840, [2] = 0x1848, [3] = 0x2ae0, [4] = 0x3100, },
		[ 9] = { [0] = 0xb020, [1] = 0x38e0, [2] = 0x1870, [3] = 0x2a60, [4] = 0x30c0, },
		[10] = { [0] = 0xb020, [1] = 0x39a0, [2] = 0x1878, [3] = 0x2a00, [4] = 0x30a0, },
		[11] = { [0] = 0xb020, [1] = 0x3a60, [2] = 0x1890, [3] = 0x2980, [4] = 0x3080, },
		[12] = { [0] = 0xb020, [1] = 0x3b40, [2] = 0x18a0, [3] = 0x2900, [4] = 0x3060, },
		[13] = { [0] = 0xb020, [1] = 0x3c00, [2] = 0x18a8, [3] = 0x28a0, [4] = 0x3040, },
		[14] = { [0] = 0xb020, [1] = 0x3ce0, [2] = 0x18b8, [3] = 0x2820, [4] = 0x3020, },
		[15] = { [0] = 0xb000, [1] = 0x3da0, [2] = 0x18c0, [3] = 0x3f60, [4] = 0x3000, },
		[16] = { [0] = 0x3000, [1] = 0x3e80, [2] = 0x18c0, [3] = 0x3e80, [4] = 0x3000, },
		};
	static const uint16_t down_uv_vert_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_UV_V] = {
		[ 0] = { [0] = 0x3000, [1] = 0x1800, [2] = 0x1800, },
		[ 1] = { [0] = 0xb080, [1] = 0x1880, [2] = 0x2f40, },
		[ 2] = { [0] = 0xb080, [1] = 0x1900, [2] = 0x2e40, },
		[ 3] = { [0] = 0x3000, [1] = 0x1940, [2] = 0x2d80, },
		[ 4] = { [0] = 0x3080, [1] = 0x1980, [2] = 0x2cc0, },
		[ 5] = { [0] = 0x3080, [1] = 0x1a00, [2] = 0x2bc0, },
		[ 6] = { [0] = 0x3100, [1] = 0x1a40, [2] = 0x2b00, },
		[ 7] = { [0] = 0x3080, [1] = 0x1ac0, [2] = 0x2a40, },
		[ 8] = { [0] = 0x3100, [1] = 0x1b00, [2] = 0x2980, },
		[ 9] = { [0] = 0x3180, [1] = 0x1b40, [2] = 0x28c0, },
		[10] = { [0] = 0x3240, [1] = 0x1b80, [2] = 0x3fc0, },
		[11] = { [0] = 0x32c0, [1] = 0x1bc0, [2] = 0x3e40, },
		[12] = { [0] = 0x3440, [1] = 0x1bc0, [2] = 0x3cc0, },
		[13] = { [0] = 0x3480, [1] = 0x1c00, [2] = 0x3b80, },
		[14] = { [0] = 0x3600, [1] = 0x1c00, [2] = 0x3a00, },
		[15] = { [0] = 0x3700, [1] = 0x1c00, [2] = 0x3900, },
		[16] = { [0] = 0x3740, [1] = 0x1c40, [2] = 0x37c0, },
	};
	static const uint16_t down_uv_horz_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_UV_H] = {
		[ 0] = { [0] = 0x3000, [1] = 0x1800, [2] = 0x1800, },
		[ 1] = { [0] = 0x3000, [1] = 0x1870, [2] = 0x2f20, },
		[ 2] = { [0] = 0x3020, [1] = 0x18c8, [2] = 0x2e60, },
		[ 3] = { [0] = 0x3020, [1] = 0x1938, [2] = 0x2d80, },
		[ 4] = { [0] = 0x3040, [1] = 0x19a0, [2] = 0x2ca0, },
		[ 5] = { [0] = 0x3080, [1] = 0x19f0, [2] = 0x2be0, },
		[ 6] = { [0] = 0x30c0, [1] = 0x1a50, [2] = 0x2b00, },
		[ 7] = { [0] = 0x3120, [1] = 0x1a98, [2] = 0x2a40, },
		[ 8] = { [0] = 0x3180, [1] = 0x1af0, [2] = 0x2960, },
		[ 9] = { [0] = 0x3200, [1] = 0x1b30, [2] = 0x28a0, },
		[10] = { [0] = 0x3280, [1] = 0x1b70, [2] = 0x3fc0, },
		[11] = { [0] = 0x3340, [1] = 0x1ba0, [2] = 0x3e40, },
		[12] = { [0] = 0x33e0, [1] = 0x1bd8, [2] = 0x3cc0, },
		[13] = { [0] = 0x34c0, [1] = 0x1bf8, [2] = 0x3b60, },
		[14] = { [0] = 0x35a0, [1] = 0x1c10, [2] = 0x3a20, },
		[15] = { [0] = 0x36a0, [1] = 0x1c20, [2] = 0x38e0, },
		[16] = { [0] = 0x37c0, [1] = 0x1c20, [2] = 0x37c0, },
	};
	/* Sinc w/ cutoff=0.5, NUM_TAPS wide Hann window. */
	static const uint16_t up_y_vert_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_Y_V] = {
		[ 0] = { [0] = 0x3000, [1] = 0x1800, [2] = 0x1800, },
		[ 1] = { [0] = 0xb000, [1] = 0x18c0, [2] = 0x2e80, },
		[ 2] = { [0] = 0xb000, [1] = 0x19a0, [2] = 0x2cc0, },
		[ 3] = { [0] = 0xb040, [1] = 0x1a70, [2] = 0x2b40, },
		[ 4] = { [0] = 0xb040, [1] = 0x1b30, [2] = 0x29c0, },
		[ 5] = { [0] = 0xb040, [1] = 0x1bd0, [2] = 0x2880, },
		[ 6] = { [0] = 0xb080, [1] = 0x1c90, [2] = 0x3e40, },
		[ 7] = { [0] = 0xb0c0, [1] = 0x1d30, [2] = 0x3c00, },
		[ 8] = { [0] = 0xb0c0, [1] = 0x1dc0, [2] = 0x39c0, },
		[ 9] = { [0] = 0xb100, [1] = 0x1e50, [2] = 0x37c0, },
		[10] = { [0] = 0xb100, [1] = 0x1eb0, [2] = 0x3640, },
		[11] = { [0] = 0xb100, [1] = 0x1f10, [2] = 0x34c0, },
		[12] = { [0] = 0xb100, [1] = 0x1f70, [2] = 0x3340, },
		[13] = { [0] = 0xb100, [1] = 0x1fb0, [2] = 0x3240, },
		[14] = { [0] = 0xb0c0, [1] = 0x1fe0, [2] = 0x3140, },
		[15] = { [0] = 0xb080, [1] = 0x0800, [2] = 0x3080, },
		[16] = { [0] = 0x3000, [1] = 0x0800, [2] = 0x3000, },
	};
	static const uint16_t up_y_horz_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_Y_H] = {
		[ 0] = { [0] = 0x3000, [1] = 0xb4a0, [2] = 0x1930, [3] = 0x1920, [4] = 0xb4a0, },
		[ 1] = { [0] = 0x3000, [1] = 0xb500, [2] = 0x19d0, [3] = 0x1880, [4] = 0xb440, },
		[ 2] = { [0] = 0x3000, [1] = 0xb540, [2] = 0x1a88, [3] = 0x2f80, [4] = 0xb3e0, },
		[ 3] = { [0] = 0x3000, [1] = 0xb580, [2] = 0x1b30, [3] = 0x2e20, [4] = 0xb380, },
		[ 4] = { [0] = 0x3000, [1] = 0xb5c0, [2] = 0x1bd8, [3] = 0x2cc0, [4] = 0xb320, },
		[ 5] = { [0] = 0x3020, [1] = 0xb5e0, [2] = 0x1c60, [3] = 0x2b80, [4] = 0xb2c0, },
		[ 6] = { [0] = 0x3020, [1] = 0xb5e0, [2] = 0x1cf8, [3] = 0x2a20, [4] = 0xb260, },
		[ 7] = { [0] = 0x3020, [1] = 0xb5e0, [2] = 0x1d80, [3] = 0x28e0, [4] = 0xb200, },
		[ 8] = { [0] = 0x3020, [1] = 0xb5c0, [2] = 0x1e08, [3] = 0x3f40, [4] = 0xb1c0, },
		[ 9] = { [0] = 0x3020, [1] = 0xb580, [2] = 0x1e78, [3] = 0x3ce0, [4] = 0xb160, },
		[10] = { [0] = 0x3040, [1] = 0xb520, [2] = 0x1ed8, [3] = 0x3aa0, [4] = 0xb120, },
		[11] = { [0] = 0x3040, [1] = 0xb4a0, [2] = 0x1f30, [3] = 0x3880, [4] = 0xb0e0, },
		[12] = { [0] = 0x3040, [1] = 0xb400, [2] = 0x1f78, [3] = 0x3680, [4] = 0xb0a0, },
		[13] = { [0] = 0x3020, [1] = 0xb340, [2] = 0x1fb8, [3] = 0x34a0, [4] = 0xb060, },
		[14] = { [0] = 0x3020, [1] = 0xb240, [2] = 0x1fe0, [3] = 0x32e0, [4] = 0xb040, },
		[15] = { [0] = 0x3020, [1] = 0xb140, [2] = 0x1ff8, [3] = 0x3160, [4] = 0xb020, },
		[16] = { [0] = 0xb000, [1] = 0x3000, [2] = 0x0800, [3] = 0x3000, [4] = 0xb000, },
	};
	static const uint16_t up_uv_vert_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_UV_V] = {
		[ 0] = { [0] = 0x3000, [1] = 0x1800, [2] = 0x1800, },
		[ 1] = { [0] = 0xb000, [1] = 0x18c0, [2] = 0x2e80, },
		[ 2] = { [0] = 0xb080, [1] = 0x19c0, [2] = 0x2cc0, },
		[ 3] = { [0] = 0xb080, [1] = 0x1a80, [2] = 0x2b40, },
		[ 4] = { [0] = 0xb080, [1] = 0x1b40, [2] = 0x29c0, },
		[ 5] = { [0] = 0x3000, [1] = 0x1bc0, [2] = 0x2880, },
		[ 6] = { [0] = 0xb040, [1] = 0x1c80, [2] = 0x3e40, },
		[ 7] = { [0] = 0xb100, [1] = 0x1d40, [2] = 0x3c00, },
		[ 8] = { [0] = 0xb0c0, [1] = 0x1dc0, [2] = 0x39c0, },
		[ 9] = { [0] = 0xb0c0, [1] = 0x1e40, [2] = 0x37c0, },
		[10] = { [0] = 0xb140, [1] = 0x1ec0, [2] = 0x3640, },
		[11] = { [0] = 0xb0c0, [1] = 0x1f00, [2] = 0x34c0, },
		[12] = { [0] = 0xb140, [1] = 0x1f80, [2] = 0x3340, },
		[13] = { [0] = 0xb140, [1] = 0x1fc0, [2] = 0x3240, },
		[14] = { [0] = 0xb140, [1] = 0x0800, [2] = 0x3140, },
		[15] = { [0] = 0xb080, [1] = 0x0800, [2] = 0x3080, },
		[16] = { [0] = 0x3000, [1] = 0x0800, [2] = 0x3000, },
	};
	static const uint16_t up_uv_horz_coefs[OVL_NUM_PHASES][OVL_NUM_TAPS_UV_H] = {
		[ 0] = { [0] = 0x3000, [1] = 0x1800, [2] = 0x1800, },
		[ 1] = { [0] = 0xb000, [1] = 0x18d0, [2] = 0x2e60, },
		[ 2] = { [0] = 0xb000, [1] = 0x1990, [2] = 0x2ce0, },
		[ 3] = { [0] = 0xb020, [1] = 0x1a68, [2] = 0x2b40, },
		[ 4] = { [0] = 0xb040, [1] = 0x1b20, [2] = 0x29e0, },
		[ 5] = { [0] = 0xb060, [1] = 0x1bd8, [2] = 0x2880, },
		[ 6] = { [0] = 0xb080, [1] = 0x1c88, [2] = 0x3e60, },
		[ 7] = { [0] = 0xb0a0, [1] = 0x1d28, [2] = 0x3c00, },
		[ 8] = { [0] = 0xb0c0, [1] = 0x1db8, [2] = 0x39e0, },
		[ 9] = { [0] = 0xb0e0, [1] = 0x1e40, [2] = 0x37e0, },
		[10] = { [0] = 0xb100, [1] = 0x1eb8, [2] = 0x3620, },
		[11] = { [0] = 0xb100, [1] = 0x1f18, [2] = 0x34a0, },
		[12] = { [0] = 0xb100, [1] = 0x1f68, [2] = 0x3360, },
		[13] = { [0] = 0xb0e0, [1] = 0x1fa8, [2] = 0x3240, },
		[14] = { [0] = 0xb0c0, [1] = 0x1fe0, [2] = 0x3140, },
		[15] = { [0] = 0xb060, [1] = 0x1ff0, [2] = 0x30a0, },
		[16] = { [0] = 0x3000, [1] = 0x0800, [2] = 0x3000, },
	};
	struct mfld_overlay_regs *regs = ovl->regs.virt;

	if (ovl->y_vscale == 0 ||
	    (ovl->y_vscale > 0x10000) != (y->vscale > 0x10000)) {
		memcpy(regs->Y_VCOEFS, y->vscale > 0x10000 ?
		       down_y_vert_coefs : up_y_vert_coefs, sizeof(regs->Y_VCOEFS));
		ovl->dirty |= OVL_DIRTY_COEFS;
		ovl->y_vscale = y->vscale;
	}

	if (ovl->y_hscale == 0 ||
	    (ovl->y_hscale > 0x10000) != (y->hscale > 0x10000)) {
		memcpy(regs->Y_HCOEFS, y->hscale > 0x10000 ?
		       down_y_horz_coefs : up_y_horz_coefs, sizeof(regs->Y_HCOEFS));
		ovl->dirty |= OVL_DIRTY_COEFS;
		ovl->y_hscale = y->hscale;
	}

	if (ovl->uv_vscale == 0 ||
	    (ovl->uv_vscale > 0x10000) != (uv->vscale > 0x10000)) {
		memcpy(regs->UV_VCOEFS, uv->vscale > 0x10000 ?
		       down_uv_vert_coefs : up_uv_vert_coefs, sizeof(regs->UV_VCOEFS));
		ovl->dirty |= OVL_DIRTY_COEFS;
		ovl->uv_vscale = uv->vscale;
	}

	if (ovl->uv_hscale == 0 ||
	    (ovl->uv_hscale > 0x10000) != (uv->hscale > 0x10000)) {
		memcpy(regs->UV_HCOEFS, uv->hscale > 0x10000 ?
		       down_uv_horz_coefs : up_uv_horz_coefs, sizeof(regs->UV_HCOEFS));
		ovl->dirty |= OVL_DIRTY_COEFS;
		ovl->uv_hscale = uv->hscale;
	}
}

static void ovl_disable(struct mfld_overlay *ovl)
{
	struct mfld_overlay_regs *regs = ovl->regs.virt;

	/* Wait for previous update to finish before touching the shadow registers */
	if (ovl_wait(ovl))
		return;

	regs->OCOMD &= ~OVL_OCOMD_OV_ENBL;

	ovl->dirty |= OVL_DIRTY_REGS;

	ovl_commit(ovl);
}

static int
mfld_overlay_update_plane(struct drm_plane *plane, struct drm_crtc *crtc, struct drm_framebuffer *fb,
			  int crtc_x, int crtc_y, unsigned int crtc_w, unsigned int crtc_h,
			  uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h)
{
	struct psb_intel_crtc *psbcrtc = to_psb_intel_crtc(crtc);
	struct psb_framebuffer *psbfb = to_psb_fb(fb);
	struct mfld_overlay *ovl = to_mfld_overlay(plane);
	struct mfld_overlay_regs *regs = ovl->regs.virt;
	struct mfld_overlay_config c = { };
	struct mfld_overlay_plane y = {
		.is_chroma = false,
	};
	struct mfld_overlay_plane uv = {
		.is_chroma = true,
	};
	unsigned int y_off, u_off, v_off;
	bool visible;
	int r;

	r = ovl_config_init(&c, crtc_x, crtc_y, crtc_w, crtc_h,
			    src_x, src_y, src_w, src_h,
			    0, 0, crtc->hwmode.crtc_hdisplay,
			    crtc->hwmode.crtc_vdisplay,
			    plane->opts.chroma_siting, fb);
	if (r)
		return r;

	ovl_calc_scale_factors(&c);

	visible = drm_region_clip_scaled(&c.src, &c.dst, &c.clip,
					 c.hscale, c.vscale);
	if (!visible) {
		ovl_disable(ovl);
		return 0;
	}

	/* Adjust source to start/end at correct destination pixel centers */
	drm_region_adjust_size(&c.src, -c.hscale, -c.vscale);

	ovl->pipe = psbcrtc->pipe;

	ovl_calc_plane(&c, &y, fb->pitches[0]);

	if (ovl_format_is_packed(fb->pixel_format)) {
		int xoff, yoff;

		drm_chroma_phase_offsets(&xoff, &yoff, c.hsub, c.vsub, c.chroma_siting, false);

		uv.hscale = y.hscale / c.hsub;
		uv.hphase = y.hphase / c.hsub + xoff;

		/* Docs say these are ignored, but they are in fact needed. */
		uv.vscale = y.vscale / c.vsub;
		uv.vphase0 = y.vphase0 / c.vsub + yoff;
		uv.vphase1 = y.vphase1 / c.vsub + yoff;
	} else {
		ovl_calc_plane(&c, &uv, fb->pitches[1]);
	}

	/* Wait for previous update to finish before touching the register memory */
	r = ovl_wait(ovl);
	if (r)
		return r;

	regs->YRGBSCALE = ((y.vscale & 0xfff0) << 16) | ((y.hscale & 0x1ffff0) >> 1);
	regs->UVSCALE = ((uv.vscale & 0xfff0) << 16) | ((uv.hscale & 0x1ffff0) >> 1);
	regs->UVSCALEV = (y.vscale & 0x7ff0000) | ((uv.vscale & 0x7ff0000) >> 16);

	regs->INIT_PHS = (((y.vphase0  >> 16) & 0xf) << 20) |
			 (((y.vphase1  >> 16) & 0xf) << 16) |
			 (((y.hphase   >> 16) & 0xf) << 12) |
			 (((uv.vphase0 >> 16) & 0xf) <<  8) |
			 (((uv.vphase1 >> 16) & 0xf) <<  4) |
			 (((uv.hphase  >> 16) & 0xf) <<  0);
	regs->YRGB_VPH = ((y.vphase1 & 0xfff0) << 16) | (y.vphase0 & 0xfff0);
	regs->UV_VPH = ((uv.vphase1 & 0xfff0) << 16) | (uv.vphase0 & 0xfff0);
	regs->HORZ_PH = ((uv.hphase & 0xfff0) << 16) | (y.hphase & 0xfff0);

	regs->SWIDTH = (uv.width << 16) | (y.width & 0xffff);
	regs->SHEIGHT = (uv.height << 16) | (y.height & 0xffff);

	y_off = fb->offsets[0] + y.xoff  *  y.cpp +  y.yoff * y.stride;
	u_off = fb->offsets[1] + uv.xoff * uv.cpp + uv.yoff * uv.stride;
	v_off = fb->offsets[2] + uv.xoff * uv.cpp + uv.yoff * uv.stride;

	regs->OBUF_0Y = y_off;
	regs->OBUF_0U = u_off;
	regs->OBUF_0V = v_off;
	regs->OBUF_1Y = y_off;
	regs->OBUF_1U = u_off;
	regs->OBUF_1V = v_off;

	if (c.interlaced) {
		regs->OBUF_1Y += y.stride >> 1;
		regs->OBUF_1U += uv.stride >> 1;
		regs->OBUF_1V += uv.stride >> 1;
	}

	y.width *= y.cpp;
	uv.width *= uv.cpp;
	y.width = (((y_off + y.width + 0x3f) >> 6) - (y_off >> 6)) << 3;
	uv.width = (((u_off + uv.width + 0x3f) >> 6) - (u_off >> 6)) << 3;
	regs->SWIDTHSW = (uv.width << 16) | (y.width & 0xffff);

	regs->OSTRIDE = (uv.stride << 16) | (y.stride & 0xffff);

	regs->DWINPOS = (c.dst.y1 << 16) | (c.dst.x1 & 0xffff);
	regs->DWINSZ = ((c.dst.y2 - c.dst.y1) << 16) | ((c.dst.x2 - c.dst.x1) & 0xffff);

	regs->OSTART_0Y = psbfb->offset;
	regs->OSTART_0U = psbfb->offset;
	regs->OSTART_0V = psbfb->offset;
	regs->OSTART_1Y = psbfb->offset;
	regs->OSTART_1U = psbfb->offset;
	regs->OSTART_1V = psbfb->offset;

	regs->OCONFIG &= ~OVL_OCONFIG_LINE_CONFIG;
	regs->OCONFIG |= (c.num_line_buffers == 3) << 0;

	regs->OCOMD &= ~(OVL_OCOMD_ORDER422 | OVL_OCOMD_FORMAT);

	switch (fb->pixel_format) {
	case DRM_FORMAT_YUYV:
		regs->OCOMD |= OVL_OCOMD_ORDER422_YUYV |
			       OVL_OCOMD_FORMAT_YUV422_PACKED;
		break;
	case DRM_FORMAT_YVYU:
		regs->OCOMD |= OVL_OCOMD_ORDER422_YVYU |
			       OVL_OCOMD_FORMAT_YUV422_PACKED;
		break;
	case DRM_FORMAT_UYVY:
		regs->OCOMD |= OVL_OCOMD_ORDER422_UYVY |
			       OVL_OCOMD_FORMAT_YUV422_PACKED;
		break;
	case DRM_FORMAT_VYUY:
		regs->OCOMD |= OVL_OCOMD_ORDER422_VYUY |
			       OVL_OCOMD_FORMAT_YUV422_PACKED;
		break;
	case DRM_FORMAT_YUV422:
		regs->OCOMD |= OVL_OCOMD_FORMAT_YUV422_PLANAR;
		break;
	case DRM_FORMAT_YUV420:
		regs->OCOMD |= OVL_OCOMD_FORMAT_YUV420_PLANAR;
		break;
	case DRM_FORMAT_NV12:
		regs->OCOMD |= OVL_OCOMD_FORMAT_NV12;
		break;
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YUV410:
		regs->OCOMD |= OVL_OCOMD_FORMAT_YUV41X_PLANAR;
		break;
	}

	regs->OCOMD |= OVL_OCOMD_OV_ENBL;

	ovl->dirty |= OVL_DIRTY_REGS;

	ovl_setup_coefs(ovl, &y, &uv);

	ovl_commit(ovl);

	return 0;
}

static int
mfld_overlay_disable_plane(struct drm_plane *plane)
{
	struct mfld_overlay *ovl = to_mfld_overlay(plane);

	ovl_disable(ovl);

	return 0;
}

static void ovl_set_brightness_contrast(struct mfld_overlay *ovl, const struct drm_plane_opts *opts)
{
	struct mfld_overlay_regs *regs = ovl->regs.virt;
	int bri, con;

	if (opts->csc_range == DRM_CSC_RANGE_MPEG) {
		if (opts->contrast >= 0x8000)
			/* 255/219 <= contrast <= 7.53125 */
			con = (((opts->contrast - 0x8000) * 815) >> 16) + 75;
		else
			/* 0.0 <= contrast < 255/219 */
			con = opts->contrast / 440;
	} else {
		if (opts->contrast >= 0x8000)
			/* 1.0 <= contrast <= 7.53125 */
			con = (((opts->contrast - 0x8000) * 837) >> 16) + 64;
		else
			/* 0.0 <= contrast < 1.0 */
			con = opts->contrast >> 9;
	}

	if (opts->csc_range == DRM_CSC_RANGE_MPEG) {
		/* 16 * 255/219 = ~19 */
		if (opts->brightness >= 0x8000)
			/* -19 <= bri <= 127 */
			bri = ((opts->brightness - 0x8000) / 224 - 19);
		else
			/* -128 <= bri < -19 */
			bri = (opts->brightness / 300 - 128);
	} else {
		/* -128 <= bri <= 127 */
		bri = (opts->brightness >> 8) - 128;
	}

	/* Scare anyone who touches this code w/o thinking. */
	WARN_ON(con < 0 || con > 482);
	WARN_ON(bri < -128 || bri > 127);

	regs->OCLRC0 = ((con & 0x1ff) << 18) | ((bri & 0xff) << 0);

	ovl->dirty |= OVL_DIRTY_REGS;
}

static void ovl_set_hue_saturation(struct mfld_overlay *ovl, const struct drm_plane_opts *opts)
{
	struct mfld_overlay_regs *regs = ovl->regs.virt;
	int sat;
	/* [0:0xffff] -> [-FP_PI/2:FP_PI/2] shifted by 2*FP_PI */
	unsigned int deg = ((opts->hue + 2) >> 2) + 3 * FP_PI / 2;
	int sh_sin = fp_sin(deg);
	int sh_cos = fp_cos(deg);

	if (opts->csc_range == DRM_CSC_RANGE_MPEG) {
		if (opts->saturation >= 0x8000)
			/*
			 * abs(sh_sin) + abs(sh_cos) < 8.0, therefore
			 * 255/224 <= saturation <= 8/(2*sin(PI/4))
			 */
			sat = (((opts->saturation - 0x8000) * 1157) >> 16) + 146;
		else
			/* 0.0 <= saturation < 255/224 */
			sat = opts->saturation / 225;
	} else {
		if (opts->saturation >= 0x8000)
			/*
			 * abs(sh_sin) + abs(sh_cos) < 8.0, therefore
			 * 1.0 <= saturation <= 8/(2*sin(PI/4))
			 */
			sat = (((opts->saturation - 0x8000) * 1193) >> 16) + 128;
		else
			/* 0.0 <= saturation < 1.0 */
			sat = opts->saturation >> 8;
	}
	sh_sin = (sh_sin * sat) / (1 << FP_FRAC_BITS);
	sh_cos = (sh_cos * sat) / (1 << FP_FRAC_BITS);

	/* Scare anyone who touches this code w/o thinking. */
	WARN_ON(abs(sh_sin) > 724);
	WARN_ON(sh_cos < 0 || sh_cos > 724);
	WARN_ON(abs(sh_sin) + sh_cos >= 8 << 7);

	regs->OCLRC1 = ((sh_sin & 0x7ff) << 16) | ((sh_cos & 0x3ff) << 0);

	ovl->dirty |= OVL_DIRTY_REGS;
}

static void ovl_set_csc_matrix(struct mfld_overlay *ovl, struct drm_plane_opts *opts)
{
	struct mfld_overlay_regs *regs = ovl->regs.virt;

	switch (opts->csc_matrix) {
	case DRM_CSC_MATRIX_BT601:
		regs->OCONFIG &= ~OVL_OCONFIG_CSC_MODE;
		break;
	default:
		opts->csc_matrix = DRM_CSC_MATRIX_BT709;
		regs->OCONFIG |= OVL_OCONFIG_CSC_MODE;
		break;
	}

	ovl->dirty |= OVL_DIRTY_REGS;
}

static int
mfld_overlay_set_plane_opts(struct drm_plane *plane, uint32_t flags, struct drm_plane_opts *opts)
{
	struct mfld_overlay *ovl = to_mfld_overlay(plane);

	if (flags & DRM_MODE_PLANE_CSC_RANGE) {
		switch (opts->csc_range) {
		case DRM_CSC_RANGE_JPEG:
			break;
		default:
			opts->csc_range = DRM_CSC_RANGE_MPEG;
			break;
		}

		/* Must re-compute color correction if YCbCr range is changed */
		flags |= DRM_MODE_PLANE_BRIGHTNESS | DRM_MODE_PLANE_CONTRAST |
			 DRM_MODE_PLANE_HUE | DRM_MODE_PLANE_SATURATION;
	}

	if (flags & DRM_MODE_PLANE_CSC_MATRIX) {
		switch (opts->csc_matrix) {
		case DRM_CSC_MATRIX_BT601:
			break;
		default:
			opts->csc_matrix = DRM_CSC_MATRIX_BT709;
			break;
		}

		ovl_set_csc_matrix(ovl, opts);
	}

	if (flags & DRM_MODE_PLANE_CHROMA_SITING) {
		/* hardware can't handle misaligned chroma planes, ignore it */
		opts->chroma_siting &= ~DRM_CHROMA_SITING_MISALIGNED_PLANES;

		/* FIXME should recompute scaling etc. in case chroma phase changed. */
	}

	if (flags & (DRM_MODE_PLANE_BRIGHTNESS | DRM_MODE_PLANE_CONTRAST))
		ovl_set_brightness_contrast(ovl, opts);

	if (flags & (DRM_MODE_PLANE_HUE | DRM_MODE_PLANE_SATURATION))
		ovl_set_hue_saturation(ovl, opts);

	ovl_commit(ovl);

	return 0;
}

static void mfld_overlay_destroy(struct drm_plane *plane);

static const struct drm_plane_funcs mfld_overlay_funcs = {
	.update_plane = mfld_overlay_update_plane,
	.disable_plane = mfld_overlay_disable_plane,
	.destroy = mfld_overlay_destroy,
	.set_plane_opts = mfld_overlay_set_plane_opts,
};

static const uint32_t mfld_overlay_formats[] = {
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_NV12,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV411,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV410,
};

static int ovl_regs_init(struct drm_device *dev, struct mfld_overlay *ovl)
{
	u32 gtt_page_offset;
	u32 phys;
	int r;

	/* FIXME allocate two pages to allow new updates w/o waiting for the previous one? */

	ovl->regs.page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!ovl->regs.page)
		return -ENOMEM;

	phys = page_to_pfn(ovl->regs.page) << PAGE_SHIFT;

	r = psb_gtt_map_pvr_memory(dev, (u32)ovl->regs.page, KERNEL_ID,
				   (IMG_CPU_PHYADDR *)&phys, 1, &gtt_page_offset, 16);
	if (r)
		goto free_page;

	ovl->regs.gtt = gtt_page_offset << PAGE_SHIFT;

	ovl->regs.virt = vmap(&ovl->regs.page, 1, 0, ttm_io_prot(TTM_PL_FLAG_UNCACHED, PAGE_KERNEL));
	if (!ovl->regs.virt)
		goto unmap_gtt;

	return 0;

 unmap_gtt:
	psb_gtt_unmap_pvr_memory(dev, (u32)ovl->regs.page, KERNEL_ID);
 free_page:
	__free_page(ovl->regs.page);

	return r;
}

static void ovl_regs_fini(struct drm_device *dev, struct mfld_overlay *ovl)
{
	vunmap(ovl->regs.virt);
	psb_gtt_unmap_pvr_memory(dev, (u32)ovl->regs.page, KERNEL_ID);
	__free_page(ovl->regs.page);
}

#ifdef CONFIG_DEBUG_FS
static int ovl_regs_show(struct seq_file *s, void *data)
{
	struct mfld_overlay *ovl = s->private;
	struct mfld_overlay_regs *regs = ovl->regs.virt;
	struct drm_device *dev = ovl->base.dev;

#undef OVL_REG_SHOW
#define OVL_REG_SHOW(x) seq_printf(s, "%s 0x%08x\n", #x, OVL_REG_READ(ovl, OVL_ ## x))

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_FORCE_POWER_ON))
		return -ENXIO;

	OVL_REG_SHOW(OVADD);
	OVL_REG_SHOW(DOVSTA);
	OVL_REG_SHOW(OGAMC5);
	OVL_REG_SHOW(OGAMC4);
	OVL_REG_SHOW(OGAMC3);
	OVL_REG_SHOW(OGAMC2);
	OVL_REG_SHOW(OGAMC1);
	OVL_REG_SHOW(OGAMC0);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

#undef OVL_REG_SHOW
#define OVL_REG_SHOW(x) seq_printf(s, "%s 0x%08x\n", #x, regs->x)

	OVL_REG_SHOW(OBUF_0Y);
	OVL_REG_SHOW(OBUF_1Y);
	OVL_REG_SHOW(OBUF_0U);
	OVL_REG_SHOW(OBUF_0V);
	OVL_REG_SHOW(OBUF_1U);
	OVL_REG_SHOW(OBUF_1V);
	OVL_REG_SHOW(OSTRIDE);
	OVL_REG_SHOW(YRGB_VPH);
	OVL_REG_SHOW(UV_VPH);
	OVL_REG_SHOW(HORZ_PH);
	OVL_REG_SHOW(INIT_PHS);
	OVL_REG_SHOW(DWINPOS);
	OVL_REG_SHOW(DWINSZ);
	OVL_REG_SHOW(SWIDTH);
	OVL_REG_SHOW(SWIDTHSW);
	OVL_REG_SHOW(SHEIGHT);
	OVL_REG_SHOW(YRGBSCALE);
	OVL_REG_SHOW(UVSCALE);
	OVL_REG_SHOW(OCLRC0);
	OVL_REG_SHOW(OCLRC1);
	OVL_REG_SHOW(DCLRKV);
	OVL_REG_SHOW(DCLRKM);
	OVL_REG_SHOW(SCHRKVH);
	OVL_REG_SHOW(SCHRKVL);
	OVL_REG_SHOW(SCHRKEN);
	OVL_REG_SHOW(OCONFIG);
	OVL_REG_SHOW(OCOMD);
	OVL_REG_SHOW(OSTART_0Y);
	OVL_REG_SHOW(OSTART_1Y);
	OVL_REG_SHOW(OSTART_0U);
	OVL_REG_SHOW(OSTART_0V);
	OVL_REG_SHOW(OSTART_1U);
	OVL_REG_SHOW(OSTART_1V);
	OVL_REG_SHOW(OTILEOFF_0Y);
	OVL_REG_SHOW(OTILEOFF_1Y);
	OVL_REG_SHOW(OTILEOFF_0U);
	OVL_REG_SHOW(OTILEOFF_0V);
	OVL_REG_SHOW(OTILEOFF_1U);
	OVL_REG_SHOW(OTILEOFF_1V);
	OVL_REG_SHOW(UVSCALEV);

	return 0;
}

static int ovl_regs_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, ovl_regs_show, inode->i_private);
}

static const struct file_operations ovl_regs_fops = {
	.owner = THIS_MODULE,
	.open = ovl_regs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ovl_coefs_show(struct seq_file *s, void *data)
{
	struct mfld_overlay *ovl = s->private;
	struct mfld_overlay_regs *regs = ovl->regs.virt;
	int p, t;

	for (p = 0; p < OVL_NUM_PHASES; p++) {
		seq_printf(s, "Y_VCOEFS[%d]", p);
		for (t = 0; t < OVL_NUM_TAPS_Y_V; t++)
			seq_printf(s, " 0x%04x", regs->Y_VCOEFS[p][t]);
		seq_printf(s, "\n");
	}
	for (p = 0; p < OVL_NUM_PHASES; p++) {
		seq_printf(s, "Y_HCOEFS[%d]", p);
		for (t = 0; t < OVL_NUM_TAPS_Y_H; t++)
			seq_printf(s, " 0x%04x", regs->Y_HCOEFS[p][t]);
		seq_printf(s, "\n");
	}
	for (p = 0; p < OVL_NUM_PHASES; p++) {
		seq_printf(s, "UV_VCOEFS[%d]", p);
		for (t = 0; t < OVL_NUM_TAPS_UV_V; t++)
			seq_printf(s, " 0x%04x", regs->UV_VCOEFS[p][t]);
		seq_printf(s, "\n");
	}
	for (p = 0; p < OVL_NUM_PHASES; p++) {
		seq_printf(s, "UV_HCOEFS[%d]", p);
		for (t = 0; t < OVL_NUM_TAPS_UV_H; t++)
			seq_printf(s, " 0x%04x", regs->UV_HCOEFS[p][t]);
		seq_printf(s, "\n");
	}

	return 0;
}

static int ovl_coefs_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, ovl_coefs_show, inode->i_private);
}

static const struct file_operations ovl_coefs_fops = {
	.owner = THIS_MODULE,
	.open = ovl_coefs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void ovl_debugfs_init(struct drm_device *dev, struct mfld_overlay *ovl)
{
	char name[16];

	snprintf(name, sizeof name, "overlay%c", ovl->id == 0 ? 'A' : 'C');

	ovl->debugfs_dentry = debugfs_create_dir(name, dev->primary->debugfs_root);
	if (!ovl->debugfs_dentry)
		return;

	debugfs_create_file("regs", 0440, ovl->debugfs_dentry, ovl, &ovl_regs_fops);
	debugfs_create_file("coefs", 0440, ovl->debugfs_dentry, ovl, &ovl_coefs_fops);
}

static void ovl_debugfs_fini(struct mfld_overlay *ovl)
{
	debugfs_remove_recursive(ovl->debugfs_dentry);
}
#else
static void ovl_debugfs_init(struct drm_device *dev, struct mfld_overlay *ovl) {}
static void ovl_debugfs_fini(struct mfld_overlay *ovl) {}
#endif

int mdfld_overlay_init(struct drm_device *dev, int id)
{
	unsigned long possible_crtcs = 0x7;
	struct mfld_overlay *ovl;
	int r;

	switch (id) {
	case 0:
	case 1:
		break;
	default:
		return -EINVAL;
	}

	ovl = kzalloc(sizeof *ovl, GFP_KERNEL);
	if (!ovl)
		return -ENOMEM;

	r = ovl_regs_init(dev, ovl);
	if (r)
		goto free_ovl;

	ovl->dev = dev;
	ovl->pipe = 0;
	ovl->id = id;
	ovl->mmio_offset = 0x30000 + id * 0x8000;

	ovl_debugfs_init(dev, ovl);

	/* FIXME move */
	ovl_setup_regs(ovl);

	/* fill in some defaults */
	drm_plane_opts_defaults(&ovl->base.opts);
	ovl->base.opts.csc_range = DRM_CSC_RANGE_MPEG;
	ovl->base.opts.csc_matrix = DRM_CSC_MATRIX_BT709;
	ovl->base.opts.chroma_siting = DRM_CHROMA_SITING_MPEG2;
	ovl->base.opts_flags = DRM_MODE_PLANE_BRIGHTNESS | DRM_MODE_PLANE_CONTRAST |
			       DRM_MODE_PLANE_HUE | DRM_MODE_PLANE_SATURATION |
			       DRM_MODE_PLANE_CSC_RANGE | DRM_MODE_PLANE_CSC_MATRIX |
			       DRM_MODE_PLANE_CHROMA_SITING;

	ovl_set_brightness_contrast(ovl, &ovl->base.opts);
	ovl_set_hue_saturation(ovl, &ovl->base.opts);
	ovl_set_csc_matrix(ovl, &ovl->base.opts);

	drm_plane_init(dev, &ovl->base, possible_crtcs, &mfld_overlay_funcs,
		       mfld_overlay_formats, ARRAY_SIZE(mfld_overlay_formats));

	return 0;

 free_ovl:
	kfree(ovl);

	return r;
}

static void mfld_overlay_destroy(struct drm_plane *plane)
{
	struct mfld_overlay *ovl = to_mfld_overlay(plane);
	struct drm_device *dev = ovl->dev;

	ovl_debugfs_fini(ovl);

	ovl_wait(ovl);
	ovl_regs_fini(dev, ovl);

	kfree(ovl);
}
