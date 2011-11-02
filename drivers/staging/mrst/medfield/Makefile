# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

INCDIR=drivers/staging/mrst

include_dirs := \
	-I$(INCDIR)/pvr \
	-I$(INCDIR)/pvr/include4 \
        -I$(INCDIR)/pvr/services4/include \
        -I$(INCDIR)/pvr/services4/include/env/linux \
        -I$(INCDIR)/pvr/services4/3rdparty/linux_framebuffer_mrst \
        -I$(INCDIR)/pvr/services4/srvkm/env/linux \
        -I$(INCDIR)/pvr/services4/srvkm/include \
        -I$(INCDIR)/pvr/services4/srvkm/bridged \
        -I$(INCDIR)/pvr/services4/srvkm/common \
        -I$(INCDIR)/pvr/services4/system/include \
        -I$(INCDIR)/pvr/services4/srvkm/hwdefs \
        -I$(INCDIR)/pvr/services4/srvkm/bridged/sgx \
        -I$(INCDIR)/pvr/services4/srvkm/devices/sgx \
        -I$(INCDIR)/ \
        -I$(INCDIR)/drv \
        -I$(INCDIR)/bc_video \
        -I$(INCDIR)/imgv \
	-Iinclude/linux \
	-Iinclude/drm

ccflags-y += $(include_dirs)
ccflags-y += -I$(INCDIR)/pvr/services4/system/unified -DSGX540 -DSUPPORT_SGX540 -DSGX_CORE_REV=121

ccflags-y += \
	-DLINUX \
	-DPVR_BUILD_DIR="\"pc_i686_moorestown_linux\"" \
	-DSERVICES4 \
	-D_XOPEN_SOURCE=600 \
	-DPVR2D_VALIDATE_INPUT_PARAMS \
	-DDISPLAY_CONTROLLER=mrstlfb \
	-UDEBUG_LOG_PATH_TRUNCATE \
	-DSUPPORT_SRVINIT \
	-DSUPPORT_SGX \
	-DSUPPORT_LINUX_X86_WRITECOMBINE \
	-DTRANSFER_QUEUE \
	-DSUPPORT_DRI_DRM \
	-DSUPPORT_DRI_DRM_EXT \
	-DSYS_USING_INTERRUPTS \
	-DSUPPORT_HW_RECOVERY \
	-DSUPPORT_ACTIVE_POWER_MANAGEMENT \
	-DPVR_SECURE_HANDLES \
	-DUSE_PTHREADS \
	-DSUPPORT_SGX_EVENT_OBJECT \
	-DSUPPORT_SGX_HWPERF \
	-DSUPPORT_SGX_LOW_LATENCY_SCHEDULING \
	-DSUPPORT_LINUX_X86_PAT \
	-DPVR_PROC_USE_SEQ_FILE \
	-DSUPPORT_CACHE_LINE_FLUSH \
	-DSUPPORT_CPU_CACHED_BUFFERS \
	-DDEBUG_MESA_OGL_TRACE \
	-DDISABLE_PM \
	-DSUPPORT_SGX_NEW_STATUS_VALS \
	-DSUPPORT_PERCONTEXT_PB \
	-DDRM_PVR_USE_INTEL_FB	\
	-DSUPPORT_MEMINFO_IDS

ccflags-$(CONFIG_MDFD_GL3) += -DSUPPORT_EXTERNAL_SYSTEM_CACHE

PVRSRV_RESOURCE_PROFILING =
PERPROC_LIST =
RES_MAN_EXTEND =
PVRSRV_OS_MEM_INFO =

ccflags-$(PVRSRV_RESOURCE_PROFILING) += -DPVRSRV_RESOURCE_PROFILING
ccflags-$(PERPROC_LIST) += -DPERPROC_LIST
ccflags-$(RES_MAN_EXTEND) += -DRES_MAN_EXTEND
ccflags-$(PVRSRV_OS_MEM_INFO) += -DPVRSRV_OS_MEM_INFO

