#
# Spec file originally created for Fedora, modified for Moblin Linux
#

Summary: The Linux kernel (the core of the Linux operating system)


# For a stable, released kernel, released_kernel should be 1. For rawhide
# and/or a kernel built from an rc snapshot, released_kernel should
# be 0.
%define released_kernel 1

# Versions of various parts

# base_sublevel is the kernel version we're starting with and patching
# on top of -- for example, 2.6.22-rc7 starts with a 2.6.21 base,
# which yields a base_sublevel of 21.

%define base_sublevel 0


## If this is a released kernel ##
%if 0%{?released_kernel}
# Do we have a 3.0.y stable update to apply?
%define stable_update 8
# 3.x.y kernel always has the stable_update digit
%define stablerev .%{stable_update}
# Set rpm version accordingly
%define rpmversion 3.%{base_sublevel}%{?stablerev}

## The not-released-kernel case ##
%else
# The next upstream release sublevel (base_sublevel+1)
%define upstream_sublevel %(expr %{base_sublevel} + 1)
# The rc snapshot level

%define rcrev 0


%if 0%{?rcrev}
%define rctag ~rc%rcrev
%endif

%if !0%{?rcrev}
%define rctag ~rc0
%endif

# Set rpm version accordingly
%define rpmversion 3.%{upstream_sublevel}%{?rctag}
%endif

# The kernel tarball/base version
%define kversion 3.%{base_sublevel}

%define make_target bzImage

%define KVERREL %{version}-%{release}
%define hdrarch %_target_cpu

%define all_x86 i386 i586 i686 %{ix86}

%define _default_patch_fuzz 0

# Per-arch tweaks

%ifarch %{all_x86}
%define image_install_path boot
%define hdrarch i386
%define kernel_image arch/x86/boot/bzImage
%endif

%ifarch x86_64
%define image_install_path boot
%define kernel_image arch/x86/boot/bzImage
%endif

ExclusiveArch: %{all_x86}

#
# Packages that need to be installed before the kernel is, because the %post
# scripts use them.
#
%define kernel_prereq  /sbin/lsmod, /sbin/init

#
# This macro does requires, provides, conflicts, obsoletes for a kernel package.
#	%%kernel_reqprovconf <subpackage>
# It uses any kernel_<subpackage>_conflicts and kernel_<subpackage>_obsoletes
# macros defined above.
#
%define kernel_reqprovconf \
Provides: kernel = %{rpmversion}-%{release}\
Provides: kernel-uname-r = %{KVERREL}%{?1:-%{1}}\
Requires(pre): %{kernel_prereq}\
%{?1:%{expand:%%{?kernel_%{1}_conflicts:Conflicts: %%{kernel_%{1}_conflicts}}}}\
%{?1:%{expand:%%{?kernel_%{1}_provides:Provides: %%{kernel_%{1}_provides}}}}\
# We can't let RPM do the dependencies automatic because it'll then pick up\
# a correct but undesirable perl dependency from the module headers which\
# isn't required for the kernel proper to function\
AutoReq: no\
AutoProv: yes\
%{nil}

Name: kernel-adaptation-bb

Group: System/Kernel
License: GPLv2
URL: http://www.kernel.org/
Version: %{rpmversion}
Release: 1

%kernel_reqprovconf

#
# List the packages used during the kernel build
#
BuildRequires: module-init-tools, bash >= 2.03
BuildRequires:  findutils,  make >= 3.78
#BuildRequires: linux-firmware
BuildRequires: elfutils-libelf-devel binutils-devel 
BuildRequires: which

Source0: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/linux-%{kversion}.tar.bz2
Source1: ti-wlan-2fc817c.tar.bz2
Source2: wl12xx-compat-build.sh

Source200: cmdline

# Maintain these patches without git-buildpackage
#GbpIgnorePatch: 0 1 2

# For a stable release kernel
%if 0%{?stable_update}
Patch00: patch-3.%{base_sublevel}.%{stable_update}.bz2

%endif
%if 0%{?rcrev}
Patch00: patch-3.%{upstream_sublevel}-rc%{rcrev}.bz2
%endif

# Reminder of the patch filename format:
# linux-<version it is supposed to be upstream>-<description-separated-with-dashes>.patch
#


#
# Stable patch - critical bugfixes
#


#
# MCG Android tree differences
#
Patch1:     f816404.diff.bz2

#
# TI WLAN (out-of-tree) builder script patch
#
Patch2:     tizen-wl12xx-compat-build.patch

