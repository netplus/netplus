#include <netp/handler/tls_handler.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/hex.h>

namespace netp { namespace handler {

	void tls_handler::closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_ctx = nullptr;

		NETP_ASSERT(m_tls_channel != nullptr);
		m_tls_channel = nullptr;
		m_flag |= f_closed;
		_do_clean();
		if (m_flag & f_connected) {
			ctx->fire_closed();
		}
	}

	void tls_handler::write_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_flag |= f_write_closed;
		if (m_flag & f_connected) {
			m_ctx->fire_write_closed();
		}
	}

	void tls_handler::read_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_flag |= f_read_closed;
		if (m_flag & f_connected) {
			m_ctx->fire_read_closed();
		}
	}

	void tls_handler::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT((m_flag&(f_closed|f_read_closed)) ==0 );
		NETP_ASSERT(m_ctx != nullptr);
		NETP_ASSERT(m_tls_channel != nullptr);
		m_tls_channel->received_data((uint8_t*)income->head(), income->len());
	}

	void tls_handler::write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
		NETP_ASSERT(ctx == m_ctx);
		if ( !(m_flag& f_tls_ch_activated)) {
			chp->set(netp::E_CHANNEL_INVALID_STATE);
			return;
		}

		if (m_flag&f_write_closed) {
			chp->set(netp::E_CHANNEL_WRITE_CLOSED);
			return;
		}
		m_outlets_to_tls_ch.push({ outlet, chp });
		_try_tls_ch_flush();
	}

	void tls_handler::close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(ctx == m_ctx);
		if (m_tls_channel != nullptr) {
			try {
				m_tls_channel->close();
			} catch (std::exception& e) {
				NETP_ERR("[tls]tls record write failed: %s", e.what());
			}
		}
		ctx->close();
	}

	//@NOTE: all error would be processed here
	void tls_handler::_do_clean() {
		while (m_outlets_to_tls_ch.size()) {
			tls_ch_outlet& outlet = m_outlets_to_tls_ch.front();
			NETP_WARN("[tls]cancel write, nbytes: %u", outlet.data->len());
			outlet.write_p->set(netp::E_CHANNEL_CLOSED);
			m_outlets_to_tls_ch.pop();
		}
	}

	void tls_handler::_try_tls_ch_flush() {
		if ((m_flag&f_tls_ch_write_idle) && m_outlets_to_tls_ch.size()) {
			m_flag &= ~f_tls_ch_write_idle;
			m_flag |= (f_tls_ch_writing_user_data);

			NETP_ASSERT(m_outlets_to_tls_ch.size());
			tls_ch_outlet& outlet = m_outlets_to_tls_ch.front();
			try {
				m_tls_channel->send((uint8_t*)outlet.data->head(), outlet.data->len());
			} catch (std::exception& e) {
				NETP_ERR("[tls]tls record write failed: %s", e.what());
				m_tls_channel->close();
				m_flag &= ~( f_tls_ch_writing_user_data);
			}
		}
	}

	void tls_handler::_tls_ch_flush_done(int code) {
		NETP_ASSERT( m_flag&f_tls_ch_writing_user_data);
		NETP_ASSERT(code != netp::E_CHANNEL_WRITE_BLOCK);

		NETP_ASSERT(m_outlets_to_tls_ch.size());
		tls_ch_outlet& outlet = m_outlets_to_tls_ch.front();
		NETP_ASSERT(outlet.write_p != nullptr);
		outlet.write_p->set(netp::OK);
		m_outlets_to_tls_ch.pop();
		m_flag &= ~(f_tls_ch_writing_user_data);

		if (code != netp::OK) {
			NETP_WARN("[tls]write appdata failed: %d", code);
			return;
		}
		_try_tls_ch_flush();
	}

	bool tls_handler::tls_session_established(const Botan::TLS::Session& session)
	{
		m_flag |= f_tls_ch_established;

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

	void tls_handler::tls_session_activated() {
		NETP_ASSERT(m_ctx != nullptr);
		m_flag |= (f_tls_ch_activated|f_connected);
		m_ctx->fire_connected();
	}

	void tls_handler::_socket_ch_flush_done(int code) {
		NETP_ASSERT(m_outlets_to_socket_ch.size());
		NETP_ASSERT((m_flag & f_writing));
		NETP_ASSERT(code != netp::E_CHANNEL_WRITE_BLOCK);
		socket_ch_outlet& outlet = m_outlets_to_socket_ch.front();
		if (outlet.is_userdata) {
			_tls_ch_flush_done(code);
		}

		//this line should be after _tls_ch_flush_done to avoid nested _try_socket_ch_flush
		m_flag &= ~f_writing;
		m_flag |= f_write_idle;
		m_outlets_to_socket_ch.pop();

		if (code != netp::OK) {
			NETP_INFO("[tls]write failed: %d", code);
			return;
		}
		if (!(m_flag & f_write_closed)) {
			_try_socket_ch_flush();
		}
	}

	void tls_handler::_try_socket_ch_flush() {
		NETP_ASSERT( (m_flag&(f_closed|f_write_closed)) ==0 );
		if ( (m_flag&f_write_idle) && (m_outlets_to_socket_ch.size())) {
			NETP_ASSERT( (m_flag&f_writing)==0 );
			m_flag |= f_writing;
			m_flag &= ~f_write_idle;

			socket_ch_outlet& outlet = m_outlets_to_socket_ch.front();
			NRP<netp::promise<int>> f = netp::make_ref<netp::promise<int>>();
			f->if_done([TLS_H = NRP<tls_handler>(this), L = m_ctx->L](int const& rt) {
				NETP_ASSERT(L->in_event_loop());
				TLS_H->_socket_ch_flush_done(rt);
			});

			m_ctx->write(f,outlet.data);
		}
	}

	void tls_handler::tls_emit_data(const uint8_t buf[], size_t length) 
	{
		if (m_flag&(f_closed|f_write_closed)) {
			NETP_WARN("[tls]handler shutdown already, ignore write, nbytes: %u", length);
			NETP_ASSERT((m_flag & (f_tls_ch_writing_user_data)) != (f_tls_ch_writing_user_data));
			return;
		}

		NETP_ASSERT(m_ctx != nullptr);
		m_outlets_to_socket_ch.push({ netp::make_ref<netp::packet>(buf, length) , (m_flag&f_tls_ch_writing_user_data)!=0 });
		_try_socket_ch_flush();
	}

	void tls_handler::tls_alert(Botan::TLS::Alert alert)
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

	void tls_handler::tls_record_received(uint64_t /*seq_no*/, const uint8_t buf[], size_t buf_size)
	{
		NETP_ASSERT(m_ctx != nullptr);
		NETP_DEBUG("received bytes: %d", buf_size);
		if (buf_size > 0) {
			m_ctx->fire_read(netp::make_ref<netp::packet>(buf, buf_size));
		}
	}
}}

#endif