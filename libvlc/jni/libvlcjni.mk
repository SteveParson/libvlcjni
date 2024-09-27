LOCAL_PATH := $(call my-dir)

include $(CLEAN_VARS)
LOCAL_MODULE := libvlc
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libvlc.so
include $(PREBUILT_SHARED_LIBRARY)

# libvlcjni
include $(CLEAR_VARS)
LOCAL_MODULE    := libvlcjni
LOCAL_SRC_FILES := libvlcjni.c
LOCAL_SRC_FILES += libvlcjni-mediaplayer.c
LOCAL_SRC_FILES += libvlcjni-vlcobject.c
LOCAL_SRC_FILES += libvlcjni-media.c libvlcjni-medialist.c libvlcjni-mediadiscoverer.c libvlcjni-rendererdiscoverer.c
LOCAL_SRC_FILES += libvlcjni-dialog.c
LOCAL_SRC_FILES += std_logger.c
LOCAL_C_INCLUDES := $(VLC_SRC_DIR)/include $(VLC_BUILD_DIR)/include
LOCAL_CFLAGS := -std=c17
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES := libvlc

include $(BUILD_SHARED_LIBRARY)
