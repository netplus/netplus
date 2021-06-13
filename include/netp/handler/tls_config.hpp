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

		std::string ca_path;
		std::string cert;
		std::string privkey;

		std::string host;
		u16_t port;

		tls_config():
			client_cert_auth_required(false),
			cert_verify_required(false),
			ca_path(),
			cert(),
			privkey(),
			host(),
			port(0)
		{}
	};
}}
#endif