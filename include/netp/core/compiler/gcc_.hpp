#ifndef  _NETP_CORE_COMPILER_COMPILER_GNUGCC_HPP_
#define _NETP_CORE_COMPILER_COMPILER_GNUGCC_HPP_


//https://sourceforge.net/p/predef/wiki/Endianness/
#if defined(__APPLE__) && defined(__MACH__)
	#include <machine/endian.h>
	#if !defined(__BYTE_ORDER) && defined(BYTE_ORDER)
		#define __BYTE_ORDER BYTE_ORDER
	#endif

	#if !defined(__BIG_ENDIAN) && defined(BIG_ENDIAN)
		#define __BIG_ENDIAN BIG_ENDIAN
	#endif
#else
	#include <endian.h>
	#define __NETP_NO_NTOH_LL_HTON_LL
#endif

#define __NETP_IS_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)
#define __NETP_IS_LITTLE_ENDIAN (!__NETP_IS_BIG_ENDIAN)

#ifdef __NETP_NO_NTOH_LL_HTON_LL
	#if __NETP_IS_BIG_ENDIAN
		#define htonll(val) (val)
		#define ntohll(val) (val)
	#else
		#define htonll(val) __bswap_64(val)
		#define ntohll(val) __bswap_64(val)
	#endif
#endif

#define __NETP_TLS thread_local
#define __NETP_NOEXCEPT noexcept

/*
This function attribute indicates that a function must be inlined.

The compiler attempts to inline the function, regardless of the characteristics of the function.
In some circumstances the compiler may choose to ignore the __attribute__((always_inline)) attribute and not inline a function. For example:
A recursive function is never inlined into itself.
Functions making use of alloca() are never inlined.
Note
This function attribute is a GNU compiler extension that the ARM compiler supports. It has the keyword equivalent __forceinline.
*/
#define __NETP_FORCE_INLINE inline __attribute__((always_inline))
#define __NETP_NO_INLINE __attribute__ ((noinline))
#define __NETP_ALIGN(x) __attribute__ ((aligned(x)))

#define NETP_LIKELY(x) __builtin_expect(!!(x), 1)
#define NETP_UNLIKELY(x) __builtin_expect(!!(x), 0)

#ifdef _GNU_SOURCE
    #define NETP_ENABLE_GNU_SOURCE
#endif

//ssize_t
#include <sys/types.h>

namespace netp {
	typedef __SIZE_TYPE__		size_t;
	typedef ::ssize_t		ssize_t;
	typedef int SOCKET;
}


#if __GNUC__ <= 4 && __GNUC_MINOR__ <= 8
namespace std {
    union max_align_t {
      char          c;
      float         f;
      __uint32_t    u32;
      __uint64_t    u64;
      double        d;
      long double   ld;
      __uint32_t*   ptr;
    };
}
#endif//endof __GNUC__


#endif

