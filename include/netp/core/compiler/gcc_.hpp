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
	#if __NETP_IS_BIG_ENDIAN
	#define htonll(val) (val)
	#define ntohll(val) (val)
	#else
	#define htonll(val) __bswap_64(val)
	#define ntohll(val) __bswap_64(val)
	#endif
#endif

#define __NETP_IS_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)
#define __NETP_IS_LITTLE_ENDIAN (!__NETP_IS_BIG_ENDIAN)

#define __NETP_TLS thread_local
#define __NETP_NOEXCEPT noexcept
#define __NETP_FORCE_INLINE inline __attribute__((always_inline))

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

#endif