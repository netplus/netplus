#ifndef _NETP_SECURITY_FLETCHER_HPP
#define _NETP_SECURITY_FLETCHER_HPP

#include <netp/core.hpp>

namespace netp { namespace security {

	/*
	 @note the impl is borrowed from https://en.wikipedia.org/wiki/Fletcher%27s_checksum#Comparison_with_the_Adler_checksum
	*/

	static_assert(sizeof(::uint16_t) == sizeof(netp::u16_t), "sizeof(::uint16_t)!=sizeof(netp::u16_t)");
	static_assert(sizeof(::uint32_t) == sizeof(netp::u32_t), "sizeof(::uint32_t)!=sizeof(netp::u32_t)");
	static_assert(sizeof(::size_t) == sizeof(netp::size_t), "sizeof(::size_t)!=sizeof(netp::size_t)");

	inline ::uint16_t fletcher16(const ::uint8_t *data, ::size_t len)
	{
		::uint32_t c0, c1;
		unsigned int i;

		for (c0 = c1 = 0; len >= 5802; len -= 5802) {
			for (i = 0; i < 5802; ++i) {
				c0 = c0 + *data++;
				c1 = c1 + c0;
			}
			c0 = c0 % 255;
			c1 = c1 % 255;
		}
		for (i = 0; i < len; ++i) {
			c0 = c0 + *data++;
			c1 = c1 + c0;
		}
		c0 = c0 % 255;
		c1 = c1 % 255;
		return (::uint16_t)(c1 << 8 | c0);
	}

	inline ::uint32_t fletcher32(const ::uint16_t *data, ::size_t len)
	{
		::uint32_t c0, c1;
		unsigned int i;

		for (c0 = c1 = 0; len >= 360; len -= 360) {
			for (i = 0; i < 360; ++i) {
				c0 = c0 + *data++;
				c1 = c1 + c0;
			}
			c0 = c0 % 65535;
			c1 = c1 % 65535;
		}
		for (i = 0; i < len; ++i) {
			c0 = c0 + *data++;
			c1 = c1 + c0;
		}
		c0 = c0 % 65535;
		c1 = c1 % 65535;
		return (c1 << 16 | c0);
	}
}}
#endif