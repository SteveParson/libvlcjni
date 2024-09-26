LOCAL_PATH := $(call my-dir)

# VLC's buildsystem resulting binaries
include $(CLEAN_VARS)
LOCAL_MODULE := libvlccompat
LOCAL_SRC_FILES := $(VLC_BUILD_DIR)/compat/.libs/libcompat.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAN_VARS)
LOCAL_MODULE := libvlccore
LOCAL_SRC_FILES := $(VLC_BUILD_DIR)/src/.libs/libvlccore.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAN_VARS)
LOCAL_MODULE := libvlc-native
LOCAL_MODULE_FILENAME := libvlc
LOCAL_SRC_FILES := $(VLC_BUILD_DIR)/lib/.libs/libvlc.a
LOCAL_STATIC_LIBRARIES := libvlccore
include $(PREBUILT_STATIC_LIBRARY)

# libvlc build with all its modules
include $(CLEAR_VARS)
LOCAL_MODULE    := libvlc
LOCAL_SRC_FILES := $(VLC_BUILD_DIR)/ndk/libvlcjni-modules.c \
				   $(VLC_BUILD_DIR)/ndk/libvlcjni-symbols.c
LOCAL_LDFLAGS := -L$(VLC_CONTRIB)/lib
LOCAL_LDLIBS := \
    $(VLC_MODULES) \
    $(VLC_BUILD_DIR)/lib/.libs/libvlc.a \
    $(VLC_BUILD_DIR)/src/.libs/libvlccore.a \
    $(VLC_BUILD_DIR)/compat/.libs/libcompat.a \
    $(VLC_CONTRIB_LDFLAGS) \
    -ldl -lz -lm -llog \
    -ljpeg \
    -Wl,-Bsymbolic
LOCAL_CXXFLAGS := -std=c++17
# This duplicates the libvlc* link flags, but it propagates the dependency
# on the native build which is what we want overall
LOCAL_STATIC_LIBRARIES := libvlccore libvlccompat libvlc-native
include $(BUILD_SHARED_LIBRARY)
