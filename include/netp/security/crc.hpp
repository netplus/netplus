#ifndef _NETP_SECURITY_HPP
#define _NETP_SECURITY_HPP

#include <netp/core.hpp>

namespace netp { namespace security {

	//it's faster than f16
	inline netp::u16_t crc16(netp::byte_t* dataptr, netp::u32_t len)
	{
		netp::u32_t sum = 0;
		u16_t* data16 = (u16_t*)dataptr;
		
		u32_t n = 0, m = len / sizeof(u16_t);
		for (n = 0; n < m; ++n) {
			sum += u32_t(data16[n]);
		}

		if (len&0x1) {
			sum += u32_t(dataptr[n-1]);
		}

		while (sum>>16) {
			sum = (sum & 0xffff) + (sum >> 16);
		}

		return u16_t(~netp::u16_t(sum));
	}
}}
#endif