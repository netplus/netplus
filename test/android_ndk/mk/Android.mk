
LOCAL_PATH := $(call my-dir)

NETP_LIB_DIR 				:= $(LOCAL_PATH)/../../..
NETP_LIB_INCLUDE_DIR 		:= $(NETP_LIB_DIR)/include
NETP_LIB_BIN_FILE			:= $(NETP_LIB_DIR)/projects/android-ndk/obj/local/armeabi-v7a/libnetplus.a

include $(CLEAR_VARS)
LOCAL_MODULE          	:= netplus
LOCAL_SRC_FILES 		:= $(NETP_LIB_BIN_FILE)
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE 			:= ndk_hello_netplus



LOCAL_SRC_FILES 		:= ../src/main.cpp

LOCAL_CPP_FEATURES  	:= rtti exceptions

LOCAL_C_INCLUDES 		:= $(NETP_LIB_INCLUDE_DIR)

#NETP_CFLAGS_GENERIC 		:= -g -Wall -O0
NETP_CFLAGS_GENERIC 		:= -Os

ifeq ($(APP_OPTIM),release)
	NETP_CFLAGS_GENERIC += -g0 
else
	NETP_CFLAGS_GENERIC += -g -Wall -O0 -DDEBUG
endif

LOCAL_CFLAGS 			:= $(NETP_CFLAGS_GENERIC) 
LOCAL_CPPFLAGS 			:= $(NETP_CFLAGS_GENERIC) -mfpu=neon -std=c++11


LOCAL_ARM_MODE 			:= arm


LOCAL_WHOLE_STATIC_LIBRARIES += netplus

include $(BUILD_EXECUTABLE)