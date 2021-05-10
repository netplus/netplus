CURRENT_DIR 		:= $(shell pwd)
UNAME_S 			:= $(shell uname -s)
OS_NAME				:= $(shell hostnamectl status| grep -i 'Operating System' | awk '{print $$3}')
PRJ_BUILD			:= release
PRJ_ARCH			:= x86_64
PRJ_SIMD			:= 
PRJ_BUILD_SUFFIX 	:= 
PRJ_DEFS			:= 
#
# usage
# make build=debug arch=x86_32 simd=ssse3
# make build=release arch=x86_64 simd=ssse3
#

#build_config could be [release|debug]
ifdef build
	PRJ_BUILD := $(build)
endif


ifdef arch
	PRJ_ARCH :=$(arch)
endif

ifdef simd
	PRJ_SIMD := $(simd)
endif


ifdef defs
	PRJ_DEFS := $(defs)
endif

DEFINES := $(DEFINES) $(PRJ_DEFS)

ifeq ($(PRJ_ARCH),armv7a)
	CXX := armv7-rpi2-linux-gnueabihf-g++
	CC := armv7-rpi2-linux-gnueabihf-gcc
	AR := armv7-rpi2-linux-gnueabihf-ar
endif

#detect cxx version
$(shell $(CXX) -v 2> ./__tmp_makefile_cxx_version)
CXX_VERSION := $(shell cat ./__tmp_makefile_cxx_version | grep -i 'gcc version' | awk '{print $$3}')
$(shell rm -f ./__tmp_makefile_cxx_version)

ifdef deps
	PRJ_DEPS := $(deps)
else
	PRJ_DEPS := ../../../../libs/deps
endif

DEPS_LIBS_CONFIG_INC_FILE := $(PRJ_DEPS)/deps-libs-config.inc
DEPS_LIBS_DEFS_FILE		 := $(PRJ_DEPS)/deps-libs-defs

DEPS_INC 	:= 
DEPS_LIB 	:= 
DEPS_DEFS 	:= 

ifneq ("$(wildcard $(DEPS_LIBS_CONFIG_INC_FILE))","")
	include $(DEPS_LIBS_CONFIG_INC_FILE)
	DEPS_INC := $(DEPS_INC) $(_DEPS_INC)
	DEPS_LIB := $(DEPS_LIB) $(_DEPS_LIB)
endif

ifneq ("$(wildcard $(DEPS_LIBS_DEFS_FILE))","")
	DEPS_DEFS := $(shell cat $(DEPS_LIBS_DEFS_FILE))
	DEFINES := $(DEPS_DEFS) $(DEFINES)
endif


CC_SIMD = 
CC_3RD_CPP_MISC = 

#preprocessing related flag, it's useful for debug purpose
#refer to https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/Preprocessor-Options.html#Preprocessor-Options
#-MP -MMD -MF dependency_file

#-fPIC https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/Code-Gen-Options.html#Code-Gen-Options
CC_MISC		:= -fPIC -c
CC_C11		:= -std=c++11
LINK_MISC 	:= -rdynamic $(LINK_MISC)
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
	LINK_MISC := -m64 $(LINK_MISC)
else ifeq ($(PRJ_ARCH),x86_32)
    CC_MISC := $(CC_MISC) -m32
	LINK_MISC := -m32 $(LINK_MISC)
else ifeq ($(PRJ_ARCH),armv7a)
    CC_MISC := $(CC_MISC)
else 
	CC_MISC := $(CC_MISC) -munknown_arch
endif

X86_X86_X86 := x86_32 x86_64
ARCH_IS_X86 := YES
ARCH_IS_ARMV7A := NO
SIMD_DEFINES := 

#for x86_64, sse2 is on by default for both x32&x64 gcc compiler
#for x86_32, sse2 is off by default for a x32 gcc compiler
ifeq ($(PRJ_ARCH), $(findstring $(PRJ_ARCH),$(X86_X86_X86) ))
	ifeq ($(PRJ_SIMD),$(findstring $(PRJ_SIMD),avx2))
		CC_SIMD := -msse2 -mssse3 -mavx2
		SIMD_DEFINES := BFR_ENABLE_SSE2 BFR_ENABLE_SSSE3 BFR_ENABLE_AVX2
	else ifeq ($(PRJ_SIMD),ssse3)
		CC_SIMD := -msse2 -mssse3
		SIMD_DEFINES := BFR_ENABLE_SSE2 BFR_ENABLE_SSSE3
	else 
		ifeq ($(PRJ_ARCH),x86_32)
			CC_SIMD := -msse2
			SIMD_DEFINES := BFR_ENABLE_SSE2
		else
			CC_SIMD :=
		endif
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
	TARGET_VER := $(ver)
else
	TARGET_VER := a000
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