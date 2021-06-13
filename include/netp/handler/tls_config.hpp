#ifndef _NETP_HANDLER_TLS_CONFIG_HPP
#define _NETP_HANDLER_TLS_CONFIG_HPP

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

namespace netp { namespace handler {

	struct tls_config:
		public netp::ref_base 
	{
		bool client_cert_auth_required;
		bool cert_verify_required;

		tls_config():
			client_cert_auth_required(false),
			cert_verify_required(false)
		{}
	};
}}
#endif