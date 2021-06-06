#ifndef _NETP_IPV4_HPP
#define _NETP_IPV4_HPP

#include <vector>
#include <netp/memory.hpp>

namespace netp {
	typedef unsigned int ipv4_t;
	typedef unsigned short port_t;
	static_assert(sizeof(ipv4_t) == 4, "ipv4_t size assert failed");
	static_assert(sizeof(port_t) == 2, "port_t size assert failed");

	typedef std::vector<ipv4_t, netp::allocator<ipv4_t>> vector_ipv4_t;

#pragma pack(push,1)
	union ipv4_u4 {
		ipv4_t nipv4;
		struct __ipv4_c4 {
			u8_t u1;
			u8_t u2;
			u8_t u3;
			u8_t u4;
		} u4;
	};
#pragma pack(pop)
}

#endif