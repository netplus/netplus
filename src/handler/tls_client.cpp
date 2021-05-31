#include <netp/logger_broker.hpp>
#include <netp/handler/tls_client.hpp>

#ifdef NETP_WITH_BOTAN

#include <botan/tls_client.h>
#include <botan/hex.h>

namespace netp { namespace handler {

	void tls_client::connected(NRP<channel_handler_context> const& ctx)  {
		NETP_ASSERT(m_ctx == nullptr);
		NETP_ASSERT(m_tls_context != nullptr);
		NETP_ASSERT(m_tls_client == nullptr);

		m_ctx = ctx;
		m_tls_client = netp::make_shared<Botan::TLS::Client>(*this, 
				*(m_tls_context->session_mgr),
				*(m_tls_context->credentials_mgr),
				*(m_tls_context->policy),
				*(m_tls_context->rng),
				*(m_tls_context->server_info),
				m_tls_context->tls_version,
				m_tls_context->next_protocols
			);

		m_state = tls_state::S_HANDSHAKE;
	}

	void tls_client::closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_ctx = nullptr;
		
		NETP_ASSERT(m_tls_client != nullptr);
		m_tls_client = nullptr;
		m_state = tls_state::S_CLOSED;
		__tls_do_clean();
		ctx->fire_closed();
	}

