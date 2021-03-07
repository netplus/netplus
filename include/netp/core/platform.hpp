#ifndef _NETP_CORE_PLATFORM_H_
#define _NETP_CORE_PLATFORM_H_

#include <netp/core/compiler.hpp>

//refer to https://sourceforge.net/p/predef/wiki/OperatingSystems/
#if defined(_WIN32) || defined(_WIN64)
	#define _NETP_WIN
	#define _NETP_PLATFORM_STR		win
#elif defined(__linux__)
	//gnu linux and android
	#if defined(__gnu_linux__)
		#define _NETP_GNU_LINUX
		#define _NETP_PLATFORM_STR		gnu_linux
	#elif defined(__ANDROID__)
		#define _NETP_ANDROID
		#define _NETP_PLATFORM_STR		android
		#ifdef __ANDROID_API__
				#define _NETP_ANDROID_API_LEVEL __ANDROID_API__
		#else
			#error "unknown android api level"
		#endif
	#endif
#elif defined(__APPLE__)
	#define _NETP_APPLE
	#define _NETP_PLATFORM_STR	osx
#else
	#error "unknown platform"
#endif

#define __platform_generic_header(__generic__)		<netp/core/platform/__generic__##_.hpp>
#define _platform_generic_header(__generic__)		__platform_generic_header(__generic__)
#include _platform_generic_header( _NETP_PLATFORM_STR)

#define __platform_arch_header(__generic__,__arch__) <netp/core/platform/__generic__##_##__arch__##_.hpp>
#define _platform_arch_header(__generic__,__arch__) __platform_arch_header(__generic__,__arch__)
#include _platform_arch_header(_NETP_PLATFORM_STR,_NETP_ARCH_STR)

#endif