#
# Tizen patches
#
Patch3:     0001-CFLAGS-fix-for-Tizen.patch
Patch4:     0002-Enable-proc-fs-to-print-more-than-32-groups-entries.patch
Patch5:     0003-PVR-hybrid-rm-drivers-staging-mrst-from-MCG-kernel.patch
Patch6:     0004-PVR-driver.patch
Patch7:     0005-gfx-pvr-add-missing-1.7-IOCTL-IDs.patch
Patch8:     0006-gfx-tc35876x-fix-i2c-driver-device-name-mismatch.patch
Patch9:     0007-gfx-tc35876x-don-t-register-the-device-ufi-does-it-a.patch
Patch10:    0008-gfx-pvr-keep-around-IOCTL-names-even-for-release-bui.patch
Patch11:    0009-gfx-pvr-add-missing-1.7-ukernel-commands.patch
Patch12:    0010-gfx-pvr-increase-SGX_MAX_INIT_MEM_HANDLES-per-1.7.patch
Patch13:    0011-gfx-pvr-fix-SGX_BRIDGE_INIT_INFO-per-1.7.patch
Patch14:    0012-gfx-display-force-panel-type.patch
Patch15:    0013-gfx-fix-MDFD_GL3-makefile-logic.patch
Patch16:    0014-gfx-drv-add-missing-mdfld_gl3-header.patch
Patch17:    0015-gfx-drv-make-page-flip-work-on-fb-s-with-pvrBO-null.patch
Patch18:    0016-Remove-the-reference-to-the-moorestown-directory.patch
Patch19:    0017-gfx-drv-tc35876x-fix-input-muxing-for-dv1.patch
Patch20:    0018-gfx-pvr-rename-DEBUG-to-PVR_DEBUG_EXT.patch
Patch21:    0019-gfx-pvr-fix-clock-enabling-per-1.7.patch
Patch22:    0020-gfx-pvr-fix-SGX-get-misc-info-ABI.patch
Patch23:    0021-gfx-pvr-Split-3D-paramter-heap-to-shared-and-per-con.patch
Patch24:    0022-gfx-pvr-Update-heap-address-to-match-1.7-DDK.patch
Patch25:    0023-gfx-pvr-fix-typo-in-PVRSRV_BRIDGE_CHG_DEV_MEM_ATTRIB.patch
Patch26:    0024-gfx-pvr-fix-SGX-KICK-IOCTL-param-struct.patch
Patch27:    0025-gfx-pvr-add-missing-IOCTLs.patch
Patch28:    0026-gfx-pvr-Keep-required-drm-pos-close-function.patch
Patch29:    0027-gfx-pvr-enable-workqueues.patch
Patch30:    0028-gfx-pvr-change-map-export_devmem-to-map-export_devme.patch
Patch31:    0029-gfx-pvr-fix-sgx-transfer-kick-IOCTLs.patch
Patch32:    0030-gfx-pvr-annotate-IOCTL-IDs.patch
Patch33:    0031-gfx-pvr-make-DoQuerySyncOpsSatisfied-accept-counter-.patch
Patch34:    0032-gfx-pvr-add-PVRSRVSyncOps-TakeToken-FlushToToken.patch
Patch35:    0033-gfx-pvr-Implement-ioctl-return-for-devinitpart2.patch
Patch36:    0034-gfx-pvr-Update-add-shared-parameter-buffer-ioctl.patch
Patch37:    0035-gfx-pvr-Implement-uKernel-assert-fail-status-variabl.patch
Patch38:    0036-gfx-pvr-Add-host-control-variable-to-match-uKernel.patch
Patch39:    0037-gfx-pvr-Remove-unused-variable-from-uKernel-struct.patch
Patch40:    0038-gfx-pvr-add-force-cleanup-param.patch
Patch41:    0039-gfx-pvr-Update-error-return-codes-to-match-1.7.patch
Patch42:    0040-gfx-pvr-indicate-presence-of-cache-op-in-misc-info.patch
Patch43:    0041-gfx-drv-tc35876x-fix-bridge-and-panel-GPIO-numbers-a.patch
Patch44:    0042-gfx-drv-tc35876x-don-t-oops-if-functions-are-called-.patch
Patch45:    0043-gfx-pvr-clarify-Kconfig-terms-wrt.-ABI-version-vs.-s.patch
Patch46:    0044-gfx-pvr-remove-ABI-dependency-on-firmware-tracing-fe.patch
Patch47:    0045-gfx-pvr-add-header-to-track-the-ABI-version.patch
Patch48:    0046-gfx-pvr-remove-warning-on-gaps-in-the-IOCTL-ID-range.patch
Patch49:    0047-gfx-pvr-reduce-verbosity-of-debug-messages.patch
Patch50:    0048-gfx-drv-fix-display-backlight-PWM-duty-cycle-setting.patch
Patch51:    0049-gfx-pvr-add-Kconfig-option-to-dump-fw-trace-to-conso.patch
Patch52:    0050-gfx-pvr-refactor-fw-state-dumping-code.patch
Patch53:    0051-gfx-pvr-refactor-pvr_get_sgx_dev_info.patch
Patch54:    0052-gfx-pvr-export-HWRecoveryResetSGX.patch
Patch55:    0053-gfx-pvr-add-debugfs-entry-to-reset-sgx.patch
Patch56:    0054-gfx-pvr-add-debugfs-entry-to-read-sgx-firmware-trace.patch
Patch57:    0055-gfx-drv-tc35876x-Fix-physical-display-size-informati.patch
Patch58:    0056-gfx-drv-Fix-mode-after-all-drm-clients-have-exited.patch
Patch59:    0057-gfx-pvr-move-core-debugging-functions-to-a-separate-.patch
Patch60:    0058-gfx-pvr-make-sure-power-is-on-during-SGX-reset.patch
Patch61:    0059-gfx-pvr-add-sgx_-read-write-_reg.patch
Patch62:    0060-gfx-pvr-add-sgx_save_registers_no_pwron.patch
Patch63:    0061-gfx-pvr-debugfs-replace-test_and_set_bit-with-spinlo.patch
Patch64:    0062-gfx-pvr-add-hwrec_debugfs-entries.patch
Patch65:    0063-gfx-pvr-fix-regression-in-user-debug-request.patch
Patch66:    0064-gfx-pvr-Move-ioctl-number-check-before-first-use.patch
Patch67:    0065-gfx-build-driver-using-top-level-Makefile-and-drop-m.patch
Patch68:    0066-gfx-hide-false-positive-warnings-include-dir-warning.patch
Patch69:    0067-gfx-display-Avoid-NULL-pointer-deference.patch
Patch70:    0068-gfx-pvr-reduce-loglevel-of-fw-state-not-available-ms.patch
Patch71:    0069-gfx-pvr-make-firmware-trace-output-IMG-compatible.patch
Patch72:    0070-gfx-pvr-fix-locking-of-the-firmare-trace-debugfs-ent.patch
Patch73:    0071-gfx-display-tc35876x-fix-null-pointer-dereference-in.patch
Patch74:    0072-gfx-display-tc35876x-remove-device-creation-hack.patch
Patch75:    0073-gfx-display-remove-legacy-pm-interface.patch
Patch76:    0074-gfx-display-reduce-the-use-of-global-variables.patch
Patch77:    0075-gfx-display-remove-suspicious-runtime-PM-related-cod.patch
Patch78:    0076-gfx-display-remove-the-remains-of-unused-procfs-supp.patch
Patch79:    0077-gfx-display-remove-unused-module-parameter-rtpm-gfxr.patch
Patch80:    0078-gfx-display-fix-and-clean-runtime-PM-code.patch
Patch81:    0079-gfx-display-clean-up-PCI-suspend-resume.patch
Patch82:    0080-gfx-display-remove-module-and-early-parameter-to-tog.patch
Patch83:    0081-gfx-display-drop-gl3-enable-kernel-command-line-and-.patch
Patch84:    0082-gfx-display-add-Android-early-suspend-support.patch
Patch85:    0083-gfx-display-put-DSI-lanes-to-ULPS-before-disabling-p.patch
Patch86:    0084-gfx-display-add-driver-for-CMI-LCD-panel-I2C.patch
Patch87:    0085-HACK-gfx-display-add-display-I2C-device.patch
Patch88:    0086-gfx-display-switch-panel-power-off-on-when-blanking-.patch
Patch89:    0087-gfx-display-tc35876x-soft-reset-the-LCD-controller-a.patch
Patch90:    0088-gfx-display-give-the-panel-more-time-to-wake-up-afte.patch
Patch91:    0089-gfx-display-reg-and-field-helpers.patch
Patch92:    0090-gfx-display-only-change-the-device-ready-bit-don-t-t.patch
Patch93:    0091-gfx-display-fix-pipe-plane-enable-disable.patch
Patch94:    0092-gfx-display-unconditionally-enable-display.patch
Patch95:    0093-gfx-display-use-REG_BIT_WAIT-for-waiting-bits-to-fli.patch
Patch96:    0094-gfx-display-don-t-touch-port-control-twice-in-a-row.patch
Patch97:    0095-staging-mrst-Return-ERR_PTR-from-fb_create-hook.patch
Patch98:    0096-drm-add-plane-support-v3.patch
Patch99:    0097-drm-add-an-fb-creation-ioctl-that-takes-a-pixel-form.patch
Patch100:   0098-drm-Add-a-missing.patch
Patch101:   0099-drm-Redefine-pixel-formats.patch
Patch102:   0100-drm-plane-Clear-plane.crtc-and-plane.fb-after-disabl.patch
Patch103:   0101-drm-fourcc-Use-__u32-instead-of-u32.patch
Patch104:   0102-drm-plane-Check-source-coordinates.patch
Patch105:   0103-drm-plane-Check-crtc-coordinates-against-integer-ove.patch
Patch106:   0104-drm-plane-Make-formats-parameter-to-drm_plane_init-c.patch
Patch107:   0105-drm-plane-Check-that-the-fb-pixel-format-is-supporte.patch
Patch108:   0106-drm-Replace-pitch-with-pitches-in-drm_framebuffer.patch
Patch109:   0107-drm-Handle-duplicate-FOURCCs.patch
Patch110:   0108-drm-Check-that-the-requested-pixel-format-is-valid.patch
Patch111:   0109-drm-Add-drm_format_num_planes-utility-function.patch
Patch112:   0110-drm-Add-drm_format_plane_cpp-utility-function.patch
Patch113:   0111-drm-Add-drm_format_-horz-vert-_chroma_subsampling-ut.patch
Patch114:   0112-drm-Add-drm_framebuffer_check-utility-function.patch
Patch115:   0113-drm-Add-struct-drm_region-and-assorted-utility-funct.patch
Patch116:   0114-drm-Add-drm_calc_-hscale-vscale-utility-functions.patch
Patch117:   0115-drm-plane-Add-plane-options-ioctl.patch
Patch118:   0116-drm-Add-drm_chroma_phase_offsets-utility-function.patch
Patch119:   0117-staging-mrst-Add-alignment-argument-to-psb_gtt_map_p.patch
Patch120:   0118-staging-mrst-Use-drm_framebuffer_check.patch
Patch121:   0119-staging-mrst-Add-support-for-Medfield-video-overlays.patch
Patch122:   0120-staging-mrst-Add-overlay-color-correction-settings.patch
Patch123:   0121-staging-mrst-Add-overlay-CSC-matrix-and-chroma-sitin.patch
Patch124:   0122-staging-mrst-Add-overlay-color-keying-and-constant-a.patch
Patch125:   0123-staging-mrst-Add-overlay-Z-order-support.patch
Patch126:   0124-staging-mrst-Need-to-wait-for-overlay-in-set_plane_o.patch
Patch127:   0125-staging-mrst-Fix-zorder-handling-while-overlay-updat.patch
Patch128:   0126-drm-Install-drm_fourcc.h.patch
Patch129:   0127-drm-plane-mutex_unlock-was-missing.patch
Patch130:   0128-drm-Fix-__user-sparse-warnings.patch
Patch131:   0129-staging-mrst-Fix-error-handling-in-psbfb_create.patch
Patch132:   0130-staging-mrst-Remove-dead-code.patch
Patch133:   0131-staging-mrst-psb_gtt-Remove-the-rw-semaphore.patch
Patch134:   0132-staging-mrst-overlay-Use-set_memory_wc-instead-of-vm.patch
Patch135:   0133-staging-mrst-overlay-Use-jiffies-based-timeout-in-ov.patch
Patch136:   0134-staging-mrst-overlay-Flush-posted-writes-to-the-OVAD.patch
Patch137:   0135-staging-mrst-overlay-Use-double-buffering-for-overla.patch
Patch138:   0136-staging-mrst-overlay-Use-msleep-1-in-ovl_wait.patch
Patch139:   0137-staging-mrst-ossync-Rewrite-sync-counter-comparisons.patch
Patch140:   0138-staging-mrst-ossync-Make-flags-parameter-to-PVRSRVCa.patch
Patch141:   0139-staging-mrst-ossync-Use-spin_lock_irq-in-PVRSRVCallb.patch
Patch142:   0140-staging-mrst-ossync-Execute-sync-callbacks-outside-t.patch
Patch143:   0141-staging-mrst-ossync-Avoid-double-kmalloc.patch
Patch144:   0142-staging-mrst-Silence-a-compiler-warning.patch
Patch145:   0143-staging-mrst-psb_gtt-Fix-smatch-warnings.patch
Patch146:   0144-staging-mrst-Remove-an-unused-variable.patch
Patch147:   0145-staging-mrst-ossync-Make-sync_list-and-sync_lock-sta.patch
Patch148:   0146-staging-mrst-Fix-BUG_ON-triggering-in-drm_vblank_put.patch
Patch149:   0147-psb_video-remove-OSPM_GL3_CACHE_ISLAND-when-not-enab.patch
Patch150:   0148-psb_video-remove-MSVDX-firmware-uploading-from-drive.patch
Patch151:   0149-psb_video-implement-reset-function-by-power-up-down-.patch
Patch152:   0150-gfx-pvr-fix-uninitialized-var-bug-on-error-path.patch
Patch153:   0151-gfx-drm-add-missing-header.patch
Patch154:   0152-gfx-drm-ttm-add-support-for-non-swappable-buffers.patch
Patch155:   0153-gfx-imgv-fix-parameter-checking-for-exec-cmd-IOCTL.patch
Patch156:   0154-gfx-imgv-refactor-the-checking-of-buffer-placement.patch
Patch157:   0155-gfx-pvr-add-helper-function-to-lookup-a-pvr-buf-by-i.patch
Patch158:   0156-gfx-imgv-add-memory-backend-support-for-fixed-pages.patch
Patch159:   0157-gfx-imgv-refactor-the-placement-allocator-ioctls.patch
Patch160:   0158-gfx-imgv-add-support-for-wrapping-a-pvr-buffer-as-tt.patch
Patch161:   0159-gfx-drv-Remove-unused-variable.patch
Patch162:   0160-gfx-imgv-Dereference-a-pointer-after-null-check.patch
Patch163:   0161-gfx-pvr-Dereference-a-pointer-after-null-check.patch
Patch164:   0162-gfx-drv-Move-NULL-check-outside-spin-lock.patch
Patch165:   0163-drm-ttm-Fix-clearing-of-highmem-pages.patch
Patch166:   0164-gfx-drv-Remove-useless-global-variable.patch
Patch167:   0165-gfx-drv-Fix-page-flip-lockup-when-requesting-vblank-.patch
Patch168:   0166-gfx-drv-Clean-pending-page-flip-events-when-device-i.patch
Patch169:   0167-gfx-pvr-Fix-SGX-failing-to-complete-queued-rendering.patch
Patch170:   0168-gfx-drv-Clear-links-when-freeing-head-of-list.patch
Patch171:   0169-gfx-display-tc35876x-remove-extra-dsi_device_ready-s.patch
Patch172:   0170-gfx-display-tc35876x-make-mdfld_dsi_configure_-up-do.patch
Patch173:   0171-gfx-display-tc35876x-remove-redundant-switching-of-d.patch
Patch174:   0172-gfx-display-refactor-psb_runtime_-idle-suspend.patch
Patch175:   0173-gfx-display-remove-another-redundant-panel-state-var.patch
Patch176:   0174-gfx-display-allow-powering-down-GL3-cache-regardless.patch
Patch177:   0175-pvr-unifdef-pvr-power-management-code-for-improved-r.patch
Patch178:   0176-gfx-display-remove-redundant-dpi_panel_on-setting.patch
Patch179:   0177-gfx-display-make-tc35876x-independent-of-tmd-vid-dri.patch
Patch180:   0178-gfx-remove-obsolete-.gitignore.patch
Patch181:   0179-gfx-imgv-remove-redundant-include-drm-drm_os_linux.h.patch
Patch182:   0180-Revert-drm-Protect-drm-drm_os_linux.h-inclusion-with.patch
Patch183:   0181-gfx-pvr-remove-unused-code-in-intel-linux-directorie.patch
Patch184:   0182-gfx-display-remove-unused-uopt-user-options.patch
Patch185:   0183-gfx-remove-the-outdated-README.patch
Patch186:   0184-gfx-drv-overlay-Fix-NV12-chroma-SWIDTHSW.patch
Patch187:   0185-MUST_REVERT-gfx-drm-explicitly-authenticate-for-Andr.patch
Patch188:   0186-MUST_REVERT-drm-psb-Added-gralloc-buffer-support-for.patch
Patch189:   0187-gfx-drv-overlay-More-thorough-fix-for-SWIDTHSW-issue.patch
Patch190:   0188-gfx-drv-overlay-Set-CC_OUT-bit-in-OCONFIG.patch
Patch191:   0189-gfx-drv-Add-command-trace-points-to-flip.patch
Patch192:   0190-gfx-drv-Add-trace-events-for-powermanagement.patch
Patch193:   0191-gfx-pvr-fix-corrupted-command-trace-for-SGX-transfer.patch
Patch194:   0192-gfx-pvr-optimize-clearing-sync-counter-trace-info-st.patch
Patch195:   0193-HACK-gfx-drv-when-resuming-make-sure-power-is-on.patch
Patch196:   0194-Make-psb-driver-interface-files-not-world-writable.patch
Patch197:   0195-gfx-display-do-not-spam-tc35876x_brightness_control-.patch
Patch198:   0196-pvr-increase-source-sync-object-limit.patch
Patch199:   0197-imgv-Fix-video-bind-page-management.patch
Patch200:   0198-Addition-of-the-OTM-HDMI-driver-for-Medfield.patch
Patch201:   0199-Enable-the-OTM-HDMI-driver-on-ICS.patch
Patch202:   0200-Enable-hotplug-kernel-handler.patch
Patch203:   0201-MUST_REVERT-Avoid-calling-mode-set-for-MIPI-during-H.patch
Patch204:   0202-Enable-suspend-resume-support-for-HDMI.patch
Patch205:   0203-mode-management-changes-between-local-and-external-d.patch
Patch206:   0204-Fix-intermittent-hotplug-lost-issue-because-of-page-.patch
Patch207:   0205-gfx-drv-Add-register-definitions-for-Chimei-Innolux-.patch
Patch208:   0206-gfx-drv-Move-stuff-around.patch
Patch209:   0207-gfx-drv-Fix-panel-poweron-sleep-value.patch
Patch210:   0208-gfx-drv-Add-support-for-CABC.patch
Patch211:   0209-pvr_debugfs-Fix-inline-function-declaration.patch
Patch212:   0210-gfx-pvr-fix-SGX_READ_HWPERF-IOCTL-according-to-v1.7-.patch
Patch213:   0211-gfx-pvr-check-the-size-of-SGX_READ_HWPERF-IOCTL-para.patch
Patch214:   0212-gfx-pvr-cmd-trace-rename-flip-request-syncobj-names.patch
Patch215:   0213-gfx-drv-pvr-cmd-trace-show-both-old-and-new-flip-req.patch
Patch216:   0214-gfx-display-use-a-bool-parameter-for-force-in-ospm_p.patch
Patch217:   0215-gfx-display-switch-to-the-power-island-management-co.patch
Patch218:   0216-gfx-display-remove-no-op-assignments-in-ospm_power_u.patch
Patch219:   0217-gfx-display-ospm_power_using_hw_begin-rewrite.patch
Patch220:   0218-gfx-display-drop-redundant-ospm_resume_pci-in-ospm_p.patch
Patch221:   0219-gfx-display-remove-redundant-code-in-ospm_power_usin.patch
Patch222:   0220-gfx-display-remove-useless-gb-Suspend-Resume-InProgr.patch
Patch223:   0221-gfx-display-cleanup-ospm_power_suspend.patch
Patch224:   0222-gfx-display-remove-unused-panel_desc-field-and-defin.patch
Patch225:   0223-drv-psb-check-crtc-pointer-before-calling-in-DPMS-an.patch
Patch226:   0224-gfx-drv-Do-not-kfree-pvr-buffer-s-page-list.patch
Patch227:   0225-gfx-drv-psb_gtt.h-fix-indentation.patch
Patch228:   0226-gfx-drv-fix-psb_gtt_-map-unmap-_pvr_memory-interface.patch
Patch229:   0227-gfx-drv-psb_gtt.c-remove-unnecessary-casting-of-psb_.patch
Patch230:   0228-gfx-drv-change-psb_gtt_insert_-to-remove-casting.patch
Patch231:   0229-gfx-drv-psb_gtt.c-make-function-calls-take-as-few-li.patch
Patch232:   0230-gfx-drv-psb_gtt.c-improve-readability-by-using-ERR_P.patch
Patch233:   0231-gfx-drv-psb_gtt.c-replace-printk-with-DRM_DEBUG.patch
Patch234:   0232-gfx-drv-psb_gtt.c-add-spaces-around-comments.patch
Patch235:   0233-gfx-drv-fix-resource-leak-in-psb_gtt_map_meminfo.patch
Patch236:   0234-gfx-gtt-refactor-gtt-mapping-code.patch
Patch237:   0235-gfx-gtt-refcount-gtt-mappings.patch
Patch238:   0236-gfx-drv-pass-meminfo-instead-of-handle-to-psb_gtt_-u.patch
Patch239:   0237-gfx-drv-use-psbfb-pvrBO-instead-of-hKernelMemInfo-on.patch
Patch240:   0238-gfx-drv-don-t-store-meminfo-handle-on-psbfb.patch
Patch241:   0239-gfx-pvr-unifdef-RES_MAN_EXTEND.patch
Patch242:   0240-gfx-pvr-add-a-way-of-getting-the-src-meminfo-backing.patch
Patch243:   0241-gfx-drv-support-creating-fb-s-from-mapped-memory.patch
Patch244:   0242-gfx-pvr-add-an-interface-for-inc-dec-meminfo-ref-cou.patch
Patch245:   0243-gfx-drv-update-meminfo-reference-count-when-creating.patch
Patch246:   0244-gfx-pvr-Remove-most-of-the-display-class-code.patch
Patch247:   0245-gfx-drv-Lock-gPVRSRVLock-mutex-before-touching-the-m.patch
Patch248:   0246-gfx-pvr-Lock-gPVRSRVLock-in-PVRSRVMISR.patch
Patch249:   0247-gfx-drv-Add-PIPE-DSL-regs.patch
Patch250:   0248-gfx-pvr-ossync-Indicate-whether-sync-callback-is-cal.patch
Patch251:   0249-gfx-gtt-Support-unmapping-from-arbitrary-context.patch
Patch252:   0250-gfx-drv-Check-fb-bpp-before-doing-irreversible-chang.patch
Patch253:   0251-gfx-drv-Move-read-ops-sync-counter-functions-into-ps.patch
Patch254:   0252-gfx-drv-Add-some-helper-function-to-manipulate-fb-re.patch
Patch255:   0253-gfx-drv-Introduce-drm_flip-helper-class.patch
Patch256:   0254-gfx-drv-Rewrite-the-CRTC-page-flipping-code-to-use-t.patch
Patch257:   0255-gfx-drv-Avoid-page-flipping-while-too-close-to-vblan.patch
Patch258:   0256-HACK-gfx-display-Double-the-tc35876x-pixel-clock.patch
Patch259:   0257-gfx-overlay-Synchronize-overlay-updates-with-CRTC-pa.patch
Patch260:   0258-gfx-drv-Move-flip-trace-commands-into-psb_fb.patch
Patch261:   0259-gfx-overlay-Optimize-filter-coefficient-load-with-sy.patch
Patch262:   0260-drm-disconnect-plane-from-fb-crtc-when-disabled.patch
Patch263:   0261-gfx-drv-Keep-a-reference-to-pvr-per-process-data.patch
Patch264:   0262-gfx-drv-Clean-up-error-handling-during-fb-creation.patch
Patch265:   0263-gfx-drv-Use-need_gtt-in-psb_fb_gtt_ref-unref.patch
Patch266:   0264-gfx-drv-Free-the-correct-pointer-in-psb_framebuffer_.patch
Patch267:   0265-gfx-drv-Kill-psbfb_vdc_reg.patch
Patch268:   0266-gfx-drv-Fix-locking-in-psb_gtt_-map-unmap-_meminfo_i.patch
Patch269:   0267-gfx-drv-Add-WARN_ON-1-to-psb_gtt_-map-unmap-_meminfo.patch
Patch270:   0268-gfx-drv-Increase-mem-info-ref-count-safely.patch
Patch271:   0269-gfx-drv-Reference-count-cursor-BOs.patch
Patch272:   0270-gfx-drv-Kill-psb_bo_offset.patch
Patch273:   0271-gfx-drv-Warn-if-mode_config.mutex-is-not-locked-in-G.patch
Patch274:   0272-Static-Analysis-fixes-for-OTM-HDMI-driver.patch
Patch275:   0273-Fix-SMATCH-issues-found-in-OTM-HDMI-driver.patch
Patch276:   0274-Cleanup-of-OTM-HDMI-Makefile.patch
Patch277:   0275-Cleanup-of-hooking-OTM-HDMI-driver-into-mrst-Makefil.patch
Patch278:   0276-Fix-SPARSE-issues-found-in-OTM-HDMI-driver.patch
Patch279:   0277-Fix-compilation-warnings-in-OTM-HDMI-driver.patch
Patch280:   0278-Correct-the-compile-condition-for-command-line-inter.patch
Patch281:   0279-Fix-more-build-warnings-found-in-OTM-HDMI-driver.patch
Patch282:   0280-Port-Panel-fitting-changes-for-HDMI-from-Gingerbread.patch
Patch283:   0281-Cleanup-of-some-hardcoded-values-in-OTM-HDMI-driver.patch
Patch284:   0282-Mark-some-unused-functions-in-code-to-avoid-warnings.patch
Patch285:   0283-MUST_REVERT-Set-overlay-clip-region-for-HDMI-to-be-s.patch
Patch286:   0284-Change-HDMI-I2C-adapter-from-3-to-8.patch
Patch287:   0285-In-encoder-dpms-power-on-display-island-before-acces.patch
Patch288:   0286-During-page-flip-update-the-fb_helper-fbdev-to-that-.patch
Patch289:   0287-Enable-HDMI-Audio-callbacks-from-HDMI-driver.patch
Patch290:   0288-Enable-HDMI-audio-routing-and-signaling-to-user-spac.patch
Patch291:   0289-HDMI-Audio-PHY-should-be-disabled-if-HDMI-plane-is-d.patch
Patch292:   0290-Add-DVI-interoperability-support-for-HDMI.patch
Patch293:   0291-Lack-of-MTX_CMDID_NULL-causes-topaz-fence-to-timeout.patch
Patch294:   0292-drv-tc35876x-pass-the-DRM-device-to-all-tc35876x-fun.patch
Patch295:   0293-drv-tc35876x-set-the-brightness-only-when-the-panel-.patch
Patch296:   0294-drv-tc35876x-add-a-named-module-parameter-for-regist.patch
Patch297:   0295-drv-gfx-do-not-use-KERN_ALERT-for-debug-messages.patch
Patch298:   0296-gfx-drv-avoid-NULL-deref-when-enabling-PSB-debug-out.patch
Patch299:   0297-drv-psb-print-useful-values-in-the-PSB-IRQ-debug-mac.patch
Patch300:   0298-gfx-display-initialize-backlight-PWM-frequency.patch
Patch301:   0299-Correct-the-VHDMI-values-inline-to-OTM-HDMI-values.patch
Patch302:   0300-Resume-HDMI-audio-after-suspend.patch
Patch303:   0301-MUST-REVERT-Fix-the-video-slowness-issue-when-unplug.patch
Patch304:   0302-Patch-to-mitigate-HDMI-Suspend-Resume-crash-noise-du.patch
Patch305:   0303-enable-parsing-detailed-timings-for-EDID-rev1.3.patch
Patch306:   0304-Fix-invalid-pointer-reference-in-BUFER_UNDERRUN-call.patch
Patch307:   0305-gfx-drv-Fix-unlikely-race-condition.patch
Patch308:   0306-gfx-pvr-Add-an-extended-sync-callback-API.patch
Patch309:   0307-gfx-pvr-Expose-helper-macro-to-compare-synchronizati.patch
Patch310:   0308-gfx-drv-Fix-race-between-SGX-and-page-flip.patch
Patch311:   0309-gfx-drv-remove-unused-state-save-restore-functions.patch
Patch312:   0310-gfx-drv-remove-unused-mdfld_wait_for_PIPEA_DISABLE.patch
Patch313:   0311-gfx-drv-move-macro-to-check-pipe-validity-to-psb_int.patch
Patch314:   0312-gfx-drv-add-pipe-specific-macros-to-access-panel-tim.patch
Patch315:   0313-gfx-drv-use-pipe-specific-macros-to-access-panel-tim.patch
Patch316:   0314-gfx-drv-remove-old-panel-timing-register-macros.patch
Patch317:   0315-gfx-drv-add-pipe-specific-macros-to-access-the-dsp-p.patch
Patch318:   0316-gfx-drv-use-pipe-specific-macros-to-access-dsp-regs-.patch
Patch319:   0317-gfx-drv-use-pipe-specific-macros-to-access-the-dsp-r.patch
Patch320:   0318-gfx-drv-use-pipe-specific-macros-to-access-the-palet.patch
Patch321:   0319-gfx-drv-save-pipe-specific-panel-timing-regs-to-an-a.patch
Patch322:   0320-gfx-drv-clean-up-the-save-restore-of-the-dspcntr-pip.patch
Patch323:   0321-gfx-drv-clean-up-the-save-restore-of-the-palette-reg.patch
Patch324:   0322-gfx-drv-remove-old-dsp-palette-reg-macros.patch
Patch325:   0323-gfx-drv-fix-exiting-DSI-ULPS-mode-during-resume.patch
Patch326:   0324-gfx-drv-add-helper-to-wait-for-HW-flag-becoming-set-.patch
Patch327:   0325-gfx-drv-cleanup-dsi-pll-lock-loop-in-mdfld_restore_d.patch
Patch328:   0326-gfx-drv-save-the-pfit-hdmi-mipi-regs-along-with-the-.patch
Patch329:   0327-gfx-drv-clean-up-the-save-restore-of-the-fp-mipi-dpl.patch
Patch330:   0328-gfx-drv-rename-PLL-PLL-DIV-registers-according-to-th.patch
Patch331:   0329-gfx-drv-give-a-better-name-to-mdfld_-save-restore-_d.patch
Patch332:   0330-gfx-drv-save-restore-gunit-registers.patch
Patch333:   0331-gfx-drv-Fix-double-free-in-page-flip.patch
Patch334:   0332-gfx-pvr-Fix-spinlock-usage-in-ossync-code.patch
Patch335:   0333-HACK-gfx-pvr-Don-t-try-to-complete-SGX-commands-in-a.patch
Patch336:   0334-gfx-hdmi-Enabling-EDID-prints-during-run-time.patch
Patch337:   0335-gfx-hdmi-Convert-API-interface-documentation-to-kern.patch
Patch338:   0336-gfx-hdmi-Bug-fix-for-overflow-buffer-boundaries.patch
Patch339:   0337-Revert-MUST_REVERT-gfx-drm-explicitly-authenticate-f.patch
Patch340:   0338-gfx-display-move-ospm_power_-un-init-functions-to-av.patch
Patch341:   0339-gfx-display-switch-off-GL3-power-island-at-boot-when.patch
Patch342:   0340-gfx-set-power-state.patch
Patch343:   0341-gfx-display-use-regulator-instead-of-gpio-to-power-o.patch
Patch344:   0342-Add-check-reset-function-for-msvdx-firmware.patch
Patch345:   0343-drm-Reject-mode-set-with-current-fb-if-no-current-fb.patch
Patch346:   0344-drm-Change-drm_display_mode-type-to-unsigned.patch
Patch347:   0345-drm-Warn-if-mode-to-umode-conversion-overflows-the-d.patch
Patch348:   0346-drm-Check-crtc-x-and-y-coordinates.patch
Patch349:   0347-drm-Make-drm_mode_attachmode-void.patch
Patch350:   0348-drm-Fix-memory-leak-in-drm_mode_setcrtc.patch
Patch351:   0349-drm-Check-user-mode-against-overflows.patch
Patch352:   0350-drm-Check-CRTC-viewport-against-framebuffer-size.patch
Patch353:   0351-drm-Fix-drm_mode_attachmode_crtc.patch
Patch354:   0352-drm-Make-drm_crtc_convert_-umode-to_umode-static-and.patch
Patch355:   0353-drm-Handle-drm_object_get-failures.patch
Patch356:   0354-drm-Use-a-flexible-array-member-for-blob-property-da.patch
Patch357:   0355-drm-Add-drm_mode_copy.patch
Patch358:   0356-drm-Unify-and-fix-idr-error-handling.patch
Patch359:   0357-gfx-drv-Improve-warning-messages.patch
Patch360:   0358-gfx-drv-Check-framebuffer-depth-with-HDMI.patch
Patch361:   0359-gfx-drv-Check-the-kernel-fb-size-against-the-stolen-.patch
Patch362:   0360-gfx-drv-Correctly-set-info-par.patch
Patch363:   0361-gfx-drv-Don-t-tell-fbdev-about-mmio-regions.patch
Patch364:   0362-gfx-drv-Move-stolen-vram-iounmap-into-gtt-code.patch
Patch365:   0363-gfx-pvr-fix-list-of-supported-FW-version.patch
Patch366:   0364-gfx-pvr-fw_version-should-compare-4-integers-instead.patch
Patch367:   0365-HACK-gfx-Adjust-HDMI-hdisplay-vdisplay-values.patch
Patch368:   0366-gfx-overlay-Clip-the-overlay-correctly.patch
Patch369:   0367-gfx-drv-Avoid-freeing-the-sync-counter-before-comple.patch
Patch370:   0368-gfx-display-remove-redundant-pipe-register-writes-in.patch
Patch371:   0369-gfx-display-tc35876x-disable-flopped-high-speed-tran.patch
Patch372:   0370-gfx-drv-DPST-3.0-kernel-side-support.patch
Patch373:   0371-gfx-Fix-for-HDMI-i2c-operation-not-detected-by-some-.patch
Patch374:   0372-gfx-Support-for-HDMI-repeater-operations.patch
Patch375:   0373-gfx-fix-local-screen-blank-out-with-rapid-hotplug-un.patch
Patch376:   0374-gfx-change-HDMI-modes-from-Android-application.patch
Patch377:   0375-gfx-CABC-Setting-changes.patch
Patch378:   0376-gfx-drv-Fix-swap-interval-0-page-flipping.patch
Patch379:   0377-gfx-drv-Clear-all-pending-flips-when-a-pipe-is-being.patch
Patch380:   0378-PVR-hybrid-atomisp-Makefile-fixes.patch
Patch381:   0379-PVR-hybrid-atomisp-build-fixes.patch
Patch382:   0380-PVR-hybrid-build-fixes.patch
Patch383:   0381-TMD-6x10-merge-MCG-display-panel-code-onto-OTC-pvr-d.patch
Patch384:   0382-TMD-6x10-fixes-to-OTC-side-of-the-MCG-display-panel-.patch
Patch385:   0383-TMD-6x10-fixes-to-MCG-side-of-the-MCG-display-panel-.patch
Patch386:   0384-TMD-6x10-merge-more-crtc-functions-into-otc-pvr-gfx-.patch
Patch387:   0385-staging-msvdx-remove-unused-mb-concealment-support.patch
Patch388:   0386-staging-imgv-remove-dead-code.patch
Patch389:   0387-staging-imgv-remove-user-buffer-ttm-wrapping-support.patch
Patch390:   0388-staging-imgv-remove-support-for-binding-gfx-buffers.patch
Patch391:   0389-staging-gfx-support-for-checking-for-tablet-platform.patch
Patch392:   0390-staging-gfx-introduce-new-driver-private-drm-frame-p.patch
Patch393:   0391-staging-bc_video-remove-unused-mem-alloc-and-camera-.patch
Patch394:   0392-staging-msvdx-remove-unused-support-for-rar-offset.patch
Patch395:   0393-staging-msvdx-remove-unused-header-inclusion.patch
Patch396:   0394-staging-imgv-mmu-reduce-scope-for-implementation-det.patch
Patch397:   0395-staging-topaz-remove-unused-shadow-registers.patch
Patch398:   0396-staging-topaz-reduce-polling-frequency-in-register-r.patch
Patch399:   0397-staging-topaz-fix-mtx-data-size-calculation.patch
Patch400:   0398-staging-msvdx-support-for-D0-and-non-DO-reset-sequen.patch
Patch401:   0399-staging-imgv-delay-fence-timeout.patch
Patch402:   0400-staging-topaz-check-if-hw-is-idle-based-on-command-f.patch
Patch403:   0401-staging-topaz-schedule-hw-suspension-on-timeout.patch
Patch404:   0402-staging-topaz-dbg-logging-for-timeout.patch
Patch405:   0403-staging-topaz-check-if-hw-is-stuck.patch
Patch406:   0404-staging-topaz-do-not-mark-mtx-saved-if-driver-is-not.patch
Patch407:   0405-staging-imgv-ttm-remove-restricted-access-region-sup.patch
Patch408:   0406-staging-imgv-ttm-remove-local-proto-for-buffer-class.patch
Patch409:   0407-staging-imgv-ttm-replace-buffer-creation-with-latest.patch
Patch410:   0408-staging-topaz-rewrite-hw-reset-logic.patch
Patch411:   0409-staging-msvdx-reduce-polling-frequency-in-register-r.patch
Patch412:   0410-staging-msvdx-use-ospm-to-determine-pm-state.patch
Patch413:   0411-staging-msvdx-upload-firmware-using-dma-as-part-of-f.patch
Patch414:   0412-staging-msvdx-rewrite-hw-reset-logic.patch
Patch415:   0413-staging-msvdx-check-context-type-before-resetting.patch
Patch416:   0414-staging-msvdx-remove-explicit-delay-after-data-submi.patch
Patch417:   0415-staging-msvdx-remove-otc-hdmi-support.patch
Patch418:   0416-staging-topaz-add-support-for-bias-table.patch
Patch419:   0417-staging-msvdx-support-for-non-DO-firmware.patch
Patch420:   0418-staging-msvdx-add-query-for-active-hw-video-entry.patch
Patch421:   0419-staging-msvdx-hdmi-support.patch
Patch422:   0420-Tizen-Revert-PORT-FROM-R2-remove-depmod-from-build.patch
Patch423:   0421-Backport-SMACK-changes-from-3.3-to-3.0.patch
Patch424:   0422-config-tizen-base-from-MCG-WW19-release.patch
Patch425:   0423-config-tizen-disable-HDMI-audio.patch
Patch426:   0424-config-tizen-enable-PVR-debug-and-command-tracing.patch
Patch427:   0425-config-tizen-enable-smack.patch
Patch428:   0426-config-tizen-tizen-networking-options.patch
Patch429:   0427-config-tizen-miscellanous-config-changes.patch
Patch430:   0428-Fix-compilation-when-ANDROID_PARANOID_NET-is-disable.patch

