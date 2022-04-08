#ifndef _NETP_IPV4_HPP
#define _NETP_IPV4_HPP

#include <vector>
#include <netp/memory.hpp>

namespace netp {
	typedef unsigned short port_t;
	static_assert(sizeof(port_t) == 2, "port_t size assert failed");

#pragma pack(push,1)
	//@comment
	//192.168.1.1
	//little endian, b1 == 1, big endian b1 == 192
	typedef u32_t ipv4_u32_t;
	typedef union __ipv4_bits ipv4_t;
	union __ipv4_bits {
		ipv4_u32_t u32;
		struct _bit {
			u8_t b1;
			u8_t b2;
			u8_t b3;
			u8_t b4;
		} bits;
	};
	static_assert(sizeof(ipv4_t) == 4, "ipv4_t size assert failed");

	__NETP_FORCE_INLINE
	bool operator== (ipv4_t const& A, ipv4_t const& B) {
		return A.u32 == B.u32;
	}
	__NETP_FORCE_INLINE
	bool operator!= (ipv4_t const& A, ipv4_t const& B) {
		return A.u32 != B.u32;
	}

#pragma pack(pop)
	static_assert(sizeof(ipv4_t) == 4, "ipv4_bits size check");

}

#endif