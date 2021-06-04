#ifndef _NETP_SECURITY_XXTEA_HPP_
#define _NETP_SECURITY_XXTEA_HPP_

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/security/cipher_abstract.hpp>

//borrow from: https://en.wikipedia.org/wiki/XXTEA
//published by David Wheeler and Roger Needham
#define NETP_XXTEA_DELTA 0x9e3779b9
#define NETP_XXTEA_MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

inline static void wikipedia_btea_encrypt(netp::u32_t *v, netp::u32_t const& n, netp::u32_t key[4] ) {
	NETP_ASSERT(sizeof(netp::u32_t) == 4);
	netp::u32_t y, z, sum;
	unsigned p, rounds, e;
	rounds = 6 + 52 / n;
	sum = 0;
	z = v[n - 1];
	do {
		sum += NETP_XXTEA_DELTA;
		e = (sum >> 2) & 3;
		for (p = 0; p<n - 1; p++) {
			y = v[p + 1];
			z = v[p] += NETP_XXTEA_MX;
		}
		y = v[0];
		z = v[n - 1] += NETP_XXTEA_MX;
	} while (--rounds);
}
inline static void wikipedia_btea_decrypt(netp::u32_t *v, netp::u32_t const& n, netp::u32_t key[4]) {
	NETP_ASSERT(sizeof(netp::u32_t) == 4);
	netp::u32_t y, z, sum;
	unsigned p, rounds, e;
	rounds = 6 + 52 / n;
	sum = rounds*NETP_XXTEA_DELTA;
	y = v[0];
	do {
		e = (sum >> 2) & 3;
		for (p = n - 1; p>0; p--) {
			z = v[p - 1];
			y = v[p] -= NETP_XXTEA_MX;
		}
		z = v[n - 1];
		y = v[0] -= NETP_XXTEA_MX;
		sum -= NETP_XXTEA_DELTA;
	} while (--rounds);
}

inline static void xxtea_to_bytes_array(netp::byte_t* arr, netp::u32_t const& alength, netp::u32_t const* const data, netp::u32_t const& dlen) {
	NETP_ASSERT(arr != nullptr);
	NETP_ASSERT(data != nullptr);
	NETP_ASSERT((dlen>>2)<=alength);

#if __NETP_IS_LITTLE_ENDIAN
//	if (netp::is_little_endian()) {
		std::memcpy(arr, (netp::byte_t*)data, alength);
//		return;
//	}
#else
	for (netp::u32_t i = 0; i < alength; ++i) {
		arr[i] = (netp::byte_t)(data[i >> 2] >> ((i & 3) << 3));
	}
#endif
	(void)dlen;
	(void)alength;
}

inline static void xxtea_to_uint32_array(netp::u32_t* arr, netp::u32_t const& alength, netp::byte_t const* const data, netp::u32_t const& dlen) {
	NETP_ASSERT(arr != nullptr);
	NETP_ASSERT(alength >= dlen >> 2);
	NETP_ASSERT(data != nullptr);
	//if (netp::is_little_endian()) {
#if __NETP_IS_LITTLE_ENDIAN
		std::memcpy(arr, data, dlen);
	//}
#else
	for (netp::u32_t i = 0; i < dlen; ++i) {
		arr[i >> 2] |= (netp::u32_t)data[i] << ((i & 3) << 3);
	}
#endif
	(void)alength;
}

namespace netp { namespace security {

	struct xxtea_ctx {
		u32_t encrypted_data_length;
		u32_t decrypted_data_length;
		byte_t* encrypted_data;
		byte_t* decrypted_data;
	};