ccflags-$(CONFIG_DRM_MID_RELEASE) += -DBUILD="\"release\"" -DPVR_BUILD_TYPE="\"release\"" -DRELEASE
ccflags-$(CONFIG_DRM_MID_DEBUG) += -DBUILD="\"debug\"" -DPVR_BUILD_TYPE="\"debug\"" -DPVR_DEBUG_EXT -DDEBUG_LINUX_MEM_AREAS -DDEBUG_LINUX_MEMORY_ALLOCATIONS -DDEBUG_LINUX_MMAP_AREAS -DDEBUG_BRIDGE_KM
ccflags-$(CONFIG_PCI_MSI) += -DCONFIG_PCI_MSI

PVRDIR = ../pvr
ENVDIR = ../pvr/services4/srvkm/env/linux
COMMONDIR = ../pvr/services4/srvkm/common
BRIDGEDDIR = ../pvr/services4/srvkm/bridged
SGXDIR = ../pvr/services4/srvkm/devices/sgx
FBDEVDIR = ../pvr/services4/3rdparty/linux_framebuffer_mrst
DRMDRVDIR = ../drv
SYSCONFIGDIR = ../pvr/services4/system/unified
IMGVDIR = ../imgv
BUFFER_CLASS_DIR = ../bc_video

medfield_gfx-y += $(ENVDIR)/osfunc.o \
	  $(ENVDIR)/ossync.o \
	  $(ENVDIR)/mutils.o \
	  $(ENVDIR)/mmap.o \
	  $(ENVDIR)/module.o \
	  $(ENVDIR)/pdump.o \
	  $(ENVDIR)/proc.o \
	  $(ENVDIR)/pvr_bridge_k.o \
	  $(ENVDIR)/pvr_debug.o \
	  $(ENVDIR)/mm.o \
	  $(ENVDIR)/mutex.o \
	  $(ENVDIR)/event.o \
	  $(ENVDIR)/osperproc.o \
	  $(ENVDIR)/pvr_drm.o

medfield_gfx-y += $(COMMONDIR)/buffer_manager.o \
	  $(COMMONDIR)/devicemem.o \
	  $(COMMONDIR)/deviceclass.o \
	  $(COMMONDIR)/handle.o \
	  $(COMMONDIR)/hash.o \
	  $(COMMONDIR)/metrics.o \
	  $(COMMONDIR)/pvrsrv.o \
	  $(COMMONDIR)/queue.o \
	  $(COMMONDIR)/ra.o \
	  $(COMMONDIR)/resman.o \
	  $(COMMONDIR)/power.o \
	  $(COMMONDIR)/mem.o \
	  $(COMMONDIR)/pdump_common.o \
	  $(COMMONDIR)/perproc.o \
	  $(COMMONDIR)/lists.o \
	  $(COMMONDIR)/mem_debug.o \
	  $(COMMONDIR)/osfunc_common.o

medfield_gfx-y += $(BRIDGEDDIR)/bridged_support.o \
	  $(BRIDGEDDIR)/bridged_pvr_bridge.o \
	  $(BRIDGEDDIR)/sgx/bridged_sgx_bridge.o

medfield_gfx-y += $(SYSCONFIGDIR)/sysconfig.o \
	  $(SYSCONFIGDIR)/sysutils.o \
	  $(SYSCONFIGDIR)/sys_pvr_drm_export.o

medfield_gfx-y += $(SGXDIR)/sgxinit.o \
	  $(SGXDIR)/sgxpower.o \
	  $(SGXDIR)/sgxreset.o \
	  $(SGXDIR)/sgxutils.o \
	  $(SGXDIR)/sgxkick.o \
	  $(SGXDIR)/sgxtransfer.o \
	  $(SGXDIR)/mmu.o \
	  $(SGXDIR)/pb.o

