#include <netp/handler/tls_server.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/tls_server.h>

namespace netp { namespace handler {

	tls_server::tls_server(NRP<tls_context> const& tlsctx) :
		tls_handler(tlsctx)
	{}

	void tls_server::connected(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx == nullptr);
		NETP_ASSERT(m_tls_ctx != nullptr);
		NETP_ASSERT(m_tls_channel == nullptr);
		m_flag &= ~(f_ch_closed|f_ch_read_closed|f_ch_write_closed);
		
		m_ctx = ctx;
		m_tls_channel = netp::non_atomic_shared::make<Botan::TLS::Server>(*this,
			*(m_tls_ctx->session_mgr),
			*(m_tls_ctx->credentials_mgr),
			*(m_tls_ctx->policy),
			*(m_tls_ctx->rng),
			false /*is_not_udp*/
		);
	}
}}
#endif