	void tls_client::write_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_ctx->close();
	}

	void tls_client::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT(m_ctx != nullptr);
		NETP_ASSERT(m_tls_client != nullptr);
		m_tls_client->received_data((uint8_t*)income->head(), income->len());
	}

	void tls_client::write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet ) {
			NETP_ASSERT( ctx == m_ctx);
			if (m_state != tls_state::S_TRANSFER) {
				chp->set(netp::E_CHANNEL_INVALID_STATE);
				return;
			};

			if(m_write_state == tls_write_state::S_WRITE_SHUTDOWN) {
				chp->set(netp::E_CHANNEL_WRITE_CLOSED);
				return;
			}
			m_outlets.push({outlet, chp});
			__tls_try_appdata_flush();
	}

	//@NOTE: all error would be processed here
	void tls_client::__tls_do_clean() {
		while (m_outlets.size()) {
			tls_outlet& outlet = m_outlets.front();
			NETP_WARN("[tls]cancel write, nbytes: %u", outlet.outlet->len() );
			outlet.write_p->set(netp::E_CHANNEL_CLOSED);
			m_outlets.pop();
		}
	}

	void tls_client::__tls_try_appdata_flush() {
		if( (m_write_state == tls_write_state::S_WRITE_IDLE) && m_outlets.size() ) {
			m_write_state = tls_write_state::S_APPDATE_WRITE_PREPARE;
			NETP_ASSERT( m_outlets.size() );
			tls_outlet& out = m_outlets.front();
			try{
				m_tls_client->send( (uint8_t*)out.outlet->head(), out.outlet->len() );
			} catch(std::exception& e) {
				NETP_ERR("[tls]tls record write failed: %s", e.what() );
				m_tls_client->close();
			}

			//NETP_ASSERT(m_write_state == tls_write_state::S_APPDATE_WRITING);
		}
	}

	void tls_client::__tls_do_write_appdata_done(int code) {
		NETP_ASSERT(m_write_state == tls_write_state::S_APPDATE_WRITING);
		m_write_state = tls_write_state::S_WRITE_IDLE;

		if(code == netp::E_CHANNEL_WRITE_BLOCK) {return;}
		if(code != netp::OK) {
			NETP_WARN("[tls]write appdata failed: %d", code );
		}

		NETP_ASSERT( m_outlets.size() );
		tls_outlet& outlet = m_outlets.front();
		NETP_ASSERT(outlet.write_p != nullptr);
		outlet.write_p->set(netp::OK);
		m_outlets.pop();

		__tls_try_interleave_flush();
		__tls_try_appdata_flush();
	}

	void tls_client::__tls_try_interleave_flush() {
		if (m_write_state == tls_write_state::S_WRITE_IDLE && (m_interleave_outlets.size())) {
			m_write_state = tls_write_state::S_INTERLEAVE_WRITING;
			NRP<netp::packet>& outp = m_interleave_outlets.front();
			NRP<netp::promise<int>> f = m_ctx->write(outp);
			f->if_done([TLS_H = NRP<tls_client>(this), L = m_ctx->L](int const& rt) {
				NETP_ASSERT(L->in_event_loop());
				TLS_H->__tls_do_write_interleave_done(rt);
			});
		}
	}

	void tls_client::__tls_do_write_interleave_done(int code) {
		NETP_ASSERT(m_interleave_outlets.size());
		NETP_ASSERT(m_write_state == tls_write_state::S_INTERLEAVE_WRITING);
		m_write_state = tls_write_state::S_WRITE_IDLE;
		if (code == netp::E_CHANNEL_WRITE_BLOCK) {
			return ;
		}
		if (code != netp::OK) {
			NETP_INFO("[tls]write failed: %d", code);
			return ;
		}
		m_interleave_outlets.pop();
		__tls_try_interleave_flush();
		__tls_try_appdata_flush();
	}
	
	bool tls_client::tls_session_established(const Botan::TLS::Session& session)
	{
		NETP_DEBUG("Handshake complete, %s, using: %s", session.version().to_string().c_str(), session.ciphersuite().to_string().c_str());
		if (!session.session_id().empty())
		{
			NETP_DEBUG("Session ID: %s", Botan::hex_encode(session.session_id()).c_str());
		}

		if (!session.session_ticket().empty())
		{
			NETP_DEBUG("Session ticket: %s", Botan::hex_encode(session.session_ticket()).c_str());
		}

		//if (flag_set("print-certs"))
		{
			const std::vector<Botan::X509_Certificate>& certs = session.peer_certs();
			for (size_t i = 0; i != certs.size(); ++i)
			{
				NETP_DEBUG("Certificate: %d/%d\n: %s\n%s", i + 1, certs.size()
					, certs[i].to_string().c_str(), certs[i].PEM_encode().c_str());
			}
		}
		return true;
	}

	void tls_client::tls_session_activated() {
		NETP_ASSERT(m_ctx != nullptr);
		m_state = tls_state::S_TRANSFER;
		m_ctx->fire_connected();
	}

	void tls_client::tls_emit_data(const uint8_t buf[], size_t length)
	{
		if (m_write_state == tls_write_state::S_WRITE_SHUTDOWN) {
			NETP_WARN("[tls]handler shutdown already, ignore write, nbytes: %u", length);
			return;
		}

		NETP_ASSERT(m_ctx != nullptr);
		NRP<netp::packet> outp = netp::make_ref<netp::packet>(buf, length);
		if(m_write_state == tls_write_state::S_APPDATE_WRITE_PREPARE) {
			m_write_state = tls_write_state::S_APPDATE_WRITING;
			NRP<netp::promise<int>> f = m_ctx->write(outp);
			f->if_done([TLS_H=NRP<tls_client>(this), L=m_ctx->L](int const& rt) {
				NETP_ASSERT(L->in_event_loop());
				TLS_H->__tls_do_write_appdata_done(rt);
			});
			return ;
		}

		m_interleave_outlets.push(outp);
		__tls_try_interleave_flush();
	}

	void tls_client::tls_alert(Botan::TLS::Alert alert)
	{
		NETP_DEBUG("Alert: %s", alert.type_string().c_str());
		Botan::TLS::Alert::Type t = alert.type();
		switch (t) {
			case Botan::TLS::Alert::Type::CLOSE_NOTIFY:
			{
				m_ctx->close();
			}
			break;
			default:
			{
				NETP_WARN("Alert: %s", alert.type_string().c_str());
			}
			break;
		}
	}

	void tls_client::tls_record_received(uint64_t /*seq_no*/, const uint8_t buf[], size_t buf_size)
	{
		NETP_ASSERT(m_ctx != nullptr);
		NETP_DEBUG("received bytes: %d", buf_size );
		if(buf_size>0) {
			NRP<netp::packet> in = netp::make_ref<netp::packet>(buf_size);
			in->write( buf, buf_size );
			m_ctx->fire_read(in);
		}
	}

}}

#endif