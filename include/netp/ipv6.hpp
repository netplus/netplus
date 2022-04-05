#ifndef _NETP_IPV6_HPP
#define _NETP_IPV6_HPP

namespace netp {
#pragma pack(push,1)
	typedef union __ipv6_bits ipv6_t;
	union __ipv6_bits {
		struct __u64ab {
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
#pragma pack(pop)
	static_assert(sizeof(ipv6_t) == 16, "ipv6_bits size check");

	__NETP_FORCE_INLINE
	bool operator== (ipv6_t const& A, ipv6_t const& B) {
		return A.u64.A == B.u64.A && A.u64.B == B.u64.B;
	}
	__NETP_FORCE_INLINE
	bool operator!= (ipv6_t const& A, ipv6_t const& B) {
		return A.u64.A != B.u64.A || A.u64.B != B.u64.B;
	}
}

#endif