BuildRoot: %{_tmppath}/kernel-%{KVERREL}-root


#
# This macro creates a kernel-<subpackage>-devel package.
#	%%kernel_devel_package <subpackage> <pretty-name>
#
%define kernel_devel_package() \
%package -n kernel-%{?1:%{1}-}devel\
Summary: Development package for building kernel modules to match the %{?2:%{2} }kernel\
Group: System/Kernel\
Provides: kernel%{?1:-%{1}}-devel = %{version}-%{release}\
Provides: kernel-devel = %{version}-%{release}%{?1:-%{1}}\
Provides: kernel-devel = %{version}-%{release}%{?1:-%{1}}\
Provides: kernel-devel-uname-r = %{KVERREL}%{?1:-%{1}}\
Requires: kernel%{?1:-%{1}} = %{version}-%{release}\
Requires: hardlink \
AutoReqProv: no\
Requires(pre): /usr/bin/find\
%description -n kernel%{?1:-%{1}}-devel\
This package provides kernel headers and makefiles sufficient to build modules\
against the %{?2:%{2} }kernel package.\
%{nil}

#
# This macro creates a kernel-<subpackage> and its -devel too.
#	%%define variant_summary The Linux kernel compiled for <configuration>
#	%%kernel_variant_package [-n <pretty-name>] <subpackage>
#
%define kernel_variant_package(n:) \
%package -n kernel-%1\
Summary: %{variant_summary}\
Group: System/Kernel\
%kernel_reqprovconf\
%{nil}