	//different endian on both side is supported
	inline static xxtea_ctx* xxtea_encrypt( byte_t const* const data, netp::u32_t const& dlen, byte_t const* const key, netp::u32_t const& klen,int& ec ) {
		NETP_ASSERT(key != nullptr);
		NETP_ASSERT(klen > 0);

		ec = netp::OK;
		u32_t u32k[4] = {0};
		byte_t tmp[16] = {0};
		::memcpy(tmp, key, NETP_MIN(klen, sizeof(u32k)));

		xxtea_to_uint32_array(u32k,4,tmp,16);

		netp::u32_t u32len = ((dlen & 3) == 0) ? (dlen >> 2) : ((dlen >> 2) + 1);
		u32_t* u32data = netp::allocator<u32_t>::calloc(u32len +1) ;
		if (u32data == 0) {
			ec = netp::E_MEMORY_ALLOC_FAILED;
			return 0;
		}
		*(u32data + u32len) = dlen;
		xxtea_to_uint32_array(u32data,u32len,data,dlen);
		wikipedia_btea_encrypt(u32data, u32len+1, u32k);

		xxtea_ctx* ctx = netp::allocator<xxtea_ctx>::calloc(1);
		if (ctx == nullptr) {
			netp::allocator<u32_t>::free(u32data);
			ec = netp::E_MEMORY_ALLOC_FAILED;
			return nullptr;
		}

		ctx->encrypted_data_length = (u32len + 1) << 2;
		ctx->encrypted_data = netp::allocator<byte_t>::calloc( ctx->encrypted_data_length);
		if (ctx->encrypted_data == nullptr) {
			netp::allocator<u32_t>::free(u32data);
			netp::allocator<xxtea_ctx>::free(ctx);
			ec = netp::E_MEMORY_ALLOC_FAILED;
			return nullptr;
		}

		xxtea_to_bytes_array(ctx->encrypted_data,ctx->encrypted_data_length,u32data,u32len);
		netp::allocator<u32_t>::free(u32data);
		return ctx;
	}

	inline static xxtea_ctx* xxtea_decrypt(byte_t const* const data, netp::u32_t const& dlen, byte_t const* const key, netp::u32_t const& klen, int& ec) {
		NETP_ASSERT(key != nullptr);
		NETP_ASSERT(klen > 0);

		ec = netp::OK;
		//invalid length check
		if ((dlen&3) != 0) {
			ec = netp::E_XXTEA_INVALID_DATA;
			return nullptr;
		}

		u32_t u32k[4] = {0};
		byte_t tmp[16] = {0};
		::memcpy(tmp, key, NETP_MIN(klen, sizeof(u32k)));
		xxtea_to_uint32_array(u32k, 4, tmp, 16);

		u32_t u32len = dlen >> 2;
		u32_t* u32data = (u32_t*)netp::allocator<u32_t>::calloc(u32len);
		if (u32data == nullptr) {
			ec = netp::E_MEMORY_ALLOC_FAILED;
			return nullptr;
		}
		xxtea_to_uint32_array(u32data,u32len,data,dlen);
		wikipedia_btea_decrypt(u32data, u32len, u32k);

		if (*(u32data + u32len - 1) > dlen) {
			//invalid data
			netp::allocator<u32_t>::free(u32data);
			ec = netp::E_XXTEA_INVALID_DATA;
			return nullptr;
		}

		xxtea_ctx* ctx = (xxtea_ctx*)netp::allocator<xxtea_ctx>::calloc(1);
		if (ctx == nullptr) {
			netp::allocator<u32_t>::free(u32data);
			ec = netp::E_MEMORY_ALLOC_FAILED;
			return nullptr;
		}

		ctx->decrypted_data_length = *(u32data + u32len - 1);
		ctx->decrypted_data = netp::allocator<byte_t>::calloc(ctx->decrypted_data_length);
		if (ctx->decrypted_data == nullptr) {
			ec = netp::E_MEMORY_ALLOC_FAILED;
			netp::allocator<xxtea_ctx>::free(ctx);
			netp::allocator<u32_t>::free(u32data);
			return nullptr;
		}

		xxtea_to_bytes_array(ctx->decrypted_data, ctx->decrypted_data_length, u32data, (u32len-1) );
		netp::allocator<u32_t>::free(u32data);
		return	ctx;
	}

