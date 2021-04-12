#ifndef  _NETP_CORE_COMPILER_COMPILER_MSVC_HPP_
#define _NETP_CORE_COMPILER_COMPILER_MSVC_HPP_

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

#include <BaseTsd.h>


#define __NETP_IS_BIG_ENDIAN (0)
#define __NETP_IS_LITTLE_ENDIAN (!__NETP_IS_BIG_ENDIAN)

#define __NETP_TLS thread_local
#define __NETP_FORCE_INLINE __forceinline

#if defined(_MSC_VER) && (_MSC_VER < 1600)
	typedef unsigned char uint8_t;
	typedef unsigned int uint32_t;
	typedef unsigned __int64 uint64_t;
#endif // !defined(_MSC_VER)


#if _MSC_VER == 1700
	#undef _NETP_OVERRIDE
	#define _NETP_OVERRIDE

	#undef _NETP_NOEXCEPT
	#define _NETP_NOEXCEPT throw()

	#undef _NETP_CONSTEXPR
	#define _NETP_CONSTEXPR

	#define _NETP_NO_CXX11_DELETED_FUNC
	#define _NETP_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
	#define _NETP_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_VALUE
	#define _NETP_NO_CXX11_TEMPLATE_VARIADIC_ARGS

	#define _NETP_NO_CXX11_LAMBDA_PASS_DEFAULT_VALUE
	#define _NETP_STD_CHRONO_STEADY_CLOCK_EXTEND_SYSTEM_CLOCK
#else
	#define _NETP_NOEXCEPT noexcept
#endif

#ifdef _NETP_NO_CXX11_TEMPLATE_VARIADIC_ARGS
	#if defined(_VARIADIC_MAX) && (_VARIADIC_MAX != 10)
		#pragma message( __FILE__ " must be prior to include<memory>")
	#else
		#define _VARIADIC_MAX 10
	#endif
#endif

#define NETP_LIKELY(x) (x)
#define NETP_UNLIKELY(x) (x)

/*
The __AVX__ preprocessor symbol is defined when the /arch:AVX, /arch:AVX2 or /arch:AVX512 compiler option is specified.
The __AVX2__ preprocessor symbol is defined when the /arch:AVX2 or /arch:AVX512 compiler option is specified.
The __AVX512F__, __AVX512CD__, __AVX512BW__, __AVX512DQ__ and __AVX512VL__ preprocessor symbols are defined when the /arch:AVX512 compiler option is specified.
For more information, see Predefined Macros. The /arch:AVX2 option was introduced in Visual Studio 2013 Update 2, version 12.0.34567.1. Limited support for /arch:AVX512 was added in Visual Studio 2017, and expanded in Visual Studio 2019.

refer to https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2019
refer to https://docs.microsoft.com/en-us/cpp/build/reference/arch-x86?view=vs-2019
refer to https://docs.microsoft.com/en-us/cpp/build/reference/arch-x64?view=vs-2019
*/


namespace netp {
	typedef ::SSIZE_T			ssize_t;
	typedef ::size_t				size_t;
	typedef ::size_t				SOCKET;
}

#endif