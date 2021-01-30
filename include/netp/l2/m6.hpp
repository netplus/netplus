#ifndef NETP_NET_L2_M6_HPP_
#define NETP_NET_L2_M6_HPP_

#include <string>
#include <netp/core.hpp>
#include <netp/string.hpp>

namespace netp { namespace l2 {
#pragma pack(push,1)
	union m6 {
		struct __m6__ {
			netp::u8_t b1;
			netp::u8_t b2;
			netp::u8_t b3;
			netp::u8_t b4;
			netp::u8_t b5;
			netp::u8_t b6;
		} B6;
		netp::u8_t payload[6];
	};
#pragma pack(pop)

	inline string_t m6tostring(m6 const& m6_) {
		char tmp[18] = { 0 };
		snprintf(tmp, 18, "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x", m6_.B6.b1, m6_.B6.b2, m6_.B6.b3, m6_.B6.b4, m6_.B6.b5, m6_.B6.b6);
		return string_t(tmp, 17);
	}
	inline bool operator==(netp::l2::m6 const& left, netp::l2::m6 const& right) {
		return std::memcmp((char*)left.payload, (char*)right.payload, 6) == 0;
	}
	inline bool operator!=(netp::l2::m6 const& left, netp::l2::m6 const& right) {
		return std::memcmp((char*)left.payload, (char*)right.payload, 6) != 0;
	}

}}
#endif