%define variant_summary Kernel for PC compatible systems
%kernel_devel_package adaptation-bb
%description -n kernel-adaptation-bb
This package contains the kernel optimized for BlackBay PR3


%prep

# First we unpack the kernel tarball.
# If this isn't the first make prep, we use links to the existing clean tarball
# which speeds things up quite a bit.

# Update to latest upstream.
%if 0%{?released_kernel}
%define vanillaversion 2.6.%{base_sublevel}
# released_kernel with stable_update available case
%if 0%{?stable_update}
%define vanillaversion 2.6.%{base_sublevel}.%{stable_update}
%endif
# non-released_kernel case
%else
%if 0%{?rcrev}
%define vanillaversion 2.6.%{upstream_sublevel}-rc%{rcrev}
%endif
%else
# pre-{base_sublevel+1}-rc1 case
%endif


#
# Unpack the kernel tarbal
#
%setup -q -n linux-%{kversion}

#
# Unpack TI wifi driver and copy&patch its builder script
#
%setup -q -T -D -a 1 -n linux-%{kversion}
install -m755 %{SOURCE2} .
%patch2 -p0

#
# The add an -rc patch if needed
#
%if 0%{?rcrev}
# patch-2.6.%{upstream_sublevel}-rc%{rcrev}.bz2
%patch00 -p1
%endif
%if 0%{?stable_update}
# patch-2.6.%{base_sublevel}.%{stable_update}.bz2
%patch00 -p1
%endif

#
# MCG kernel diff
#
%patch1 -p1

#####################################################################
#
# All other patches
#

