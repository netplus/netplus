#ifndef _NETP_CORE_COMPILER_HPP_
#define _NETP_CORE_COMPILER_HPP_

//for Storage class specifiers please refer to https://en.cppreference.com/w/cpp/language/storage_duration
// all names declared in unnamed namespace or a namespace within an unnamed namespace, even ones explicitly declared extern, have internal linkage. (since C++11)
// static in class enable class level access for class member/method
// thread_local for auto variables (eg: local variable) equivalent to [thread_local static] 
// 
// 
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

#define __NETP_AMW_32	32
#define __NETP_AMW_64	64

#ifdef _NETP_MSVC
    #if defined(_M_AMD64)
		#define _NETP_ARCH_X64
		#define _NETP_ARCH_STR x64
		#define _NETP_AM_W __NETP_AMW_64
		#define _NETP_AMW64
	#elif defined(_M_IX86)
		#define _NETP_ARCH_X86
		#define _NETP_ARCH_STR x86
		#define _NETP_AM_W __NETP_AMW_32
		#define _NETP_AMW32
	#elif defined(_M_ARM64)
		#define _NETP_ARCH_ARM
		#define _NETP_ARCH_STR arm64
		#define _NETP_ARM_VERSION	_M_ARM64
		#define _NETP_AMW64
	#elif defined(_M_ARM)
		#define _NETP_ARCH_ARM
		#define _NETP_ARCH_STR arm
		#define _NETP_ARM_VERSION	_M_ARM
		#define _NETP_AMW32
    #endif
#elif defined(_NETP_GCC)
	#if (defined(__x86_64__) && __x86_64__) || defined(__ppc64__) && __ppc64__
		#define _NETP_ARCH_X64
		#define _NETP_ARCH_STR x64
		#define _NETP_AM_W __NETP_AMW_64
		#define _NETP_AMW64
	#elif defined(__i386) || defined(__i486__) || defined(__i586__)|| defined(__i686__)
		#define _NETP_ARCH_X86
		#define _NETP_ARCH_STR x86
		#define _NETP_AM_W __NETP_AMW_32
		#define _NETP_AMW32
	#elif defined(__arm__)
		//https://sourceforge.net/p/predef/wiki/Architectures/
		#define _NETP_ARCH_ARM

		#ifdef __ARM_ARCH_7A__
			#define _NETP_ARCH_ARMV7A
			#define _NETP_ARCH_STR armv7a
			#define _NETP_AM_W __NETP_AMW_32
			#define _NETP_AMW32
		#else
			#error "unknown arm version"
		#endif
	#else
		#error "unknown arch"
	#endif
#else
    #error
#endif

#define NETP_DEFAULT_ALIGN (alignof(std::max_align_t))
#define NETP_IS_DEFAULT_ALIGN(alignment) (alignof(std::max_align_t) == alignment)

#define _NETP_OVERRIDE		override
#define _NETP_NOEXCEPT		noexcept
#define _NETP_CONSTEXPR	constexpr

#define _compiler_generic_header1(_generic_) <netp/core/compiler/_generic_##_.hpp>
#define _compiler_generic_header2(_generic_) _compiler_generic_header1(_generic_)
#include _compiler_generic_header2(_NETP_COMPILER_STR)

#define _compiler_arch_header1(_generic_,_arch_) <netp/core/compiler/_generic_##_##_arch_##_.hpp>
#define _compiler_arch_header2(_generic_,_arch_) _compiler_arch_header1(_generic_,_arch_)
#include _compiler_arch_header2(_NETP_COMPILER_STR,_NETP_ARCH_STR)

#ifdef __NETP_IS_LITTLE_ENDIAN
	#define NETP_HTONS(x) htons(x)
	#define NETP_HTONL(x) htonl(x)
	#define NETP_HTONLL(x) htonll(x)

	#define NETP_NTOHS(x) ntohs(x)
	#define NETP_NTOHL(x) ntohl(x)
	#define NETP_NTOHLL(x) ntohll(x)
#else
	#define NETP_HTONS(x) (x)
	#define NETP_HTONL(x) (x)
	#define NETP_HTONLL(x) (x)

	#define NETP_NTOHS(x) (x)
	#define NETP_NTOHL(x) (x)
	#define NETP_NTOHLL(x) (x)
#endif

//for l2 spec
#ifdef __NETP_IS_LITTLE_ENDIAN
	#define __L2_LITTLE_ENDIAN
#endif

#ifdef _NETP_AMW32
	#define __L2_AMW32
#endif

#define __L2_NTOHS(x) NETP_NTOHS(x)
#define __L2_HTONS(x) NETP_HTONS(x)
#define __L2_NTOHL(x) NETP_NTOHL(x)
#define __L2_HTONL(x) NETP_HTONL(x)
#define __L2_NTOHLL(x) NETP_NTOHLL(x)
#define __L2_HTONLL(x) NETP_HTONLL(x)

namespace netp {
	typedef signed char				i8_t;
	typedef short					i16_t;
	typedef int						i32_t;
	typedef long long				i64_t;
	typedef unsigned char			u8_t;
	typedef unsigned short			u16_t;
	typedef unsigned int			u32_t;
	typedef unsigned long long		u64_t;
	typedef unsigned char			byte_t;
	typedef long					long_t;
	typedef unsigned long			ulong_t;

	#define NETP_SIZEOF_ULONG (sizeof(ulong_t))

	#ifdef _NETP_AMW64
		typedef long int			intptr_t;
		typedef unsigned long int	uintptr_t;
	#else
		typedef int					intptr_t;
		typedef unsigned int		uintptr_t;
	#endif
}

#endif