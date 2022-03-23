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
	//@comment
	//192.168.1.1
	//little endian, b1 == 1, big endian b1 == 192
	union ipv4_bits {
		ipv4_t v4; //
		struct _bit {
			u8_t b1;
			u8_t b2;
			u8_t b3;
			u8_t b4;
		} bits;
	};
#pragma pack(pop)
}

#endif