#ifndef _NETP_CORE_COMPILER_HPP_
#define _NETP_CORE_COMPILER_HPP_

/*
MSVC++ 14.0 _MSC_VER == 1900 (Visual Studio 2015)
MSVC++ 12.0 _MSC_VER == 1800 (Visual Studio 2013)
MSVC++ 11.0 _MSC_VER == 1700 (Visual Studio 2012)
MSVC++ 10.0 _MSC_VER == 1600 (Visual Studio 2010)
MSVC++ 9.0  _MSC_VER == 1500 (Visual Studio 2008)
MSVC++ 8.0  _MSC_VER == 1400 (Visual Studio 2005)
MSVC++ 7.1  _MSC_VER == 1310 (Visual Studio 2003)
MSVC++ 7.0  _MSC_VER == 1300
MSVC++ 6.0  _MSC_VER == 1200
MSVC++ 5.0  _MSC_VER == 1100
*/

//for compiler
//#if __cplusplus<201103L
//	#define override
//#endif


#if defined(_MSC_VER)
	#define _NETP_COMPILER_STR msvc
	#define _NETP_MSVC
#elif defined(__GNUC__)
	#define _NETP_COMPILER_STR gcc
	#define _NETP_GCC
#else
	#error "unknown compiler"
#endif

/*

//for arm
//refer to http://community.arm.com/groups/android-community/blog/2015/03/27/arm-neon-programming-quick-reference

#define __NETP_SIMD (0)

#define NETP_SIMD_HAVE(a) (((__NETP_SIMD)&(a)) ==(a))
*/

#define _NETP_ADDRESS_MODEL_32	32
#define _NETP_ADDRESS_MODEL_64	64

#ifdef _NETP_MSVC
    #if defined(_M_AMD64)
		#define _NETP_ARCH_X64
		#define _NETP_ARCH_STR x64
        #define _NETP_ADDRESS_MODEL _NETP_ADDRESS_MODEL_64
		#define _NETP_AM64
	#elif defined(_M_IX86)
			#define _NETP_ARCH_X86
			#define _NETP_ARCH_STR x86
			#define _NETP_ADDRESS_MODEL _NETP_ADDRESS_MODEL_32
			#define _NETP_AM32
	#elif defined(_M_ARM)
			#define _NETP_ARCH_ARM
			#define _NETP_ARM_VERSION	_M_ARM
			#error "to be completed"
    #endif
#elif defined(_NETP_GCC)
	#if (defined(__x86_64__) && __x86_64__) || defined(__ppc64__) && __ppc64__
		#define _NETP_ARCH_X64
		#define _NETP_ARCH_STR x64
		#define _NETP_ADDRESS_MODEL _NETP_ADDRESS_MODEL_64
		#define _NETP_AM64
	#elif defined(__i386) || defined(__i486__) || defined(__i586__)|| defined(__i686__)
		#define _NETP_ARCH_X86
		#define _NETP_ARCH_STR x86
		#define _NETP_ADDRESS_MODEL _NETP_ADDRESS_MODEL_32
		#define _NETP_AM32
	#elif defined(__arm__)
		//https://sourceforge.net/p/predef/wiki/Architectures/
		#define _NETP_ARCH_ARM

		#ifdef __ARM_ARCH_7A__
			#define _NETP_ARCH_ARMV7A
			#define _NETP_ARCH_STR armv7a
			#define _NETP_ADDRESS_MODEL _NETP_ADDRESS_MODEL_32
			#define _NETP_AM32
		#else
			#error "unknown arm version"
		#endif
	#else
		#error "unknown arch"
	#endif
#else
    #error
#endif

#define _NETP_OVERRIDE		override
#define _NETP_NOEXCEPT		noexcept
#define _NETP_CONSTEXPR	constexpr

#define _compiler_generic_header1(_generic_) <netp/core/compiler/_generic_##_.hpp>
#define _compiler_generic_header2(_generic_) _compiler_generic_header1(_generic_)
#include _compiler_generic_header2(_NETP_COMPILER_STR)

#define _compiler_arch_header1(_generic_,_arch_) <netp/core/compiler/_generic_##_##_arch_##_.hpp>
#define _compiler_arch_header2(_generic_,_arch_) _compiler_arch_header1(_generic_,_arch_)
#include _compiler_arch_header2(_NETP_COMPILER_STR,_NETP_ARCH_STR)

namespace netp {

	typedef signed char				i8_t;
	typedef short							i16_t;
	typedef int								i32_t;
	typedef long long					i64_t;
	typedef unsigned char			u8_t;
	typedef unsigned short			u16_t;
	typedef unsigned int				u32_t;
	typedef unsigned long long	u64_t;
	typedef unsigned char			byte_t;
	typedef long							long_t;

	#define WORD_WIDTH (64u)
	typedef u64_t word_t;
}

#endif