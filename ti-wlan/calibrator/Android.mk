LOCAL_PATH:= $(call my-dir)

#
# Calibrator
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		nvs.c \
		misc_cmds.c \
		calibrator.c \
		plt.c \
		ini.c

LOCAL_CFLAGS := -DCONFIG_LIBNL20
LOCAL_LDFLAGS := -Wl,--no-gc-sections

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	external/libnl-headers

LOCAL_STATIC_LIBRARIES := libnl_2
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := calibrator

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := ProjectLogDef.ili
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := fw_logger/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := iliparser.py
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := fw_logger/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := message.py
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := fw_logger/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := parser.py
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := fw_logger/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#
# UIM Application
#
#include $(CLEAR_VARS)

#LOCAL_C_INCLUDES:= \
#	$(LOCAL_PATH)/uim_rfkill/ \
#	external/bluetooth/bluez/

#LOCAL_SRC_FILES:= \
#	uim_rfkill/uim.c
#LOCAL_CFLAGS:= -g -c -W -Wall -O2 -D_POSIX_SOURCE
#LOCAL_SHARED_LIBRARIES:= libnetutils
#LOCAL_MODULE_TAGS := eng
#LOCAL_MODULE:=uim-util

#include $(BUILD_EXECUTABLE)
