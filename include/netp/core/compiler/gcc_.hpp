#ifndef  _NETP_CORE_COMPILER_COMPILER_GNUGCC_HPP_
#define _NETP_CORE_COMPILER_COMPILER_GNUGCC_HPP_


//https://sourceforge.net/p/predef/wiki/Endianness/
#include <endian.h>
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

inline unsigned long long htonll(unsigned long long val)
{
	if (__BYTE_ORDER == __BIG_ENDIAN) return (val);
	else return __bswap_64(val);
}

inline unsigned long long ntohll(unsigned long long val)
{
	if (__BYTE_ORDER == __BIG_ENDIAN) return (val);
	else return __bswap_64(val);
}

//ssize_t
#include <sys/types.h>

namespace netp {
	typedef __SIZE_TYPE__		size_t;
	typedef ::ssize_t		ssize_t;
	typedef int SOCKET;
}



#endif
