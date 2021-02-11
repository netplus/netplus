
APP_MODULES 		:= wawo
APP_OPTIM 			:= debug

ifdef build
	APP_OPTIM := $(build)
endif

#APP_CFLAGS 		:=
#APP_CPPFLAGS 		+= -fexceptions -frtti


APP_ABI 			:= armeabi-v7a

#5.0,5.1
#4.4 19
APP_PLATFORM 		:= android-19
APP_STL 			:= c++_shared

APP_BUILD_SCRIPT	:= Android.mk


