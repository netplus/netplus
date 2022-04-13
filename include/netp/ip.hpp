#ifndef _NETP_IP_HPP
#define _NETP_IP_HPP

#include <netp/ipv4.hpp>
#include <netp/ipv6.hpp>
#include <netp/string.hpp>

namespace netp {

const ipv4_t IP_LOOPBACK = { 2130706433U };

#pragma pack(push,1)
	typedef union __ip_bits ip_t;
	union __ip_bits {
		ipv4_t v4;
		ipv6_t v6;
		netp::u8_t byte[16];
	};
#pragma pack(pop)
	static_assert(sizeof(ip_t) == 16, "ip_bits size check");

	__NETP_FORCE_INLINE
	bool operator== (ip_t const& A, ip_t const& B) {
		return std::memcmp(A.byte, B.byte, 16) ==0;
	}
	__NETP_FORCE_INLINE
	bool operator!= (ip_t const& A, ip_t const& B) {
		return std::memcmp(A.byte, B.byte, 16) != 0;
	}

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
		case 192:
			return (v4_.bits.b2 == 168);
		case 172:
			return (v4_.bits.b2 >= 16) && (v4_.bits.b2 < 32);
		default:
			return false;
		}
	}

	__NETP_FORCE_INLINE
	bool is_loopback_or_rfc1918(ipv4_t v4_) {
		switch (v4_.bits.b1) {
		case 10:
		case 127:
			return true;
		case 192:
			return (v4_.bits.b2 == 168);
		case 172:
			return (v4_.bits.b2 >= 16) && (v4_.bits.b2 < 32);
		default:
			return false;
		}
	}

	__NETP_FORCE_INLINE
	bool is_internal(ipv4_t v4_ /*nip*/) {
		return is_loopback_or_rfc1918(v4_);
	}

	extern netp::ipv4_t v4_mask_by_cidr(const netp::ipv4_t* const v4, netp::u8_t cidr);
	extern netp::ipv6_t v6_mask_by_cidr(const netp::ipv6_t* const v6, netp::u8_t cidr);
	extern int ip_from_cidr_string(const char* cidrstr, netp::ip_t* const ipbits/*stored in network endian*/, netp::u8_t* cidr, bool isv6);
}

#endif