# 0001-CFLAGS-fix-for-Tizen.patch
%patch3 -p1
# 0002-Enable-proc-fs-to-print-more-than-32-groups-entries.patch
%patch4 -p1
# 0003-PVR-hybrid-rm-drivers-staging-mrst-from-MCG-kernel.patch
%patch5 -p1
# 0004-PVR-driver.patch
%patch6 -p1
# 0005-gfx-pvr-add-missing-1.7-IOCTL-IDs.patch
%patch7 -p1
# 0006-gfx-tc35876x-fix-i2c-driver-device-name-mismatch.patch
%patch8 -p1
# 0007-gfx-tc35876x-don-t-register-the-device-ufi-does-it-a.patch
%patch9 -p1
# 0008-gfx-pvr-keep-around-IOCTL-names-even-for-release-bui.patch
%patch10 -p1
# 0009-gfx-pvr-add-missing-1.7-ukernel-commands.patch
%patch11 -p1
# 0010-gfx-pvr-increase-SGX_MAX_INIT_MEM_HANDLES-per-1.7.patch
%patch12 -p1
# 0011-gfx-pvr-fix-SGX_BRIDGE_INIT_INFO-per-1.7.patch
%patch13 -p1
# 0012-gfx-display-force-panel-type.patch
%patch14 -p1
# 0013-gfx-fix-MDFD_GL3-makefile-logic.patch
%patch15 -p1
# 0014-gfx-drv-add-missing-mdfld_gl3-header.patch
%patch16 -p1
# 0015-gfx-drv-make-page-flip-work-on-fb-s-with-pvrBO-null.patch
%patch17 -p1
# 0016-Remove-the-reference-to-the-moorestown-directory.patch
%patch18 -p1
# 0017-gfx-drv-tc35876x-fix-input-muxing-for-dv1.patch
%patch19 -p1
# 0018-gfx-pvr-rename-DEBUG-to-PVR_DEBUG_EXT.patch
%patch20 -p1
# 0019-gfx-pvr-fix-clock-enabling-per-1.7.patch
%patch21 -p1
# 0020-gfx-pvr-fix-SGX-get-misc-info-ABI.patch
%patch22 -p1
# 0021-gfx-pvr-Split-3D-paramter-heap-to-shared-and-per-con.patch
%patch23 -p1
# 0022-gfx-pvr-Update-heap-address-to-match-1.7-DDK.patch
%patch24 -p1
# 0023-gfx-pvr-fix-typo-in-PVRSRV_BRIDGE_CHG_DEV_MEM_ATTRIB.patch
%patch25 -p1
# 0024-gfx-pvr-fix-SGX-KICK-IOCTL-param-struct.patch
%patch26 -p1
# 0025-gfx-pvr-add-missing-IOCTLs.patch
%patch27 -p1
# 0026-gfx-pvr-Keep-required-drm-pos-close-function.patch
%patch28 -p1
# 0027-gfx-pvr-enable-workqueues.patch
%patch29 -p1
# 0028-gfx-pvr-change-map-export_devmem-to-map-export_devme.patch
%patch30 -p1
# 0029-gfx-pvr-fix-sgx-transfer-kick-IOCTLs.patch
%patch31 -p1
# 0030-gfx-pvr-annotate-IOCTL-IDs.patch
%patch32 -p1
# 0031-gfx-pvr-make-DoQuerySyncOpsSatisfied-accept-counter-.patch
%patch33 -p1
# 0032-gfx-pvr-add-PVRSRVSyncOps-TakeToken-FlushToToken.patch
%patch34 -p1
# 0033-gfx-pvr-Implement-ioctl-return-for-devinitpart2.patch
%patch35 -p1
# 0034-gfx-pvr-Update-add-shared-parameter-buffer-ioctl.patch
%patch36 -p1
# 0035-gfx-pvr-Implement-uKernel-assert-fail-status-variabl.patch
%patch37 -p1
# 0036-gfx-pvr-Add-host-control-variable-to-match-uKernel.patch
%patch38 -p1
# 0037-gfx-pvr-Remove-unused-variable-from-uKernel-struct.patch
%patch39 -p1
# 0038-gfx-pvr-add-force-cleanup-param.patch
%patch40 -p1
# 0039-gfx-pvr-Update-error-return-codes-to-match-1.7.patch
%patch41 -p1
# 0040-gfx-pvr-indicate-presence-of-cache-op-in-misc-info.patch
%patch42 -p1
# 0041-gfx-drv-tc35876x-fix-bridge-and-panel-GPIO-numbers-a.patch
%patch43 -p1
# 0042-gfx-drv-tc35876x-don-t-oops-if-functions-are-called-.patch
%patch44 -p1
# 0043-gfx-pvr-clarify-Kconfig-terms-wrt.-ABI-version-vs.-s.patch
%patch45 -p1
# 0044-gfx-pvr-remove-ABI-dependency-on-firmware-tracing-fe.patch
%patch46 -p1
# 0045-gfx-pvr-add-header-to-track-the-ABI-version.patch
%patch47 -p1
# 0046-gfx-pvr-remove-warning-on-gaps-in-the-IOCTL-ID-range.patch
%patch48 -p1
# 0047-gfx-pvr-reduce-verbosity-of-debug-messages.patch
%patch49 -p1
# 0048-gfx-drv-fix-display-backlight-PWM-duty-cycle-setting.patch
%patch50 -p1
# 0049-gfx-pvr-add-Kconfig-option-to-dump-fw-trace-to-conso.patch
%patch51 -p1
# 0050-gfx-pvr-refactor-fw-state-dumping-code.patch
%patch52 -p1
# 0051-gfx-pvr-refactor-pvr_get_sgx_dev_info.patch
%patch53 -p1
# 0052-gfx-pvr-export-HWRecoveryResetSGX.patch
%patch54 -p1
# 0053-gfx-pvr-add-debugfs-entry-to-reset-sgx.patch
%patch55 -p1
# 0054-gfx-pvr-add-debugfs-entry-to-read-sgx-firmware-trace.patch
%patch56 -p1
# 0055-gfx-drv-tc35876x-Fix-physical-display-size-informati.patch
%patch57 -p1
# 0056-gfx-drv-Fix-mode-after-all-drm-clients-have-exited.patch
%patch58 -p1
# 0057-gfx-pvr-move-core-debugging-functions-to-a-separate-.patch
%patch59 -p1
# 0058-gfx-pvr-make-sure-power-is-on-during-SGX-reset.patch
%patch60 -p1
# 0059-gfx-pvr-add-sgx_-read-write-_reg.patch
%patch61 -p1
# 0060-gfx-pvr-add-sgx_save_registers_no_pwron.patch
%patch62 -p1
# 0061-gfx-pvr-debugfs-replace-test_and_set_bit-with-spinlo.patch
%patch63 -p1
# 0062-gfx-pvr-add-hwrec_debugfs-entries.patch
%patch64 -p1
# 0063-gfx-pvr-fix-regression-in-user-debug-request.patch
%patch65 -p1
# 0064-gfx-pvr-Move-ioctl-number-check-before-first-use.patch
%patch66 -p1
# 0065-gfx-build-driver-using-top-level-Makefile-and-drop-m.patch
%patch67 -p1
# 0066-gfx-hide-false-positive-warnings-include-dir-warning.patch
%patch68 -p1
# 0067-gfx-display-Avoid-NULL-pointer-deference.patch
%patch69 -p1
# 0068-gfx-pvr-reduce-loglevel-of-fw-state-not-available-ms.patch
%patch70 -p1
# 0069-gfx-pvr-make-firmware-trace-output-IMG-compatible.patch
%patch71 -p1
# 0070-gfx-pvr-fix-locking-of-the-firmare-trace-debugfs-ent.patch
%patch72 -p1
# 0071-gfx-display-tc35876x-fix-null-pointer-dereference-in.patch
%patch73 -p1
# 0072-gfx-display-tc35876x-remove-device-creation-hack.patch
%patch74 -p1
# 0073-gfx-display-remove-legacy-pm-interface.patch
%patch75 -p1
# 0074-gfx-display-reduce-the-use-of-global-variables.patch
%patch76 -p1
# 0075-gfx-display-remove-suspicious-runtime-PM-related-cod.patch
%patch77 -p1
# 0076-gfx-display-remove-the-remains-of-unused-procfs-supp.patch
%patch78 -p1
# 0077-gfx-display-remove-unused-module-parameter-rtpm-gfxr.patch
%patch79 -p1
# 0078-gfx-display-fix-and-clean-runtime-PM-code.patch
%patch80 -p1
# 0079-gfx-display-clean-up-PCI-suspend-resume.patch
%patch81 -p1
# 0080-gfx-display-remove-module-and-early-parameter-to-tog.patch
%patch82 -p1
# 0081-gfx-display-drop-gl3-enable-kernel-command-line-and-.patch
%patch83 -p1
# 0082-gfx-display-add-Android-early-suspend-support.patch
%patch84 -p1
# 0083-gfx-display-put-DSI-lanes-to-ULPS-before-disabling-p.patch
%patch85 -p1
# 0084-gfx-display-add-driver-for-CMI-LCD-panel-I2C.patch
%patch86 -p1
# 0085-HACK-gfx-display-add-display-I2C-device.patch
%patch87 -p1
# 0086-gfx-display-switch-panel-power-off-on-when-blanking-.patch
%patch88 -p1
# 0087-gfx-display-tc35876x-soft-reset-the-LCD-controller-a.patch
%patch89 -p1
# 0088-gfx-display-give-the-panel-more-time-to-wake-up-afte.patch
%patch90 -p1
# 0089-gfx-display-reg-and-field-helpers.patch
%patch91 -p1
# 0090-gfx-display-only-change-the-device-ready-bit-don-t-t.patch
%patch92 -p1
# 0091-gfx-display-fix-pipe-plane-enable-disable.patch
%patch93 -p1
# 0092-gfx-display-unconditionally-enable-display.patch
%patch94 -p1
# 0093-gfx-display-use-REG_BIT_WAIT-for-waiting-bits-to-fli.patch
%patch95 -p1
# 0094-gfx-display-don-t-touch-port-control-twice-in-a-row.patch
%patch96 -p1
# 0095-staging-mrst-Return-ERR_PTR-from-fb_create-hook.patch
%patch97 -p1
# 0096-drm-add-plane-support-v3.patch
%patch98 -p1
# 0097-drm-add-an-fb-creation-ioctl-that-takes-a-pixel-form.patch
%patch99 -p1
# 0098-drm-Add-a-missing.patch
%patch100 -p1
# 0099-drm-Redefine-pixel-formats.patch
%patch101 -p1
# 0100-drm-plane-Clear-plane.crtc-and-plane.fb-after-disabl.patch
%patch102 -p1
# 0101-drm-fourcc-Use-__u32-instead-of-u32.patch
%patch103 -p1
# 0102-drm-plane-Check-source-coordinates.patch
%patch104 -p1
# 0103-drm-plane-Check-crtc-coordinates-against-integer-ove.patch
%patch105 -p1
# 0104-drm-plane-Make-formats-parameter-to-drm_plane_init-c.patch
%patch106 -p1
# 0105-drm-plane-Check-that-the-fb-pixel-format-is-supporte.patch
%patch107 -p1
# 0106-drm-Replace-pitch-with-pitches-in-drm_framebuffer.patch
%patch108 -p1
# 0107-drm-Handle-duplicate-FOURCCs.patch
%patch109 -p1
# 0108-drm-Check-that-the-requested-pixel-format-is-valid.patch
%patch110 -p1
# 0109-drm-Add-drm_format_num_planes-utility-function.patch
%patch111 -p1
# 0110-drm-Add-drm_format_plane_cpp-utility-function.patch
%patch112 -p1
# 0111-drm-Add-drm_format_-horz-vert-_chroma_subsampling-ut.patch
%patch113 -p1
# 0112-drm-Add-drm_framebuffer_check-utility-function.patch
%patch114 -p1
# 0113-drm-Add-struct-drm_region-and-assorted-utility-funct.patch
%patch115 -p1
# 0114-drm-Add-drm_calc_-hscale-vscale-utility-functions.patch
%patch116 -p1
# 0115-drm-plane-Add-plane-options-ioctl.patch
%patch117 -p1
# 0116-drm-Add-drm_chroma_phase_offsets-utility-function.patch
%patch118 -p1
# 0117-staging-mrst-Add-alignment-argument-to-psb_gtt_map_p.patch
%patch119 -p1
# 0118-staging-mrst-Use-drm_framebuffer_check.patch
%patch120 -p1
# 0119-staging-mrst-Add-support-for-Medfield-video-overlays.patch
%patch121 -p1
# 0120-staging-mrst-Add-overlay-color-correction-settings.patch
%patch122 -p1
# 0121-staging-mrst-Add-overlay-CSC-matrix-and-chroma-sitin.patch
%patch123 -p1
# 0122-staging-mrst-Add-overlay-color-keying-and-constant-a.patch
%patch124 -p1
# 0123-staging-mrst-Add-overlay-Z-order-support.patch
%patch125 -p1
# 0124-staging-mrst-Need-to-wait-for-overlay-in-set_plane_o.patch
%patch126 -p1
# 0125-staging-mrst-Fix-zorder-handling-while-overlay-updat.patch
%patch127 -p1
# 0126-drm-Install-drm_fourcc.h.patch
%patch128 -p1
# 0127-drm-plane-mutex_unlock-was-missing.patch
%patch129 -p1
# 0128-drm-Fix-__user-sparse-warnings.patch
%patch130 -p1
# 0129-staging-mrst-Fix-error-handling-in-psbfb_create.patch
%patch131 -p1
# 0130-staging-mrst-Remove-dead-code.patch
%patch132 -p1
# 0131-staging-mrst-psb_gtt-Remove-the-rw-semaphore.patch
%patch133 -p1
# 0132-staging-mrst-overlay-Use-set_memory_wc-instead-of-vm.patch
%patch134 -p1
# 0133-staging-mrst-overlay-Use-jiffies-based-timeout-in-ov.patch
%patch135 -p1
# 0134-staging-mrst-overlay-Flush-posted-writes-to-the-OVAD.patch
%patch136 -p1
# 0135-staging-mrst-overlay-Use-double-buffering-for-overla.patch
%patch137 -p1
# 0136-staging-mrst-overlay-Use-msleep-1-in-ovl_wait.patch
%patch138 -p1
# 0137-staging-mrst-ossync-Rewrite-sync-counter-comparisons.patch
%patch139 -p1
# 0138-staging-mrst-ossync-Make-flags-parameter-to-PVRSRVCa.patch
%patch140 -p1
# 0139-staging-mrst-ossync-Use-spin_lock_irq-in-PVRSRVCallb.patch
%patch141 -p1
# 0140-staging-mrst-ossync-Execute-sync-callbacks-outside-t.patch
%patch142 -p1
# 0141-staging-mrst-ossync-Avoid-double-kmalloc.patch
%patch143 -p1
# 0142-staging-mrst-Silence-a-compiler-warning.patch
%patch144 -p1
# 0143-staging-mrst-psb_gtt-Fix-smatch-warnings.patch
%patch145 -p1
# 0144-staging-mrst-Remove-an-unused-variable.patch
%patch146 -p1
# 0145-staging-mrst-ossync-Make-sync_list-and-sync_lock-sta.patch
%patch147 -p1
# 0146-staging-mrst-Fix-BUG_ON-triggering-in-drm_vblank_put.patch
%patch148 -p1
# 0147-psb_video-remove-OSPM_GL3_CACHE_ISLAND-when-not-enab.patch
%patch149 -p1
# 0148-psb_video-remove-MSVDX-firmware-uploading-from-drive.patch
%patch150 -p1
# 0149-psb_video-implement-reset-function-by-power-up-down-.patch
%patch151 -p1
# 0150-gfx-pvr-fix-uninitialized-var-bug-on-error-path.patch
%patch152 -p1
# 0151-gfx-drm-add-missing-header.patch
%patch153 -p1
# 0152-gfx-drm-ttm-add-support-for-non-swappable-buffers.patch
%patch154 -p1
# 0153-gfx-imgv-fix-parameter-checking-for-exec-cmd-IOCTL.patch
%patch155 -p1
# 0154-gfx-imgv-refactor-the-checking-of-buffer-placement.patch
%patch156 -p1
# 0155-gfx-pvr-add-helper-function-to-lookup-a-pvr-buf-by-i.patch
%patch157 -p1
# 0156-gfx-imgv-add-memory-backend-support-for-fixed-pages.patch
%patch158 -p1
# 0157-gfx-imgv-refactor-the-placement-allocator-ioctls.patch
%patch159 -p1
# 0158-gfx-imgv-add-support-for-wrapping-a-pvr-buffer-as-tt.patch
%patch160 -p1
# 0159-gfx-drv-Remove-unused-variable.patch
%patch161 -p1
# 0160-gfx-imgv-Dereference-a-pointer-after-null-check.patch
%patch162 -p1
# 0161-gfx-pvr-Dereference-a-pointer-after-null-check.patch
%patch163 -p1
# 0162-gfx-drv-Move-NULL-check-outside-spin-lock.patch
%patch164 -p1
# 0163-drm-ttm-Fix-clearing-of-highmem-pages.patch
%patch165 -p1
# 0164-gfx-drv-Remove-useless-global-variable.patch
%patch166 -p1
# 0165-gfx-drv-Fix-page-flip-lockup-when-requesting-vblank-.patch
%patch167 -p1
# 0166-gfx-drv-Clean-pending-page-flip-events-when-device-i.patch
%patch168 -p1
# 0167-gfx-pvr-Fix-SGX-failing-to-complete-queued-rendering.patch
%patch169 -p1
# 0168-gfx-drv-Clear-links-when-freeing-head-of-list.patch
%patch170 -p1
# 0169-gfx-display-tc35876x-remove-extra-dsi_device_ready-s.patch
%patch171 -p1
# 0170-gfx-display-tc35876x-make-mdfld_dsi_configure_-up-do.patch
%patch172 -p1
# 0171-gfx-display-tc35876x-remove-redundant-switching-of-d.patch
%patch173 -p1
# 0172-gfx-display-refactor-psb_runtime_-idle-suspend.patch
%patch174 -p1
# 0173-gfx-display-remove-another-redundant-panel-state-var.patch
%patch175 -p1
# 0174-gfx-display-allow-powering-down-GL3-cache-regardless.patch
%patch176 -p1
# 0175-pvr-unifdef-pvr-power-management-code-for-improved-r.patch
%patch177 -p1
# 0176-gfx-display-remove-redundant-dpi_panel_on-setting.patch
%patch178 -p1
# 0177-gfx-display-make-tc35876x-independent-of-tmd-vid-dri.patch
%patch179 -p1
# 0178-gfx-remove-obsolete-.gitignore.patch
%patch180 -p1
# 0179-gfx-imgv-remove-redundant-include-drm-drm_os_linux.h.patch
%patch181 -p1
# 0180-Revert-drm-Protect-drm-drm_os_linux.h-inclusion-with.patch
%patch182 -p1
# 0181-gfx-pvr-remove-unused-code-in-intel-linux-directorie.patch
%patch183 -p1
# 0182-gfx-display-remove-unused-uopt-user-options.patch
%patch184 -p1
# 0183-gfx-remove-the-outdated-README.patch
%patch185 -p1
# 0184-gfx-drv-overlay-Fix-NV12-chroma-SWIDTHSW.patch
%patch186 -p1
# 0185-MUST_REVERT-gfx-drm-explicitly-authenticate-for-Andr.patch
%patch187 -p1
# 0186-MUST_REVERT-drm-psb-Added-gralloc-buffer-support-for.patch
%patch188 -p1
# 0187-gfx-drv-overlay-More-thorough-fix-for-SWIDTHSW-issue.patch
%patch189 -p1
# 0188-gfx-drv-overlay-Set-CC_OUT-bit-in-OCONFIG.patch
%patch190 -p1
# 0189-gfx-drv-Add-command-trace-points-to-flip.patch
%patch191 -p1
# 0190-gfx-drv-Add-trace-events-for-powermanagement.patch
%patch192 -p1
# 0191-gfx-pvr-fix-corrupted-command-trace-for-SGX-transfer.patch
%patch193 -p1
# 0192-gfx-pvr-optimize-clearing-sync-counter-trace-info-st.patch
%patch194 -p1
# 0193-HACK-gfx-drv-when-resuming-make-sure-power-is-on.patch
%patch195 -p1
# 0194-Make-psb-driver-interface-files-not-world-writable.patch
%patch196 -p1
# 0195-gfx-display-do-not-spam-tc35876x_brightness_control-.patch
%patch197 -p1
# 0196-pvr-increase-source-sync-object-limit.patch
%patch198 -p1
# 0197-imgv-Fix-video-bind-page-management.patch
%patch199 -p1
# 0198-Addition-of-the-OTM-HDMI-driver-for-Medfield.patch
%patch200 -p1
# 0199-Enable-the-OTM-HDMI-driver-on-ICS.patch
%patch201 -p1
# 0200-Enable-hotplug-kernel-handler.patch
%patch202 -p1
# 0201-MUST_REVERT-Avoid-calling-mode-set-for-MIPI-during-H.patch
%patch203 -p1
# 0202-Enable-suspend-resume-support-for-HDMI.patch
%patch204 -p1
# 0203-mode-management-changes-between-local-and-external-d.patch
%patch205 -p1
# 0204-Fix-intermittent-hotplug-lost-issue-because-of-page-.patch
%patch206 -p1
# 0205-gfx-drv-Add-register-definitions-for-Chimei-Innolux-.patch
%patch207 -p1
# 0206-gfx-drv-Move-stuff-around.patch
%patch208 -p1
# 0207-gfx-drv-Fix-panel-poweron-sleep-value.patch
%patch209 -p1
# 0208-gfx-drv-Add-support-for-CABC.patch
%patch210 -p1
# 0209-pvr_debugfs-Fix-inline-function-declaration.patch
%patch211 -p1
# 0210-gfx-pvr-fix-SGX_READ_HWPERF-IOCTL-according-to-v1.7-.patch
%patch212 -p1
# 0211-gfx-pvr-check-the-size-of-SGX_READ_HWPERF-IOCTL-para.patch
%patch213 -p1
# 0212-gfx-pvr-cmd-trace-rename-flip-request-syncobj-names.patch
%patch214 -p1
# 0213-gfx-drv-pvr-cmd-trace-show-both-old-and-new-flip-req.patch
%patch215 -p1
# 0214-gfx-display-use-a-bool-parameter-for-force-in-ospm_p.patch
%patch216 -p1
# 0215-gfx-display-switch-to-the-power-island-management-co.patch
%patch217 -p1
# 0216-gfx-display-remove-no-op-assignments-in-ospm_power_u.patch
%patch218 -p1
# 0217-gfx-display-ospm_power_using_hw_begin-rewrite.patch
%patch219 -p1
# 0218-gfx-display-drop-redundant-ospm_resume_pci-in-ospm_p.patch
%patch220 -p1
# 0219-gfx-display-remove-redundant-code-in-ospm_power_usin.patch
%patch221 -p1
# 0220-gfx-display-remove-useless-gb-Suspend-Resume-InProgr.patch
%patch222 -p1
# 0221-gfx-display-cleanup-ospm_power_suspend.patch
%patch223 -p1
# 0222-gfx-display-remove-unused-panel_desc-field-and-defin.patch
%patch224 -p1
# 0223-drv-psb-check-crtc-pointer-before-calling-in-DPMS-an.patch
%patch225 -p1
# 0224-gfx-drv-Do-not-kfree-pvr-buffer-s-page-list.patch
%patch226 -p1
# 0225-gfx-drv-psb_gtt.h-fix-indentation.patch
%patch227 -p1
# 0226-gfx-drv-fix-psb_gtt_-map-unmap-_pvr_memory-interface.patch
%patch228 -p1
# 0227-gfx-drv-psb_gtt.c-remove-unnecessary-casting-of-psb_.patch
%patch229 -p1
# 0228-gfx-drv-change-psb_gtt_insert_-to-remove-casting.patch
%patch230 -p1
# 0229-gfx-drv-psb_gtt.c-make-function-calls-take-as-few-li.patch
%patch231 -p1
# 0230-gfx-drv-psb_gtt.c-improve-readability-by-using-ERR_P.patch
%patch232 -p1
# 0231-gfx-drv-psb_gtt.c-replace-printk-with-DRM_DEBUG.patch
%patch233 -p1
# 0232-gfx-drv-psb_gtt.c-add-spaces-around-comments.patch
%patch234 -p1
# 0233-gfx-drv-fix-resource-leak-in-psb_gtt_map_meminfo.patch
%patch235 -p1
# 0234-gfx-gtt-refactor-gtt-mapping-code.patch
%patch236 -p1
# 0235-gfx-gtt-refcount-gtt-mappings.patch
%patch237 -p1
# 0236-gfx-drv-pass-meminfo-instead-of-handle-to-psb_gtt_-u.patch
%patch238 -p1
# 0237-gfx-drv-use-psbfb-pvrBO-instead-of-hKernelMemInfo-on.patch
%patch239 -p1
# 0238-gfx-drv-don-t-store-meminfo-handle-on-psbfb.patch
%patch240 -p1
# 0239-gfx-pvr-unifdef-RES_MAN_EXTEND.patch
%patch241 -p1
# 0240-gfx-pvr-add-a-way-of-getting-the-src-meminfo-backing.patch
%patch242 -p1
# 0241-gfx-drv-support-creating-fb-s-from-mapped-memory.patch
%patch243 -p1
# 0242-gfx-pvr-add-an-interface-for-inc-dec-meminfo-ref-cou.patch
%patch244 -p1
# 0243-gfx-drv-update-meminfo-reference-count-when-creating.patch
%patch245 -p1
# 0244-gfx-pvr-Remove-most-of-the-display-class-code.patch
%patch246 -p1
# 0245-gfx-drv-Lock-gPVRSRVLock-mutex-before-touching-the-m.patch
%patch247 -p1
# 0246-gfx-pvr-Lock-gPVRSRVLock-in-PVRSRVMISR.patch
%patch248 -p1
# 0247-gfx-drv-Add-PIPE-DSL-regs.patch
%patch249 -p1
# 0248-gfx-pvr-ossync-Indicate-whether-sync-callback-is-cal.patch
%patch250 -p1
# 0249-gfx-gtt-Support-unmapping-from-arbitrary-context.patch
%patch251 -p1
# 0250-gfx-drv-Check-fb-bpp-before-doing-irreversible-chang.patch
%patch252 -p1
# 0251-gfx-drv-Move-read-ops-sync-counter-functions-into-ps.patch
%patch253 -p1
# 0252-gfx-drv-Add-some-helper-function-to-manipulate-fb-re.patch
%patch254 -p1
# 0253-gfx-drv-Introduce-drm_flip-helper-class.patch
%patch255 -p1
# 0254-gfx-drv-Rewrite-the-CRTC-page-flipping-code-to-use-t.patch
%patch256 -p1
# 0255-gfx-drv-Avoid-page-flipping-while-too-close-to-vblan.patch
%patch257 -p1
# 0256-HACK-gfx-display-Double-the-tc35876x-pixel-clock.patch
%patch258 -p1
# 0257-gfx-overlay-Synchronize-overlay-updates-with-CRTC-pa.patch
%patch259 -p1
# 0258-gfx-drv-Move-flip-trace-commands-into-psb_fb.patch
%patch260 -p1
# 0259-gfx-overlay-Optimize-filter-coefficient-load-with-sy.patch
%patch261 -p1
# 0260-drm-disconnect-plane-from-fb-crtc-when-disabled.patch
%patch262 -p1
# 0261-gfx-drv-Keep-a-reference-to-pvr-per-process-data.patch
%patch263 -p1
# 0262-gfx-drv-Clean-up-error-handling-during-fb-creation.patch
%patch264 -p1
# 0263-gfx-drv-Use-need_gtt-in-psb_fb_gtt_ref-unref.patch
%patch265 -p1
# 0264-gfx-drv-Free-the-correct-pointer-in-psb_framebuffer_.patch
%patch266 -p1
# 0265-gfx-drv-Kill-psbfb_vdc_reg.patch
%patch267 -p1
# 0266-gfx-drv-Fix-locking-in-psb_gtt_-map-unmap-_meminfo_i.patch
%patch268 -p1
# 0267-gfx-drv-Add-WARN_ON-1-to-psb_gtt_-map-unmap-_meminfo.patch
%patch269 -p1
# 0268-gfx-drv-Increase-mem-info-ref-count-safely.patch
%patch270 -p1
# 0269-gfx-drv-Reference-count-cursor-BOs.patch
%patch271 -p1
# 0270-gfx-drv-Kill-psb_bo_offset.patch
%patch272 -p1
# 0271-gfx-drv-Warn-if-mode_config.mutex-is-not-locked-in-G.patch
%patch273 -p1
# 0272-Static-Analysis-fixes-for-OTM-HDMI-driver.patch
%patch274 -p1
# 0273-Fix-SMATCH-issues-found-in-OTM-HDMI-driver.patch
%patch275 -p1
# 0274-Cleanup-of-OTM-HDMI-Makefile.patch
%patch276 -p1
# 0275-Cleanup-of-hooking-OTM-HDMI-driver-into-mrst-Makefil.patch
%patch277 -p1
# 0276-Fix-SPARSE-issues-found-in-OTM-HDMI-driver.patch
%patch278 -p1
# 0277-Fix-compilation-warnings-in-OTM-HDMI-driver.patch
%patch279 -p1
# 0278-Correct-the-compile-condition-for-command-line-inter.patch
%patch280 -p1
# 0279-Fix-more-build-warnings-found-in-OTM-HDMI-driver.patch
%patch281 -p1
# 0280-Port-Panel-fitting-changes-for-HDMI-from-Gingerbread.patch
%patch282 -p1
# 0281-Cleanup-of-some-hardcoded-values-in-OTM-HDMI-driver.patch
%patch283 -p1
# 0282-Mark-some-unused-functions-in-code-to-avoid-warnings.patch
%patch284 -p1
# 0283-MUST_REVERT-Set-overlay-clip-region-for-HDMI-to-be-s.patch
%patch285 -p1
# 0284-Change-HDMI-I2C-adapter-from-3-to-8.patch
%patch286 -p1
# 0285-In-encoder-dpms-power-on-display-island-before-acces.patch
%patch287 -p1
# 0286-During-page-flip-update-the-fb_helper-fbdev-to-that-.patch
%patch288 -p1
# 0287-Enable-HDMI-Audio-callbacks-from-HDMI-driver.patch
%patch289 -p1
# 0288-Enable-HDMI-audio-routing-and-signaling-to-user-spac.patch
%patch290 -p1
# 0289-HDMI-Audio-PHY-should-be-disabled-if-HDMI-plane-is-d.patch
%patch291 -p1
# 0290-Add-DVI-interoperability-support-for-HDMI.patch
%patch292 -p1
# 0291-Lack-of-MTX_CMDID_NULL-causes-topaz-fence-to-timeout.patch
%patch293 -p1
# 0292-drv-tc35876x-pass-the-DRM-device-to-all-tc35876x-fun.patch
%patch294 -p1
# 0293-drv-tc35876x-set-the-brightness-only-when-the-panel-.patch
%patch295 -p1
# 0294-drv-tc35876x-add-a-named-module-parameter-for-regist.patch
%patch296 -p1
# 0295-drv-gfx-do-not-use-KERN_ALERT-for-debug-messages.patch
%patch297 -p1
# 0296-gfx-drv-avoid-NULL-deref-when-enabling-PSB-debug-out.patch
%patch298 -p1
# 0297-drv-psb-print-useful-values-in-the-PSB-IRQ-debug-mac.patch
%patch299 -p1
# 0298-gfx-display-initialize-backlight-PWM-frequency.patch
%patch300 -p1
# 0299-Correct-the-VHDMI-values-inline-to-OTM-HDMI-values.patch
%patch301 -p1
# 0300-Resume-HDMI-audio-after-suspend.patch
%patch302 -p1
# 0301-MUST-REVERT-Fix-the-video-slowness-issue-when-unplug.patch
%patch303 -p1
# 0302-Patch-to-mitigate-HDMI-Suspend-Resume-crash-noise-du.patch
%patch304 -p1
# 0303-enable-parsing-detailed-timings-for-EDID-rev1.3.patch
%patch305 -p1
# 0304-Fix-invalid-pointer-reference-in-BUFER_UNDERRUN-call.patch
%patch306 -p1
# 0305-gfx-drv-Fix-unlikely-race-condition.patch
%patch307 -p1
# 0306-gfx-pvr-Add-an-extended-sync-callback-API.patch
%patch308 -p1
# 0307-gfx-pvr-Expose-helper-macro-to-compare-synchronizati.patch
%patch309 -p1
# 0308-gfx-drv-Fix-race-between-SGX-and-page-flip.patch
%patch310 -p1
# 0309-gfx-drv-remove-unused-state-save-restore-functions.patch
%patch311 -p1
# 0310-gfx-drv-remove-unused-mdfld_wait_for_PIPEA_DISABLE.patch
%patch312 -p1
# 0311-gfx-drv-move-macro-to-check-pipe-validity-to-psb_int.patch
%patch313 -p1
# 0312-gfx-drv-add-pipe-specific-macros-to-access-panel-tim.patch
%patch314 -p1
# 0313-gfx-drv-use-pipe-specific-macros-to-access-panel-tim.patch
%patch315 -p1
# 0314-gfx-drv-remove-old-panel-timing-register-macros.patch
%patch316 -p1
# 0315-gfx-drv-add-pipe-specific-macros-to-access-the-dsp-p.patch
%patch317 -p1
# 0316-gfx-drv-use-pipe-specific-macros-to-access-dsp-regs-.patch
%patch318 -p1
# 0317-gfx-drv-use-pipe-specific-macros-to-access-the-dsp-r.patch
%patch319 -p1
# 0318-gfx-drv-use-pipe-specific-macros-to-access-the-palet.patch
%patch320 -p1
# 0319-gfx-drv-save-pipe-specific-panel-timing-regs-to-an-a.patch
%patch321 -p1
# 0320-gfx-drv-clean-up-the-save-restore-of-the-dspcntr-pip.patch
%patch322 -p1
# 0321-gfx-drv-clean-up-the-save-restore-of-the-palette-reg.patch
%patch323 -p1
# 0322-gfx-drv-remove-old-dsp-palette-reg-macros.patch
%patch324 -p1
# 0323-gfx-drv-fix-exiting-DSI-ULPS-mode-during-resume.patch
%patch325 -p1
# 0324-gfx-drv-add-helper-to-wait-for-HW-flag-becoming-set-.patch
%patch326 -p1
# 0325-gfx-drv-cleanup-dsi-pll-lock-loop-in-mdfld_restore_d.patch
%patch327 -p1
# 0326-gfx-drv-save-the-pfit-hdmi-mipi-regs-along-with-the-.patch
%patch328 -p1
# 0327-gfx-drv-clean-up-the-save-restore-of-the-fp-mipi-dpl.patch
%patch329 -p1
# 0328-gfx-drv-rename-PLL-PLL-DIV-registers-according-to-th.patch
%patch330 -p1
# 0329-gfx-drv-give-a-better-name-to-mdfld_-save-restore-_d.patch
%patch331 -p1
# 0330-gfx-drv-save-restore-gunit-registers.patch
%patch332 -p1
# 0331-gfx-drv-Fix-double-free-in-page-flip.patch
%patch333 -p1
# 0332-gfx-pvr-Fix-spinlock-usage-in-ossync-code.patch
%patch334 -p1
# 0333-HACK-gfx-pvr-Don-t-try-to-complete-SGX-commands-in-a.patch
%patch335 -p1
# 0334-gfx-hdmi-Enabling-EDID-prints-during-run-time.patch
%patch336 -p1
# 0335-gfx-hdmi-Convert-API-interface-documentation-to-kern.patch
%patch337 -p1
# 0336-gfx-hdmi-Bug-fix-for-overflow-buffer-boundaries.patch
%patch338 -p1
# 0337-Revert-MUST_REVERT-gfx-drm-explicitly-authenticate-f.patch
%patch339 -p1
# 0338-gfx-display-move-ospm_power_-un-init-functions-to-av.patch
%patch340 -p1
# 0339-gfx-display-switch-off-GL3-power-island-at-boot-when.patch
%patch341 -p1
# 0340-gfx-set-power-state.patch
%patch342 -p1
# 0341-gfx-display-use-regulator-instead-of-gpio-to-power-o.patch
%patch343 -p1
# 0342-Add-check-reset-function-for-msvdx-firmware.patch
%patch344 -p1
# 0343-drm-Reject-mode-set-with-current-fb-if-no-current-fb.patch
%patch345 -p1
# 0344-drm-Change-drm_display_mode-type-to-unsigned.patch
%patch346 -p1
# 0345-drm-Warn-if-mode-to-umode-conversion-overflows-the-d.patch
%patch347 -p1
# 0346-drm-Check-crtc-x-and-y-coordinates.patch
%patch348 -p1
# 0347-drm-Make-drm_mode_attachmode-void.patch
%patch349 -p1
# 0348-drm-Fix-memory-leak-in-drm_mode_setcrtc.patch
%patch350 -p1
# 0349-drm-Check-user-mode-against-overflows.patch
%patch351 -p1
# 0350-drm-Check-CRTC-viewport-against-framebuffer-size.patch
%patch352 -p1
# 0351-drm-Fix-drm_mode_attachmode_crtc.patch
%patch353 -p1
# 0352-drm-Make-drm_crtc_convert_-umode-to_umode-static-and.patch
%patch354 -p1
# 0353-drm-Handle-drm_object_get-failures.patch
%patch355 -p1
# 0354-drm-Use-a-flexible-array-member-for-blob-property-da.patch
%patch356 -p1
# 0355-drm-Add-drm_mode_copy.patch
%patch357 -p1
# 0356-drm-Unify-and-fix-idr-error-handling.patch
%patch358 -p1
# 0357-gfx-drv-Improve-warning-messages.patch
%patch359 -p1
# 0358-gfx-drv-Check-framebuffer-depth-with-HDMI.patch
%patch360 -p1
# 0359-gfx-drv-Check-the-kernel-fb-size-against-the-stolen-.patch
%patch361 -p1
# 0360-gfx-drv-Correctly-set-info-par.patch
%patch362 -p1
# 0361-gfx-drv-Don-t-tell-fbdev-about-mmio-regions.patch
%patch363 -p1
# 0362-gfx-drv-Move-stolen-vram-iounmap-into-gtt-code.patch
%patch364 -p1
# 0363-gfx-pvr-fix-list-of-supported-FW-version.patch
%patch365 -p1
# 0364-gfx-pvr-fw_version-should-compare-4-integers-instead.patch
%patch366 -p1
# 0365-HACK-gfx-Adjust-HDMI-hdisplay-vdisplay-values.patch
%patch367 -p1
# 0366-gfx-overlay-Clip-the-overlay-correctly.patch
%patch368 -p1
# 0367-gfx-drv-Avoid-freeing-the-sync-counter-before-comple.patch
%patch369 -p1
# 0368-gfx-display-remove-redundant-pipe-register-writes-in.patch
%patch370 -p1
# 0369-gfx-display-tc35876x-disable-flopped-high-speed-tran.patch
%patch371 -p1
# 0370-gfx-drv-DPST-3.0-kernel-side-support.patch
%patch372 -p1
# 0371-gfx-Fix-for-HDMI-i2c-operation-not-detected-by-some-.patch
%patch373 -p1
# 0372-gfx-Support-for-HDMI-repeater-operations.patch
%patch374 -p1
# 0373-gfx-fix-local-screen-blank-out-with-rapid-hotplug-un.patch
%patch375 -p1
# 0374-gfx-change-HDMI-modes-from-Android-application.patch
%patch376 -p1
# 0375-gfx-CABC-Setting-changes.patch
%patch377 -p1
# 0376-gfx-drv-Fix-swap-interval-0-page-flipping.patch
%patch378 -p1
# 0377-gfx-drv-Clear-all-pending-flips-when-a-pipe-is-being.patch
%patch379 -p1
# 0378-PVR-hybrid-atomisp-Makefile-fixes.patch
%patch380 -p1
# 0379-PVR-hybrid-atomisp-build-fixes.patch
%patch381 -p1
# 0380-PVR-hybrid-build-fixes.patch
%patch382 -p1
# 0381-TMD-6x10-merge-MCG-display-panel-code-onto-OTC-pvr-d.patch
%patch383 -p1
# 0382-TMD-6x10-fixes-to-OTC-side-of-the-MCG-display-panel-.patch
%patch384 -p1
# 0383-TMD-6x10-fixes-to-MCG-side-of-the-MCG-display-panel-.patch
%patch385 -p1
# 0384-TMD-6x10-merge-more-crtc-functions-into-otc-pvr-gfx-.patch
%patch386 -p1
# 0385-staging-msvdx-remove-unused-mb-concealment-support.patch
%patch387 -p1
# 0386-staging-imgv-remove-dead-code.patch
%patch388 -p1
# 0387-staging-imgv-remove-user-buffer-ttm-wrapping-support.patch
%patch389 -p1
# 0388-staging-imgv-remove-support-for-binding-gfx-buffers.patch
%patch390 -p1
# 0389-staging-gfx-support-for-checking-for-tablet-platform.patch
%patch391 -p1
# 0390-staging-gfx-introduce-new-driver-private-drm-frame-p.patch
%patch392 -p1
# 0391-staging-bc_video-remove-unused-mem-alloc-and-camera-.patch
%patch393 -p1
# 0392-staging-msvdx-remove-unused-support-for-rar-offset.patch
%patch394 -p1
# 0393-staging-msvdx-remove-unused-header-inclusion.patch
%patch395 -p1
# 0394-staging-imgv-mmu-reduce-scope-for-implementation-det.patch
%patch396 -p1
# 0395-staging-topaz-remove-unused-shadow-registers.patch
%patch397 -p1
# 0396-staging-topaz-reduce-polling-frequency-in-register-r.patch
%patch398 -p1
# 0397-staging-topaz-fix-mtx-data-size-calculation.patch
%patch399 -p1
# 0398-staging-msvdx-support-for-D0-and-non-DO-reset-sequen.patch
%patch400 -p1
# 0399-staging-imgv-delay-fence-timeout.patch
%patch401 -p1
# 0400-staging-topaz-check-if-hw-is-idle-based-on-command-f.patch
%patch402 -p1
# 0401-staging-topaz-schedule-hw-suspension-on-timeout.patch
%patch403 -p1
# 0402-staging-topaz-dbg-logging-for-timeout.patch
%patch404 -p1
# 0403-staging-topaz-check-if-hw-is-stuck.patch
%patch405 -p1
# 0404-staging-topaz-do-not-mark-mtx-saved-if-driver-is-not.patch
%patch406 -p1
# 0405-staging-imgv-ttm-remove-restricted-access-region-sup.patch
%patch407 -p1
# 0406-staging-imgv-ttm-remove-local-proto-for-buffer-class.patch
%patch408 -p1
# 0407-staging-imgv-ttm-replace-buffer-creation-with-latest.patch
%patch409 -p1
# 0408-staging-topaz-rewrite-hw-reset-logic.patch
%patch410 -p1
# 0409-staging-msvdx-reduce-polling-frequency-in-register-r.patch
%patch411 -p1
# 0410-staging-msvdx-use-ospm-to-determine-pm-state.patch
%patch412 -p1
# 0411-staging-msvdx-upload-firmware-using-dma-as-part-of-f.patch
%patch413 -p1
# 0412-staging-msvdx-rewrite-hw-reset-logic.patch
%patch414 -p1
# 0413-staging-msvdx-check-context-type-before-resetting.patch
%patch415 -p1
# 0414-staging-msvdx-remove-explicit-delay-after-data-submi.patch
%patch416 -p1
# 0415-staging-msvdx-remove-otc-hdmi-support.patch
%patch417 -p1
# 0416-staging-topaz-add-support-for-bias-table.patch
%patch418 -p1
# 0417-staging-msvdx-support-for-non-DO-firmware.patch
%patch419 -p1
# 0418-staging-msvdx-add-query-for-active-hw-video-entry.patch
%patch420 -p1
# 0419-staging-msvdx-hdmi-support.patch
%patch421 -p1
# 0420-Tizen-Revert-PORT-FROM-R2-remove-depmod-from-build.patch
%patch422 -p1
# 0421-Backport-SMACK-changes-from-3.3-to-3.0.patch
%patch423 -p1
# 0422-config-tizen-base-from-MCG-WW19-release.patch
%patch424 -p1
# 0423-config-tizen-disable-HDMI-audio.patch
%patch425 -p1
# 0424-config-tizen-enable-PVR-debug-and-command-tracing.patch
%patch426 -p1
# 0425-config-tizen-enable-smack.patch
%patch427 -p1
# 0426-config-tizen-tizen-networking-options.patch
%patch428 -p1
# 0427-config-tizen-miscellanous-config-changes.patch
%patch429 -p1
# 0428-Fix-compilation-when-ANDROID_PARANOID_NET-is-disable.patch
%patch430 -p1

