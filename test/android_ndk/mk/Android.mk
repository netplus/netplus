
LOCAL_PATH := $(call my-dir)

WW_LIB_DIR 				:= $(LOCAL_PATH)/../../..
WW_LIB_INCLUDE_DIR 		:= $(WW_LIB_DIR)/include
WW_LIB_BIN_FILE			:= $(WW_LIB_DIR)/projects/android-ndk/obj/local/armeabi-v7a/libwawo.a

include $(CLEAR_VARS)
LOCAL_MODULE          	:= wawo
LOCAL_SRC_FILES 		:= $(WW_LIB_BIN_FILE)
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE 			:= ndk_hello_wawo



LOCAL_SRC_FILES 		:= ../src/main.cpp

LOCAL_CPP_FEATURES  	:= rtti exceptions

LOCAL_C_INCLUDES 		:= $(WW_LIB_INCLUDE_DIR)

#WW_CFLAGS_GENERIC 		:= -g -Wall -O0
WW_CFLAGS_GENERIC 		:= -Os

ifeq ($(APP_OPTIM),release)
	WW_CFLAGS_GENERIC += -g0 
else
	WW_CFLAGS_GENERIC += -g -Wall -O0 -DDEBUG
endif

LOCAL_CFLAGS 			:= $(WW_CFLAGS_GENERIC) 
LOCAL_CPPFLAGS 			:= $(WW_CFLAGS_GENERIC) -mfpu=neon -std=c++11


LOCAL_ARM_MODE 			:= arm


LOCAL_WHOLE_STATIC_LIBRARIES += wawo

include $(BUILD_EXECUTABLE)