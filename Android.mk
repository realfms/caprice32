LOCAL_PATH := $(call my-dir)

###########################
#
# Caprice32 shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := CAP32

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src $(LOCAL_PATH)/../SDL2/include $(LOCAL_PATH)/../libpng

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/*.c))

LOCAL_CFLAGS += \
	-Wall -Wextra \
	-Wdocumentation \
	-Wdocumentation-unknown-command \
	-Wmissing-prototypes \
	-Wunreachable-code-break \
	-Wunneeded-internal-declaration \
	-Wmissing-variable-declarations \
	-Wfloat-conversion \
	-Wshorten-64-to-32 \
	-Wunreachable-code-return

# Warnings we haven't fixed (yet)
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-sign-compare
LOCAL_CFLAGS += -DSDL_COMPAT -DNO_GUI -DWITH_SDL2 -D_ANDROID_LOG_ -D_PRERENDER_BORDER_WORD_ALIGN_
LOCAL_SHARED_LIBRARIES := SDL2 PNG

LOCAL_LDLIBS := -lm -lz -llog

ifeq ($(NDK_DEBUG),1)
    cmd-strip :=
endif

include $(BUILD_SHARED_LIBRARY)

###########################
#
# Caprice32 static library
#
###########################

LOCAL_MODULE := CAP32_static

LOCAL_MODULE_FILENAME := libCAP32

LOCAL_CFLAGS := -DSDL_COMPAT
LOCAL_CFLAGS += -DSDL_COMPAT -DNO_GUI -DWITH_SDL2

LOCAL_LDLIBS := -lm -lz -llog
LOCAL_EXPORT_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/cpufeatures)