# Any further pre-build tree manipulations happen here.
chmod +x scripts/checkpatch.pl

#
# We want to run the config checks of all configurations for all architectures always.
# That way, developers immediately found out if they forget to enable not-their-native
# architecture. It's cheap to run anyway.
#

  cp config-tizen .config
  Arch="x86"

make ARCH=$Arch listnewconfig &> /tmp/configs
export conf=`cat /tmp/configs | grep CONFIG | wc -l`
echo CONF is $conf
if [ $conf -gt 0 ]; then
	make ARCH=$Arch listnewconfig  
	exit 1
fi
make ARCH=$Arch oldconfig > /dev/null
cp .config config
#
# get rid of unwanted files resulting from patch fuzz
# (not that we can have any)
#
find . \( -name "*.orig" -o -name "*~" \) -exec rm -f {} \; >/dev/null

cd ..


###
### build
###
%build


cp_vmlinux()
{
  eu-strip --remove-comment -o "$2" "$1"
}

BuildKernel() {
    MakeTarget=$1
    KernelImage=$2
    TargetArch=$3
    Flavour=$4
    InstallName=${5:-vmlinuz}

    # Pick the right config file for the kernel we're building
    DevelDir=/usr/src/kernels/%{KVERREL}${Flavour:+-${Flavour}}

    # When the bootable image is just the ELF kernel, strip it.
    # We already copy the unstripped file into the debuginfo package.
    if [ "$KernelImage" = vmlinux ]; then
      CopyKernel=cp_vmlinux
    else
      CopyKernel=cp
    fi

    KernelVer=%{version}-%{release}${Flavour:+-${Flavour}}
    ExtraVer=%{?rctag}-%{release}${Flavour:+-${Flavour}}
    Arch="x86"


    if [ "$Arch" = "$TargetArch" ]; then
        echo BUILDING A KERNEL FOR ${Flavour} %{_target_cpu}... ${KernelVer}
        echo USING ARCH=$Arch

        # make sure EXTRAVERSION says what we want it to say
        perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = ${ExtraVer}/" Makefile

        # and now to start the build process

        make -s mrproper
        cp config .config

        make -s ARCH=$Arch oldconfig > /dev/null
        make -s CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=$Arch %{?_smp_mflags} $MakeTarget %{?sparse_mflags}
        make -s CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=$Arch %{?_smp_mflags} modules %{?sparse_mflags} || exit 1

        # Start installing the results
        mkdir -p $RPM_BUILD_ROOT/%{image_install_path}
        install -m 644 .config $RPM_BUILD_ROOT/boot/config-$KernelVer
        install -m 644 System.map $RPM_BUILD_ROOT/boot/System.map-$KernelVer
        touch $RPM_BUILD_ROOT/boot/initrd-$KernelVer.img
        if [ -f arch/$Arch/boot/zImage.stub ]; then
          cp arch/$Arch/boot/zImage.stub $RPM_BUILD_ROOT/%{image_install_path}/zImage.stub-$KernelVer || :
        fi
        $CopyKernel $KernelImage \
        		$RPM_BUILD_ROOT/%{image_install_path}/$InstallName-$KernelVer
        chmod 755 $RPM_BUILD_ROOT/%{image_install_path}/$InstallName-$KernelVer

        mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer
        make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install KERNELRELEASE=$KernelVer
        make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT vdso_install KERNELRELEASE=$KernelVer

        #
        # Build TI WLAN (out-of-tree) drivers
        #
        ARCH=$Arch TARGET_TOOLS_PREFIX="" INSTALL_MOD_PATH=$RPM_BUILD_ROOT ./wl12xx-compat-build.sh -c mfld_pr2 %{?_smp_mflags} KERNELRELEASE=$KernelVer

        # And save the headers/makefiles etc for building modules against
        #
        # This all looks scary, but the end result is supposed to be:
        # * all arch relevant include/ files
        # * all Makefile/Kconfig files
        # * all script/ files

        rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/source
        mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        (cd $RPM_BUILD_ROOT/lib/modules/$KernelVer ; ln -s build source)
        # dirs for additional modules per module-init-tools, kbuild/modules.txt
        # first copy everything
        cp --parents `find  -type f -name "Makefile*" -o -name "Kconfig*"` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        cp Module.symvers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        cp System.map $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        if [ -s Module.markers ]; then
          cp Module.markers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        fi
        # then drop all but the needed Makefiles/Kconfig files
        rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Documentation
        rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts
        rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
        cp .config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        cp -a scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        if [ -d arch/%{_arch}/scripts ]; then
          cp -a arch/%{_arch}/scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch} || :
        fi
        if [ -f arch/%{_arch}/*lds ]; then
          cp -a arch/%{_arch}/*lds $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/ || :
        fi
        rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*.o
        rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*/*.o
        cp -a --parents arch/$Arch/include $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
        mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
        cd include
        cp -a acpi asm-generic config crypto drm generated keys linux math-emu media mtd net pcmcia rdma rxrpc scsi sound video trace $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include

        # Make sure the Makefile and version.h have a matching timestamp so that
        # external modules can be built
        touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Makefile $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/version.h
        touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/autoconf.h
        # Copy .config to include/config/auto.conf so "make prepare" is unnecessary.
        cp $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/config/auto.conf
        cd ..

        #
        # save the vmlinux file for kernel debugging into the kernel-*-devel rpm
        #

        cp vmlinux $RPM_BUILD_ROOT/lib/modules/$KernelVer

        find $RPM_BUILD_ROOT/lib/modules/$KernelVer -name "*.ko" -type f >modnames

        # mark modules executable so that strip-to-file can strip them
        xargs --no-run-if-empty chmod u+x < modnames

        # Generate a list of modules for block and networking.

        fgrep /drivers/ modnames | xargs --no-run-if-empty nm -upA |
        sed -n 's,^.*/\([^/]*\.ko\):  *U \(.*\)$,\1 \2,p' > drivers.undef

        collect_modules_list()
        {
          sed -r -n -e "s/^([^ ]+) \\.?($2)\$/\\1/p" drivers.undef |
          LC_ALL=C sort -u > $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.$1
        }

        collect_modules_list networking \
        			 'register_netdev|ieee80211_register_hw|usbnet_probe'
        collect_modules_list block \
        			 'ata_scsi_ioctl|scsi_add_host|blk_init_queue|register_mtd_blktrans'

        # remove files that will be auto generated by depmod at rpm -i time
        for i in alias ccwmap dep ieee1394map inputmap isapnpmap ofmap pcimap seriomap symbols usbmap
        do
          rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.$i
        done

        # Move the devel headers out of the root file system
        mkdir -p $RPM_BUILD_ROOT/usr/src/kernels
        mv $RPM_BUILD_ROOT/lib/modules/$KernelVer/build $RPM_BUILD_ROOT/$DevelDir
        ln -sf ../../..$DevelDir $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    fi
}

