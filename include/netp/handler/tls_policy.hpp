#ifndef _NETP_HANDLER_TLS_POLICY_HPP
#define _NETP_HANDLER_TLS_POLICY_HPP

#include <netp/core.hpp>


#ifdef NETP_WITH_BOTAN
#include <netp/handler/tls_config.hpp>
#include <botan/tls_policy.h>

namespace netp { namespace handler {

	class tls_policy : public Botan::TLS::Policy 
	{
		NRP<tls_config> m_tlsconfig;

	public:
		tls_policy(NRP<tls_config> const& tlsconfig) :m_tlsconfig(tlsconfig) {}

		//return true if client cert auth is required
		virtual bool require_client_certificate_authentication() const override {
			return m_tlsconfig->client_cert_auth_required;
		}

        /*
		std::vector<std::string> allowed_ciphers() const override {
            return {
                //"AES-256/OCB(12)",
                //"AES-128/OCB(12)",
                //"ChaCha20Poly1305",
                //"AES-256/GCM",
                "AES-128/GCM",
                //"AES-256/CCM",
                //"AES-128/CCM",
                //"AES-256/CCM(8)",
                //"AES-128/CCM(8)",
                //"Camellia-256/GCM",
                //"Camellia-128/GCM",
                //"ARIA-256/GCM",
                //"ARIA-128/GCM",
                //"AES-256",
                //"AES-128",
                //"Camellia-256",
                //"Camellia-128",
                //"SEED",
                //"3DES",
            };
		}
        */
	};
}}

#endif
#endif