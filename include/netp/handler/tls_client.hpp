#ifndef _NETP_HANDLER_TLS_HPP
#define _NETP_HANDLER_TLS_HPP

#include <netp/core.hpp>

#ifdef NETP_WITH_BOTAN
#include <netp/handler/tls_handler.hpp>

namespace netp { namespace handler {
	class tls_client final :
		public tls_handler
	{
		public:
			tls_client(NRP<tls_context> const& tlsctx):
			tls_handler(tlsctx)
			{
				m_flag |= f_tls_is_client;
			}
			void connected(NRP<channel_handler_context> const& ctx) override;
	};
}}

#endif

#endif