###
# DO it...
###

# prepare directories
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/boot

BuildKernel %make_target %kernel_image x86 adaptation-bb

###
### install
###

%define install  %{?_enable_debug_packages:%{?buildsubdir:%{debug_package}}}\
%%install


%install

install -m644 %{SOURCE200} $RPM_BUILD_ROOT/boot/cmdline

rm -rf $RPM_BUILD_ROOT/lib/firmware


###
### clean
###

%clean
rm -rf $RPM_BUILD_ROOT

###
### scripts
###

#
# This macro defines a %%post script for a kernel*-devel package.
#	%%kernel_devel_post <subpackage>
#
%define kernel_devel_post() \
%{expand:%%post -n kernel-%{?1:%{1}-}devel}\
if [ -x /usr/sbin/hardlink ]\
then\
    (cd /usr/src/kernels/%{KVERREL}%{?1:-%{1}} &&\
     /usr/bin/find . -type f | while read f; do\
       hardlink -c /usr/src/kernels/*/$f $f\
     done)\
fi\
%{nil}

# This macro defines a %%posttrans script for a kernel package.
#	%%kernel_variant_posttrans [-v <subpackage>] [-s <s> -r <r>] <mkinitrd-args>
# More text can follow to go at the end of this variant's %%post.
#
%define kernel_variant_posttrans(s:r:v:) \
%{expand:%%posttrans -n kernel-%{?-v*}}\
%{nil}

#
# This macro defines a %%post script for a kernel package and its devel package.
#	%%kernel_variant_post [-v <subpackage>] [-s <s> -r <r>] <mkinitrd-args>
# More text can follow to go at the end of this variant's %%post.
#
%define kernel_variant_post(s:r:v:) \
%{expand:%%kernel_devel_post %{?-v*}}\
%{expand:%%kernel_variant_posttrans %{?-v*}}\
%{expand:%%post -n kernel-%{?-v*}}\
%{nil}

#
# This macro defines a %%preun script for a kernel package.
#	%%kernel_variant_preun <subpackage>
#
%define kernel_variant_preun() \
%{expand:%%preun -n kernel-%{?1}}\
%{nil}


%ifarch %all_x86

%kernel_variant_preun adaptation-bb
%kernel_variant_post -v adaptation-bb

%endif


###
### file lists
###



#
# This macro defines the %%files sections for a kernel package
# and its devel packages.
#	%%kernel_variant_files [-k vmlinux] [-a <extra-files-glob>] [-e <extra-nonbinary>] <condition> <subpackage>
#
%define kernel_variant_files(a:e:k:) \
%ifarch %{1}\
%{expand:%%files -n kernel%{?2:-%{2}}}\
%defattr(-,root,root)\
/%{image_install_path}/%{?-k:%{-k*}}%{!?-k:vmlinuz}-%{KVERREL}%{?2:-%{2}}\
/boot/System.map-%{KVERREL}%{?2:-%{2}}\
#/boot/symvers-%{KVERREL}%{?2:-%{2}}.gz\
/boot/config-%{KVERREL}%{?2:-%{2}}\
/boot/cmdline\
%{?-a:%{-a*}}\
%dir /lib/modules/%{KVERREL}%{?2:-%{2}}\
/lib/modules/%{KVERREL}%{?2:-%{2}}/kernel\
/lib/modules/%{KVERREL}%{?2:-%{2}}/build\
/lib/modules/%{KVERREL}%{?2:-%{2}}/source\
/lib/modules/%{KVERREL}%{?2:-%{2}}/vdso\
/lib/modules/%{KVERREL}%{?2:-%{2}}/updates\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.block\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.devname\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.softdep\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.dep.bin\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.alias.bin\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.symbols.bin\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.networking\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.order\
/lib/modules/%{KVERREL}%{?2:-%{2}}/modules.builtin*\
%ghost /boot/initrd-%{KVERREL}%{?2:-%{2}}.img\
%{?-e:%{-e*}}\
%{expand:%%files -n kernel-%{?2:%{2}-}devel}\
%defattr(-,root,root)\
%verify(not mtime) /usr/src/kernels/%{KVERREL}%{?2:-%{2}}\
/lib/modules/%{KVERREL}%{?2:-%{2}}/vmlinux \
%endif\
%{nil}


%kernel_variant_files %all_x86 adaptation-bb
