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
		ipv4_t u32;
		struct _bit {
			u8_t b1;
			u8_t b2;
			u8_t b3;
			u8_t b4;
		} bits;
	};

	union ipv6_bits {
		struct _u64 {
			u64_t A;
			u64_t B;
		}u64;
		struct _bit {
			u16_t s1;
			u16_t s2;
			u16_t s3;
			u16_t s4;
			u16_t s5;
			u16_t s6;
			u16_t s7;
			u16_t s8;
		} bits;
	};

	union ip_bits {
		ipv4_bits v4;
		ipv6_bits v6;
		netp::u8_t byte[16];
	};
#pragma pack(pop)
	static_assert(sizeof(ipv4_bits) == 4, "ipv4_bits size check");
	static_assert(sizeof(ipv6_bits) == 16, "ipv6_bits size check");
	static_assert(sizeof(ip_bits) == 16, "ip_bits size check");
}

#endif