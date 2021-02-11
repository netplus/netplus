
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


$(warning APP_OPTIM: $(APP_OPTIM))

LOCAL_MODULE 			:= wawo

WW_LIB_DIR 				:= $(LOCAL_PATH)/../..
WW_LIB_INCLUDE_DIR 		:= $(WW_LIB_DIR)/include
WW_LIB_SOURCE_DIR 		:= $(WW_LIB_DIR)/src

WW_LIB_3RD_DIR			:= $(WW_LIB_DIR)/3rd
WW_LIB_3RD_FILES_DIR	:= $(WW_LIB_3RD_DIR)/botan/src/2.9/gcc4.8.5_armv7 $(WW_LIB_3RD_DIR)/http_parser

WW_LIB_3RD_CPP_FILES 	:= $(WW_LIB_3RD_DIR)/botan/src/2.9/gcc4.8.5_armv7/botan_all.cpp
#WW_LIB_3RD_CPP_FILES 	:= 
WW_LIB_3RD_C_FILES 		:= $(WW_LIB_3RD_DIR)/http_parser/http_parser.c

$(warning $(WW_LIB_3RD_CPP_FILES))
$(warning $(WW_LIB_3RD_C_FILES))


#NDK_PROJECT_PATH := ../
WW_LIB_CPP_FILES 		:=	$(WW_LIB_SOURCE_DIR)/basic.cpp \
							$(WW_LIB_SOURCE_DIR)/exception.cpp \
							$(WW_LIB_SOURCE_DIR)/log/console_logger.cpp \
							$(WW_LIB_SOURCE_DIR)/log/android_log_print.cpp \
							$(WW_LIB_SOURCE_DIR)/log/file_logger.cpp \
							$(WW_LIB_SOURCE_DIR)/log/format_normal.cpp \
							$(WW_LIB_SOURCE_DIR)/log/logger_manager.cpp \
							$(WW_LIB_SOURCE_DIR)/log/sys_logger.cpp \
							$(WW_LIB_SOURCE_DIR)/net/address.cpp \
							$(WW_LIB_SOURCE_DIR)/net/channel_future.cpp \
							$(WW_LIB_SOURCE_DIR)/net/channel_handler.cpp \
							$(WW_LIB_SOURCE_DIR)/net/channel_handler_context.cpp \
							$(WW_LIB_SOURCE_DIR)/net/channel_pipeline.cpp \
							$(WW_LIB_SOURCE_DIR)/net/handler/dh_symmetric_encrypt.cpp \
							$(WW_LIB_SOURCE_DIR)/net/handler/hlen.cpp \
							$(WW_LIB_SOURCE_DIR)/net/handler/http.cpp \
							$(WW_LIB_SOURCE_DIR)/net/handler/mux.cpp \
							$(WW_LIB_SOURCE_DIR)/net/handler/tls.cpp \
							$(WW_LIB_SOURCE_DIR)/net/handler/websocket.cpp \
							$(WW_LIB_SOURCE_DIR)/net/http/client.cpp \
							$(WW_LIB_SOURCE_DIR)/net/http/message.cpp \
							$(WW_LIB_SOURCE_DIR)/net/http/parser.cpp \
							$(WW_LIB_SOURCE_DIR)/net/http/utils.cpp \
							$(WW_LIB_SOURCE_DIR)/net/io_event_loop.cpp \
							$(WW_LIB_SOURCE_DIR)/net/socket.cpp \
							$(WW_LIB_SOURCE_DIR)/net/socket_base.cpp \
							$(WW_LIB_SOURCE_DIR)/net/wcp.cpp \
							$(WW_LIB_SOURCE_DIR)/rpc.cpp \
							$(WW_LIB_SOURCE_DIR)/signal/signal_manager.cpp \
							$(WW_LIB_SOURCE_DIR)/task/runner.cpp \
							$(WW_LIB_SOURCE_DIR)/task/scheduler.cpp \
							$(WW_LIB_SOURCE_DIR)/thread.cpp \
							$(WW_LIB_SOURCE_DIR)/thread_impl/mutex.cpp

$(warning $(WW_LIB_CPP_FILES))


LOCAL_SRC_FILES 		:= $(WW_LIB_3RD_C_FILES) $(WW_LIB_CPP_FILES) $(WW_LIB_3RD_CPP_FILES)


LOCAL_CPP_FEATURES  	:= rtti exceptions

LOCAL_C_INCLUDES 		:= $(WW_LIB_INCLUDE_DIR)

WW_CFLAGS_GENERIC 		:= 

ifeq ($(APP_OPTIM),release)
	WW_CFLAGS_GENERIC += -g0 -DNDEBUG
else
	WW_CFLAGS_GENERIC += -g -Wall -O0 -DDEBUG
endif


LOCAL_CFLAGS 			:= $(WW_CFLAGS_GENERIC) 
LOCAL_CPPFLAGS 			:= $(WW_CFLAGS_GENERIC) -mfpu=neon -std=c++11
LOCAL_LDFLAGS 			:= 

LOCAL_ARM_MODE 			:= arm

include $(BUILD_STATIC_LIBRARY)
