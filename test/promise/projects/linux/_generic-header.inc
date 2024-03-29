CURRENT_DIR 	:= $(shell pwd)
UNAME_S 		:= $(shell uname -s)

PRJ_BUILD		:= release
PRJ_ARCH		:= x86_64
PRJ_SIMD		:= 
PRJ_BUILD_SUFFIX := 

#
# usage
# make build=debug arch=x86_32 simd=ssse3
# make build=release arch=x86_64 simd=ssse3
#
#

#CXX := armv7-rpi2-linux-gnueabihf-g++
#CC := armv7-rpi2-linux-gnueabihf-gcc

# x86_32, x86_64
#ifdef arch
#	PRJ_ARCH:=$(arch)
#endif

#build_config could be [release|debug]
ifdef build
	PRJ_BUILD:=$(build)
endif


ifdef simd
	PRJ_SIMD := $(simd)
endif

ifdef arch
	PRJ_ARCH :=$(arch)
endif

ifeq ($(PRJ_ARCH),armv7a)
	CXX := armv7-rpi2-linux-gnueabihf-g++
	CC := armv7-rpi2-linux-gnueabihf-gcc
	AR := armv7-rpi2-linux-gnueabihf-ar
endif


CC_SIMD = 
CC_3RD_CPP_MISC = 

#preprocessing related flag, it's useful for debug purpose
#refer to https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/Preprocessor-Options.html#Preprocessor-Options
#-MP -MMD -MF dependency_file

#-fPIC https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/Code-Gen-Options.html#Code-Gen-Options
CC_MISC		:= -fPIC -c
CC_LANG_VERSION		:= -std=c++14

LINK_MISC := -rdynamic $(LINK_MISC)

LINK_CXX_LIBS = -lpthread -ldl

ifneq ($(UNAME_S),Darwin)
	LINK_CXX_LIBS := -lrt $(LINK_CXX_LIBS)
endif


ifeq ($(PRJ_BUILD),debug)
	PRJ_BUILD_SUFFIX := d
	DEFINES := $(DEFINES) DEBUG
	CC_MISC := $(CC_MISC) -g -Wall -O0
else
	DEFINES := $(DEFINES) RELEASE NDEBUG
	CC_MISC := $(CC_MISC) -O2
endif

#-ftree-vectorize enable this option would result bus error for rpi4

ifeq ($(PRJ_ARCH),x86_64)
    CC_MISC := $(CC_MISC) -m64
else ifeq ($(PRJ_ARCH),x86_32)
    CC_MISC := $(CC_MISC) -m32
else ifeq ($(PRJ_ARCH),armv7a)
    CC_MISC := $(CC_MISC)
else 
	CC_MISC := $(CC_MISC) -munknown_arch
endif

X86_X86_X86 := x86_32 x86_64
ARCH_IS_X86 := YES
ARCH_IS_ARMV7A := NO
SIMD_DEFINES := 

ifeq ($(PRJ_ARCH), $(findstring $(PRJ_ARCH),$(X86_X86_X86) ))
	ifeq ($(PRJ_SIMD),$(findstring $(PRJ_SIMD),avx2))
		CC_SIMD := -mssse3 -mavx2
		SIMD_DEFINES := BFR_ENABLE_AVX2 BFR_ENABLE_SSSE3
	else ifeq ($(PRJ_SIMD),ssse3)
		CC_SIMD := -mssse3
		SIMD_DEFINES := BFR_ENABLE_SSSE3
	else 
		CC_SIMD :=
	endif
else ifeq ($(PRJ_ARCH),armv7a)
	CC_SIMD := -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon -fno-tree-vectorize

	SIMD_DEFINES := BFR_ENABLE_NEON
	ARCH_IS_X86 := NO
	ARCH_IS_ARMV7A := YES
else 
	ARCH_IS_X86 := NO
endif

SIMD_DEFINES :=\
	$(foreach define,$(SIMD_DEFINES), -D$(define))


ifdef ver
	BUILD_VER := $(ver)
else
	BUILD_VER := a000
endif

CC_DUMP := NO

ifdef cc_dump
	CC_DUMP := $(cc_dump)
endif


comma:=,
empty:=
space:=$(empty) $(empty)

ifneq ($(PRJ_SIMD),)
	ARCH_BUILD_NAME := $(PRJ_ARCH)_$(PRJ_SIMD)
else
	ARCH_BUILD_NAME := $(PRJ_ARCH)
endif

ifneq ($(PRJ_BUILD_SUFFIX),)
	ARCH_BUILD_NAME := $(ARCH_BUILD_NAME)_$(PRJ_BUILD_SUFFIX)
endif


LIBPREFIX	= lib
LIBEXT		= a
ifndef $(O_EXT)
	O_EXT=o
endif