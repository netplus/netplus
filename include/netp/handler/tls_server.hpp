#ifndef _NETP_HANDLER_TLS_SERVER_HPP
#define _NETP_HANDLER_TLS_SERVER_HPP

#include <netp/core.hpp>

#ifdef NETP_WITH_BOTAN
#include <netp/handler/tls_handler.hpp>

namespace netp { namespace handler {
		class tls_server final :
			public tls_handler
		{
		public:
			tls_server(NRP<tls_context> const& tlsctx);
			virtual ~tls_server();
			void connected(NRP<channel_handler_context> const& ctx) override;
		};
	}
}

#endif

#endif