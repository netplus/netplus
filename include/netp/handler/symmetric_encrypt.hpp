#ifndef _NETP_HANDLER_SYMMETRIC_ENCRYPT_HPP
#define _NETP_HANDLER_SYMMETRIC_ENCRYPT_HPP

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>

#include <netp/security/cipher_abstract.hpp>

namespace netp { namespace handler {

	//packet based encrypt/decrypt
	class symmetric_encrypt :
		public netp::channel_handler_abstract
	{
		NETP_DECLARE_NONCOPYABLE(symmetric_encrypt)
		NSP<netp::security::cipher_abstract> m_cipher ;

	public:
		symmetric_encrypt():
			channel_handler_abstract(CH_INBOUND_READ|CH_OUTBOUND_WRITE)
		{}
		virtual ~symmetric_encrypt() {}

		void assign_cipher(NSP<netp::security::cipher_abstract> const& cipher) {
			m_cipher = cipher;
		}

		NSP<netp::security::cipher_abstract> cipher() const {
			return m_cipher;
		}

		void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income ) {
			NETP_ASSERT(m_cipher != nullptr);
			NRP<packet> decrypted;
			int decryptrt = m_cipher->decrypt(income, decrypted);
			NETP_ASSERT(decryptrt == netp::OK);
			ctx->fire_read(decrypted);
		}

		void write(NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet, NRP<promise<int>> const& chp) {
			NETP_ASSERT(outlet != nullptr);
			NETP_ASSERT(m_cipher != nullptr);

			NRP<packet> encrypted;
			int rt = m_cipher->encrypt(outlet, encrypted);
			NETP_ASSERT(rt == netp::OK);

			ctx->write(encrypted, chp);
		}
	};
}}
#endif