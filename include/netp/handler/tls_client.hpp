#ifndef _NETP_HANDLER_TLS_CLIENT_HPP
#define _NETP_HANDLER_TLS_CLIENT_HPP

#include <netp/core.hpp>

#ifdef NETP_WITH_BOTAN
#include <netp/handler/tls_handler.hpp>

namespace netp { namespace handler {
	class tls_client final :
		public tls_handler
	{
		public:
			tls_client(NRP<tls_context> const& tlsctx);
			void connected(NRP<channel_handler_context> const& ctx) override;
	};
}}

#endif

#endif