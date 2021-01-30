#ifndef NETP_SECURITY_OBFUSCATE_HPP
#define	NETP_SECURITY_OBFUSCATE_HPP

#include <netp/core.hpp>

//SRC_X's width must be less than sizeof(TARGET_W), it can be u8_t or , u16_t
template <typename SRC_X, typename TARGET_W>
inline static TARGET_W obfuscate(SRC_X val, TARGET_W nonce ) {
	static_assert (sizeof(TARGET_W) >= sizeof(SRC_X), "invalid type") ;
	TARGET_W ret = TARGET_W();
	netp::u8_t b[sizeof(SRC_X) * 8];
	netp::u8_t bii[sizeof(SRC_X) * 8];

	//per bytes contain bit count
	netp::u8_t pbcbc = (sizeof(SRC_X) * 8) / (sizeof(TARGET_W) / sizeof(netp::u8_t));

	for (int i = 0; i < (sizeof(SRC_X) * 8); i++) {
		if ((i%pbcbc) == 0) {
			b[i] = ((nonce >> (5 + 8*(i / pbcbc)) & 0x7) % 5) & 0xFF;
			bii[i] = b[i];
		} else {
			b[i] = ((b[i - 1] + 1) % 5);
			bii[i] = bii[i - 1];
		}

		TARGET_W bi = TARGET_W();
		if (val&(0x01LL << i)) {
			bi |= (0x01LL << b[i]);
		}
		bi |= ((0x7 & bii[i]) << 5);
		ret |= (bi << (8*(i / pbcbc)));
	}
	return ret;
}

template <typename SRC_X, typename TARGET_W>
inline static SRC_X deobfuscate(TARGET_W val) {
	SRC_X r = SRC_X();
	netp::u8_t pbcbc = (sizeof(SRC_X) * 8) / (sizeof(TARGET_W) / sizeof(netp::u8_t));
	netp::u8_t b[sizeof(SRC_X) * 8];

	for (int i = 0; i < (sizeof(SRC_X) * 8); i++) {
		if ((i%pbcbc) == 0) {
			b[i] = ((val >> (5 + 8 * (i / pbcbc)) & 0x7) % 5) & 0xFF;
		} else {
			b[i] = ((b[i - 1] + 1) % 5);
		}
		r |= (((val >> (b[i] + (8 * (i / pbcbc)))) & 0x1) << i);
	}
	return r;
}

namespace netp { namespace security {
	inline static u32_t u8_u32_obfuscate(u8_t val, u32_t nonce) {
		return obfuscate<u8_t,u32_t>(val, nonce);
	}
	inline static u32_t u16_u32_obfuscate(u16_t val, u32_t nonce) {
		return obfuscate<u16_t,u32_t>(val, nonce);
	}
	inline static u8_t u8_u32_deobfuscate(u32_t val) {
		return deobfuscate<u8_t,u32_t>(val);
	}
	inline static u16_t u16_u32_deobfuscate(u32_t val) {
		return deobfuscate<u16_t,u32_t>(val);
	}

	inline static u64_t u8_u64_obfuscate(u8_t val, u64_t nonce) {
		return obfuscate<u8_t, u64_t>(val, nonce);
	}
	inline static u64_t u16_u64_obfuscate(u16_t val, u64_t nonce) {
		return obfuscate<u16_t, u64_t>(val, nonce);
	}
	inline static u64_t u32_u64_obfuscate(u32_t val, u64_t nonce) {
		return obfuscate<u32_t, u64_t>(val, nonce);
	}
	inline static u8_t u8_u64_deobfuscate(u64_t val) {
		return deobfuscate<u8_t, u64_t>(val);
	}
	inline static u16_t u16_u64_deobfuscate(u64_t val) {
		return deobfuscate<u16_t, u64_t>(val);
	}
	inline static u32_t u32_u64_deobfuscate(u64_t val) {
		return deobfuscate<u32_t, u64_t>(val);
	}
}}

/*
		test code below
		int i = 0;
		netp::srand();
		while (i++ < 100000) {

			netp::u8_t u8 = netp::random_u8();
			netp::u32_t u32_ao_u8 = netp::security::u8_u32_obfuscate(u8, netp::random_u32());
			netp::u8_t u8_ = netp::security::u8_u32_deobfuscate(u32_ao_u8);
			NETP_INFO("u8: [%u], u32: %u", u8, u32_ao_u8);
			NETP_ASSERT(u8 == u8_);

			netp::u16_t u16 = netp::random_u16();
			netp::u32_t u32_ao_u16 = netp::security::u16_u32_obfuscate(u16, netp::random_u32());
			netp::u16_t u16_ = netp::security::u16_u32_deobfuscate(u32_ao_u16);
			NETP_INFO("u16: [%u], u32: %u", u16, u32_ao_u16);
			NETP_ASSERT(u16 == u16_);

			netp::u8_t u8_u64 = netp::random_u8();
			netp::u64_t u64_ao_u8 = netp::security::u8_u64_obfuscate(u8_u64, netp::random_u64());
			netp::u8_t u8_u64_ = netp::security::u8_u64_deobfuscate(u64_ao_u8);
			NETP_INFO("u8: [%u], u64: %llu", u8_u64, u64_ao_u8);
			NETP_ASSERT(u8_u64 == u8_u64_);

			netp::u16_t u16_u64 = netp::random_u16();
			netp::u64_t u64_ao_u16 = netp::security::u16_u64_obfuscate(u16_u64, netp::random_u64());
			netp::u16_t u16_u64_ = netp::security::u16_u64_deobfuscate(u64_ao_u16);
			NETP_INFO("u16: [%u], u64: %llu", u16_u64, u64_ao_u16);
			NETP_ASSERT(u16_u64 == u16_u64_);

			netp::u32_t u32 = netp::random_u32();
			netp::u64_t u64_ao_u32_ = netp::security::u32_u64_obfuscate(u32, netp::random_u64());
			netp::u32_t u32_ = netp::security::u32_u64_deobfuscate(u64_ao_u32_);
			NETP_INFO("u32: [%u], u64: %llu", u32, u64_ao_u32_);
			NETP_ASSERT(u32 == u32_);
		}
*/
#endif