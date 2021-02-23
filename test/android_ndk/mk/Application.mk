
APP_MODULES 		:= ndk_hello_netplus
APP_OPTIM			:= release

ifdef build
	APP_OPTIM := $(build)
endif

APP_ABI := armeabi-v7a

#5.0,5.1
#4.4 9
APP_PLATFORM 		:= android-19
APP_STL 			:= c++_shared

APP_BUILD_SCRIPT	:= Android.mk


