#include <netp/handler/tls_client.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/tls_client.h>

namespace netp { namespace handler {

	void tls_client::connected(NRP<channel_handler_context> const& ctx)  {
		NETP_ASSERT(m_ctx == nullptr);
		NETP_ASSERT(m_tls_ctx != nullptr);
		NETP_ASSERT(m_tls_channel == nullptr);

		m_flag &= ~f_closed;
		m_flag |= f_tls_ch_handshake;

		m_ctx = ctx;
		m_tls_channel = netp::make_shared<Botan::TLS::Client>(*this,
				*(m_tls_ctx->session_mgr),
				*(m_tls_ctx->credentials_mgr),
				*(m_tls_ctx->policy),
				*(m_tls_ctx->rng),
				*(m_tls_ctx->server_info),
				m_tls_ctx->tls_version,
				m_tls_ctx->next_protocols
		);
	}

}}

#endif