	inline static void xxtea_free(xxtea_ctx* ctx) {
		NETP_ASSERT( ctx != nullptr );
		if (ctx->encrypted_data) {
			netp::allocator<byte_t>::free(ctx->encrypted_data);
		}
		if (ctx->decrypted_data) {
			netp::allocator<byte_t>::free(ctx->decrypted_data);
		}
		netp::allocator<xxtea_ctx>::free(ctx);
	}

//#define NETP_DEBUG_DH_XXTEA_KEY

	class xxtea:
		public cipher_abstract
	{

	private:
		byte_t m_key[16];

	public:
		xxtea()
		{
			byte_t k[16] = { 0 };
			set_key( k, 16);
		}
		xxtea( byte_t* k, netp::u32_t const& klen ) {
			set_key(k,klen);
		}
		~xxtea() {}

		void set_key(byte_t const* const k, netp::u32_t const& klen) {
			if (k == nullptr || klen == 0) {
				NETP_THROW("invalid xxtea key");
			}
			::memcpy(m_key, k, klen );
			::memset(m_key + klen, 'z', sizeof(m_key) - klen);
		}

		int encrypt(NRP<packet> const& in, NRP<packet>& out ) const {
#ifdef NETP_DEBUG_DH_XXTEA_KEY
			char key_cstr[64] = { 0 };
			snprintf(key_cstr, 64, "%d%d%d%d-%d%d%d%d-%d%d%d%d-%d%d%d%d"
				, m_key[0], m_key[1], m_key[2], m_key[3]
				, m_key[4], m_key[5], m_key[6], m_key[7]
				, m_key[8], m_key[9], m_key[10], m_key[11]
				, m_key[12], m_key[13], m_key[14], m_key[15]
			);
			NETP_VERBOSE("[xxtea]encrypt using key: %s", key_cstr );
#endif
			int ec;
			xxtea_ctx* tea = xxtea_encrypt(in->head(), (netp::u32_t)in->len(), m_key, sizeof(m_key)/sizeof(m_key[0]), ec );
			NETP_ASSERT(ec == netp::OK);
			if (tea == nullptr) {
				return netp::E_XXTEA_ENCRYPT_FAILED;
			}

			NRP<packet> encrypted = netp::make_ref<packet>(tea->encrypted_data_length);
			encrypted->write(tea->encrypted_data, tea->encrypted_data_length);
			xxtea_free(tea);
			out.swap(encrypted);

			return ec;
		}

		int decrypt(NRP<packet> const& in, NRP<packet>& out ) const {
#ifdef NETP_DEBUG_DH_XXTEA_KEY
			char key_cstr[64] = { 0 };
			snprintf(key_cstr, 64, "%d%d%d%d-%d%d%d%d-%d%d%d%d-%d%d%d%d"
				, m_key[0], m_key[1], m_key[2], m_key[3]
				, m_key[4], m_key[5], m_key[6], m_key[7]
				, m_key[8], m_key[9], m_key[10], m_key[11]
				, m_key[12], m_key[13], m_key[14], m_key[15]
			);
			NETP_VERBOSE("[xxtea]decrypt using key: %s", key_cstr);
#endif
			int ec;
			xxtea_ctx* tea = xxtea_decrypt(in->head(), (netp::u32_t)in->len(), m_key, sizeof(m_key)/sizeof(m_key[0]), ec );
			NETP_ASSERT(ec == netp::OK);
			if (tea == nullptr) {
				return netp::E_XXTEA_DECRYPT_FAILED;
			}

			NRP<packet> decrypted = netp::make_ref<packet>(tea->decrypted_data_length);
			decrypted->write(tea->decrypted_data, tea->decrypted_data_length);
			xxtea_free(tea);
			out.swap(decrypted);

			return ec;
		}
	};
}}
#endif