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
 *
 */

#ifndef _PSB_PAGE_FLIP_H_
#define _PSB_PAGE_FLIP_H_

void psb_intel_crtc_process_vblank(struct drm_crtc *crtc);
int psb_intel_crtc_page_flip(struct drm_crtc *crtc,
                             struct drm_framebuffer *fb,
                             struct drm_pending_vblank_event *event);

#endif /* _PSB_PAGE_FLIP_H_ */