medfield_gfx-y += $(FBDEVDIR)/mrstlfb_displayclass.o \
	  $(FBDEVDIR)/mrstlfb_linux.o

medfield_gfx-y += $(DRMDRVDIR)/psb_bl.o \
	  $(DRMDRVDIR)/psb_dpst.o \
	  $(DRMDRVDIR)/psb_drv.o \
	  $(DRMDRVDIR)/psb_fb.o \
	  $(DRMDRVDIR)/psb_gtt.o \
	  $(DRMDRVDIR)/psb_hotplug.o \
	  $(DRMDRVDIR)/psb_intel_display.o \
	  $(DRMDRVDIR)/psb_page_flip.o \
	  $(DRMDRVDIR)/mdfld_hdmi_audio.o \
	  $(DRMDRVDIR)/mdfld_msic.o \
	  $(DRMDRVDIR)/mdfld_intel_hdcp.o \
	  $(DRMDRVDIR)/psb_intel_hdmi.o \
	  $(DRMDRVDIR)/psb_intel_hdmi_i2c.o \
	  $(DRMDRVDIR)/psb_reset.o \
	  $(DRMDRVDIR)/psb_schedule.o \
	  $(DRMDRVDIR)/psb_sgx.o \
	  $(DRMDRVDIR)/psb_socket.o \
	  $(DRMDRVDIR)/psb_pvr_glue.o \
	  $(DRMDRVDIR)/psb_umevents.o \
	  $(DRMDRVDIR)/mdfld_dsi_dbi.o \
	  $(DRMDRVDIR)/mdfld_dsi_dpi.o \
	  $(DRMDRVDIR)/mdfld_dsi_output.o \
	  $(DRMDRVDIR)/mdfld_output.o \
	  $(DRMDRVDIR)/tpo_cmd.o \
	  $(DRMDRVDIR)/tpo_vid.o \
	  $(DRMDRVDIR)/tmd_vid.o \
	  $(DRMDRVDIR)/tc35876x-dsi-lvds.o \
	  $(DRMDRVDIR)/mdfld_dsi_pkg_sender.o

medfield_gfx-y += \
	  $(IMGVDIR)/topaz_power.o \
	  $(IMGVDIR)/pnw_topaz.o \
	  $(IMGVDIR)/pnw_topazinit.o \
	  $(IMGVDIR)/psb_buffer.o \
	  $(IMGVDIR)/psb_fence.o \
	  $(IMGVDIR)/psb_mmu.o \
	  $(IMGVDIR)/psb_msvdx.o \
	  $(IMGVDIR)/msvdx_power.o \
	  $(IMGVDIR)/psb_msvdxinit.o \
	  $(IMGVDIR)/psb_ttm_glue.o \
	  $(IMGVDIR)/psb_ttm_fence.o \
	  $(IMGVDIR)/psb_ttm_fence_user.o \
	  $(IMGVDIR)/psb_ttm_placement_user.o

medfield_gfx-$(CONFIG_MDFLD_DSI_DPU) += $(DRMDRVDIR)/mdfld_dsi_dbi_dpu.o

medfield_gfx-$(CONFIG_MDFD_GL3) += $(DRMDRVDIR)/mdfld_gl3.o

medfield_gfx-$(CONFIG_DEBUG_FS) += $(PVRDIR)/pvr_debugfs.o
medfield_gfx-$(CONFIG_PVR_TRACE_CMD) += $(PVRDIR)/pvr_trace_cmd.o

medfield_gfx-y += $(DRMDRVDIR)/psb_powermgmt.o $(DRMDRVDIR)/psb_irq.o

medfield_gfx-y += $(BUFFER_CLASS_DIR)/bufferclass_video.o \
	$(BUFFER_CLASS_DIR)/bufferclass_video_linux.o

obj-$(CONFIG_DRM_MDFLD) += medfield_gfx.o
