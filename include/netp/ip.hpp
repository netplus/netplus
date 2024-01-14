#ifndef _NETP_IP_HPP
#define _NETP_IP_HPP

#include <netp/ipv4.hpp>
#include <netp/ipv6.hpp>
#include <netp/string.hpp>

namespace netp {

const ipv4_t IP_LOOPBACK = { 2130706433U };

enum ip_version {
	v4 = 4,
	v6 = 6
};

#pragma pack(push,1)
	typedef union __ip_byte ip_byte_t;
	using ip_t = ip_byte_t;
	union __ip_byte {
		ipv4_t v4;
		ipv6_t v6;
		netp::u8_t byte[16];
	};
	typedef union __ipv4_byte ipv4_byte_t;
	union __ipv4_byte {
		ipv4_t v4;
		netp::u8_t byte[4];
	};
	typedef union __ipv6_byte ipv6_byte_t;
	union __ipv6_byte {
		ipv6_t v6;
		netp::u8_t byte[16];
	};
#pragma pack(pop)
	static_assert(sizeof(ip_byte_t) == 16, "ip_bits_t size check");
	static_assert(sizeof(ipv4_byte_t) == 4, "ipv4_bits_t size check");
	static_assert(sizeof(ipv6_byte_t) == 16, "ipv6_bits_t size check");

	__NETP_FORCE_INLINE
	bool operator== (ip_t const& A, ip_t const& B) {
		return std::memcmp(A.byte, B.byte, 16) ==0;
	}
	__NETP_FORCE_INLINE
	bool operator!= (ip_t const& A, ip_t const& B) {
		return std::memcmp(A.byte, B.byte, 16) != 0;
	}

	extern string_t niptostring(ip_version vx, ip_t const& ip);
	extern string_t iptostring(ip_version vx, ip_t const& ip);

	extern ip_t stringtonip(ip_version vx, const char* string);
	extern ip_t stringtoip(ip_version vx, const char* string);

	extern ip_t iptonip(ip_version vx, ip_t const& ip);
	extern ip_t niptoip(ip_version vx, ip_t const& ip);

	extern ipv4_t dotiptonip(const char* dotip);
	extern ipv4_t dotiptoip(const char* dotip);

	extern bool is_dotipv4_decimal_notation(const char* string);

	inline ipv4_t ipv4tonipv4(ipv4_t const& ip) { return { NETP_HTONL(ip.u32) }; }
	inline ipv4_t nipv4toipv4(ipv4_t const& ip) { return { NETP_NTOHL(ip.u32) }; }

	extern string_t nipv4todotip(ipv4_t const& ip);
	inline string_t ipv4todotip(ipv4_t const& ip) { return nipv4todotip({ NETP_HTONL(ip.u32) }); }

	inline ipv6_t ipv6tonipv6(ipv6_t const& v6_) {
		ipv6_t v6 = v6_;
		v6.u64.A = NETP_HTONLL(v6.u64.A);
		v6.u64.B = NETP_HTONLL(v6.u64.B);
		return v6;
	}

	inline ipv6_t nipv6toipv6(ipv6_t const& v6_) {
		ipv6_t v6 = v6_;
		v6.u64.A = NETP_NTOHLL(v6.u64.A);
		v6.u64.B = NETP_NTOHLL(v6.u64.B);
		return v6;
	}

	extern ipv6_t v6stringtonip(const char* v6string);
	inline ipv6_t v6stringtoip(const char* v6string) {
		return nipv6toipv6(v6stringtonip(v6string));
	}

	extern string_t nipv6tov6string(ipv6_t const& v6);
	inline string_t ipv6tov6string(ipv6_t const& v6) {
		return nipv6tov6string(v6);
	}

	__NETP_FORCE_INLINE
	netp::u32_t ipv4_t4h(ipv4_t lip, port_t lport, ipv4_t rip, port_t rport) {
		return ((netp::u32_t)(lip.u32) * 59) ^
			((netp::u32_t)(rip.u32)) ^
			((netp::u32_t)(lport) << 16) ^
			((netp::u32_t)(rport))
			;
	}

	//::1 - IPv6  loopback
	//127.0.0.0 - 127.255.255.255  (127 / 8 prefix)
	__NETP_FORCE_INLINE
	bool is_loopback(ipv4_t v4_/*nip*/) {
		return v4_.bits.b1 == 127;
	}

	/* An IP should be considered as internal when
	10.0.0.0     -   10.255.255.255  (10/8 prefix)
	172.16.0.0   -   172.31.255.255  (172.16/12 prefix)
	192.168.0.0  -   192.168.255.255 (192.168/16 prefix)
	*/
	__NETP_FORCE_INLINE
	bool is_rfc1918(ipv4_t v4_/*nip*/) {
		switch (v4_.bits.b1) {
		case 10:
			return true;
		case 172:
			return (v4_.bits.b2 >= 16) && (v4_.bits.b2 < 32);
		case 192:
			return (v4_.bits.b2 == 168);
		default:
			return false;
		}
	}

	//100.64.0.0/10: https://datatracker.ietf.org/doc/html/rfc6598
	__NETP_FORCE_INLINE
	bool is_shared_address_space(ipv4_t v4_) {
		if (v4_.bits.b1==100) {
			return (((v4_.bits.b2 >> 6) & 0x01) == 0x01);
		}
		return false;
	}

	__NETP_FORCE_INLINE
	bool is_loopback_or_rfc1918(ipv4_t v4_) {
		switch (v4_.bits.b1) {
		case 10:
		case 127:
			return true;
		case 172:
			return (((v4_.bits.b2>>4)&0x01) == 0x01);
		case 192:
			return (v4_.bits.b2 == 168);
		default:
			return false;
		}
	}

	//10.0.0.0        -   10.255.255.255  (10/8 prefix)
	//172.16.0.0 - 172.31.255.255  (172.16/12 prefix)
	//192.168.0.0 - 192.168.255.255 (192.168/ 16 prefix)
	__NETP_FORCE_INLINE
	bool __is_internal_or_shared(ipv4_t v4_) {
		switch (v4_.bits.b1) {
		case 10:
		case 127:
			return true;
		case 192:
			return (v4_.bits.b2 == 168);
		case 172:
			return (((v4_.bits.b2>>4)&0x01) == 0x01);
		case 100:
			return (((v4_.bits.b2>>6)&0x01) == 0x01);
		default:
			return false;
		}
	}

	__NETP_FORCE_INLINE
	bool is_internal(ipv4_t v4_ /*nip*/) {
		return is_loopback_or_rfc1918(v4_);
	}

	__NETP_FORCE_INLINE
	bool is_internal_or_shared(ipv4_t v4_) {
		return __is_internal_or_shared(v4_);
	}

	extern netp::ipv4_t v4_mask_by_prefix(const netp::ipv4_t* const v4, netp::u8_t prefix);
	extern netp::ipv6_t v6_mask_by_prefix(const netp::ipv6_t* const v6, netp::u8_t prefix);
	extern int ip_from_cidr_string(netp::ip_version vx, const char* cidrstr, netp::ip_t* const ip/*stored in host endian*/, netp::u8_t* prefix );
	extern int nip_from_cidr_string(netp::ip_version vx, const char* cidrstr, netp::ip_t* const ip/*stored in network endian*/, netp::u8_t* prefix );
}

#endif