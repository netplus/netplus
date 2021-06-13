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
	};
}}

#endif
#endif