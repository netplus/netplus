#include <netp/handler/tls_handler.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/hex.h>
#include <botan/tls_handshake_msg.h>

namespace netp { namespace handler {

	void tls_handler::closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_ctx = nullptr;

		NETP_ASSERT(m_tls_channel != nullptr);
		m_tls_channel = nullptr;
		m_flag |= f_ch_closed;

		if (m_close_p != nullptr) {
			m_close_p->set(netp::OK);
			m_close_p = nullptr;
		} 

		_do_clean();
		if (m_flag & f_ch_connected) {
			ctx->fire_closed();
		}

		if (m_flag & f_tls_ch_handshake) {
			NETP_WARN("[tls_handler]session: %s, closed in handshake state", m_session_id.c_str() );
			return;
		}

		if ((m_flag & f_tls_ch_activated) ) {
			NETP_WARN("[tls_handler]session: %s, closed in !f_tls_ch_activated state", m_session_id.c_str());
			return;
		}
		NETP_VERBOSE("[tls_handler]session: %s, closed, flag: %d", m_session_id.c_str(), m_flag );
	}

	void tls_handler::write_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_flag |= f_ch_write_closed;
		if (m_close_write_p != nullptr) {
			m_close_write_p->set(netp::OK);
			m_close_write_p = nullptr;
		}

		if (m_flag & f_ch_connected) {
			m_ctx->fire_write_closed();
		}
	}

	void tls_handler::read_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		m_flag |= f_ch_read_closed;
		if (m_flag & f_ch_connected) {
			m_ctx->fire_read_closed();
		}
	}

	void tls_handler::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT((m_flag&(f_ch_closed|f_ch_read_closed)) ==0 );
		NETP_ASSERT(m_ctx != nullptr);
		NETP_ASSERT(m_tls_channel != nullptr);

		NNASP<Botan::TLS::Channel> tls_ch = m_tls_channel;
		//receive_data might reusult in tls_emit , tls_emit might result in ch close, ch close result in a operation of set m_tls_channel to null, so we have to keep one ref first
		tls_ch->received_data((uint8_t*)income->head(), income->len());
	}

	void tls_handler::_try_tls_ch_flush() {
		if ((m_flag & f_tls_ch_write_idle) && m_outlets_to_tls_ch.size()) {
			m_flag &= ~f_tls_ch_write_idle;
			m_flag |= (f_tls_ch_writing_user_data);

			NETP_ASSERT((m_flag & f_ch_write_closed) == 0);
			NETP_ASSERT(m_outlets_to_tls_ch.size());
			tls_ch_outlet& outlet = m_outlets_to_tls_ch.front();
			NETP_ASSERT(m_tls_channel != nullptr);
			NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;
			try {
				tlsch->send((uint8_t*)outlet.data->head(), outlet.data->len());
			} catch (std::exception& e) {
				NETP_ERR("[tls]tls record write failed: %s", e.what());
				tlsch->close();
				m_flag &= ~(f_tls_ch_writing_user_data);
			}
		}
	}

	void tls_handler::_tls_ch_flush_done(int code) {
		NETP_ASSERT(m_flag & f_tls_ch_writing_user_data);
		NETP_ASSERT((m_flag & f_tls_ch_write_idle) == 0);
		NETP_ASSERT(code != netp::E_CHANNEL_WRITE_BLOCK);

		NETP_ASSERT(m_outlets_to_tls_ch.size());
		tls_ch_outlet& outlet = m_outlets_to_tls_ch.front();
		NETP_ASSERT(outlet.write_p != nullptr);
		outlet.write_p->set(netp::OK);
		m_outlets_to_tls_ch.pop();
		m_flag |= f_tls_ch_write_idle;
		m_flag &= ~(f_tls_ch_writing_user_data);
		//NETP_VERBOSE("[tls]write userdata bytes: %d, code: %d", outlet.data->len(), code);

		if (code != netp::OK) {
			NETP_WARN("[tls]write userdata failed: %d", code);
			return;
		}

		if (m_flag & f_tls_ch_close_pending && m_outlets_to_tls_ch.size() == 0) {
			NETP_ASSERT(m_tls_channel != nullptr);
			NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;
			m_flag &= ~f_tls_ch_close_pending;
			m_flag |= f_tls_ch_close_called;
			tlsch->close();
			return;
		}

		_try_tls_ch_flush();
	}

	void tls_handler::write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
		NETP_ASSERT(ctx == m_ctx);
		if ( !(m_flag& f_tls_ch_activated)) {
			chp->set(netp::E_CHANNEL_INVALID_STATE);
			return;
		}

		if (m_flag&(f_ch_write_closed|f_ch_close_called|f_ch_close_pending|f_ch_close_write_called|f_ch_close_write_pending)) {
			chp->set(netp::E_CHANNEL_WRITE_CLOSED);
			return;
		}

		m_outlets_to_tls_ch.push({ outlet, chp });
		_try_tls_ch_flush();
	}

	void tls_handler::close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) {
		if(m_flag&(f_ch_handler_close_called|f_ch_closed)) {
			chp->set(netp::E_INVALID_STATE) ;
			return;
		}
		m_flag |= f_ch_handler_close_called;

		NETP_ASSERT(ctx == m_ctx);
		NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;

		if (tlsch != nullptr && (m_flag&f_tls_ch_close_called) ==0) {
			try {
				if (m_flag & f_tls_ch_writing_user_data) {
					NETP_ASSERT(m_outlets_to_tls_ch.size());
					m_flag |= (f_tls_ch_close_pending| f_ch_close_pending);
					m_close_p = chp;
					return;
				} else {
					NETP_ASSERT(m_outlets_to_tls_ch.size() == 0);
					m_flag |= (f_tls_ch_close_called| f_ch_close_pending);
					m_close_p = chp;
					tlsch->close();
					return;
				}
			} catch (std::exception& e) {
				NETP_ERR("[tls]tls record write failed: %s", e.what());
				//tls exception, close under-layer transport anyway
				m_flag |= f_ch_close_called;
				ctx->close(chp);
				return;
			}
		}

		if (m_flag & f_ch_writing) {
			NETP_ASSERT( m_outlets_to_socket_ch.size() !=0 );
			m_flag |= f_ch_close_pending;
			m_close_p = chp;
			return;
		}

		//in case of close_write exception, ctx->close() would be triggered 
		if (m_flag & f_ch_close_called) {
			chp->set(netp::E_CHANNEL_CLOSED);
			return;
		}

		m_flag |= f_ch_close_called;
		ctx->close(chp);
	}

	void tls_handler::close_write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) {
		//flush all pending data before do ctx->close_write();
		if (m_flag & (f_ch_handler_close_write_called|f_ch_write_closed)) {
			chp->set(netp::E_INVALID_STATE);
			return;
		}
		m_flag |= f_ch_handler_close_write_called;

		NETP_ASSERT(ctx == m_ctx);
		NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;

		if (tlsch != nullptr && (m_flag & f_tls_ch_close_called) == 0) {
			try {
				if (m_flag & f_tls_ch_writing_user_data) {
					NETP_ASSERT(m_outlets_to_tls_ch.size());
					m_flag |= (f_tls_ch_close_pending | f_ch_close_write_pending);
					m_close_write_p = chp;
					return;
				} else {
					NETP_ASSERT(m_outlets_to_tls_ch.size() == 0);
					m_flag |= (f_tls_ch_close_called | f_ch_close_write_pending);
					m_close_write_p = chp;
					tlsch->close();
					return;
				}
			} catch (std::exception& e) {
				NETP_ERR("[tls]tls record write failed: %s", e.what());
				//tls exception, close under-layer transport anyway
				m_flag |= f_ch_close_called;
				ctx->close(chp);
				return;
			}
		}

		if (m_flag & f_ch_writing) {
			NETP_ASSERT(m_outlets_to_socket_ch.size() != 0);
			m_flag |= f_ch_close_write_pending;
			m_close_write_p = chp;
			return;
		}

		m_flag |= f_ch_close_write_called;
		ctx->close_write(chp);
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

	//@note: BOTH client_hello and server_hello would arrive twice, 
	void tls_handler::tls_inspect_handshake_msg(const Botan::TLS::Handshake_Message& message) {
		switch(message.type()){
		case Botan::TLS::CLIENT_HELLO:
		{
			if (m_flag & f_tls_is_client) {
				m_flag |= f_tls_client_hello_sent;
			} else {
				m_flag |= f_tls_client_hello_received;
			}
		}
		break;
		case Botan::TLS::SERVER_HELLO:
		{
			if (m_flag & f_tls_is_client) {
				m_flag |= f_tls_server_hello_received;
			} else {
				m_flag |= f_tls_server_hello_sent;
			}
		}
		break;
		case Botan::TLS::FINISHED:
		{
			//NETP_ASSERT((m_flag & f_tls_ch_handshake));
			//m_flag &= ~f_tls_ch_handshake;
		}
		break;
		}
	}

	bool tls_handler::tls_session_established(const Botan::TLS::Session& session)
	{
		//this message will arrive for multitiems
		m_flag |= f_tls_ch_established;
		if (!session.session_id().empty())
		{
			m_session_id = Botan::hex_encode(session.session_id());
		}
		if (!session.session_ticket().empty())
		{
			m_session_ticket = Botan::hex_encode(session.session_ticket());
		}
		NETP_VERBOSE("[tls_handler]tls_session_established, session_id: %s, ticket: %s, ver: %s, using: %s", m_session_id.c_str(), m_session_ticket.c_str(), session.version().to_string().c_str(), session.ciphersuite().to_string().c_str());

		//if (flag_set("print-certs"))
		{
			const std::vector<Botan::X509_Certificate>& certs = session.peer_certs();
			for (size_t i = 0; i != certs.size(); ++i)
			{
				NETP_VERBOSE("Certificate: %d/%d\n: %s\n%s", i + 1, certs.size()
				, certs[i].to_string().c_str(), certs[i].PEM_encode().c_str());
			}
		}
		return true;
	}

	void tls_handler::tls_session_activated() {
		if (m_flag & f_ch_closed) { 
			NETP_WARN("[tls_handler]tls_session_activated, but ch in closed state already: session: %s" , m_session_id.c_str() );
			return; 
		}

		NETP_ASSERT(m_ctx != nullptr, "flag: %d", m_flag );
		m_flag |= (f_tls_ch_activated|f_ch_connected);

		m_ctx->fire_connected();
	}

	void tls_handler::_socket_ch_flush_done(int code) {
		NETP_ASSERT(m_outlets_to_socket_ch.size());
		NETP_ASSERT((m_flag & f_ch_writing));
		NETP_ASSERT(code != netp::E_CHANNEL_WRITE_BLOCK);
		socket_ch_outlet& outlet = m_outlets_to_socket_ch.front();
		if (outlet.is_userdata) {
			_tls_ch_flush_done(code);
		}

		//this line should be after _tls_ch_flush_done to avoid nested _try_socket_ch_flush
		m_flag &= ~f_ch_writing;
		m_flag |= f_ch_write_idle;
		m_outlets_to_socket_ch.pop();

		if (code != netp::OK) {
			NETP_INFO("[tls]write failed: %d", code);
			//the under-layer would trigger write_closed , just return
			return;
		}

		if (m_flag &(f_ch_write_closed)) {
			return;
		}

		NETP_ASSERT(m_ctx != nullptr);
		//f_ch_close_pending
		if ((m_flag & f_ch_close_pending) && m_outlets_to_socket_ch.size() == 0) {
			//tlsch->close() might result in m_outlets_to_socket_ch.size() non zero, in that case, we'll reach here in next _socket_ch_flush_done
			NETP_ASSERT( m_flag&f_tls_ch_close_called );
			NETP_ASSERT(m_flag & f_ch_handler_close_called);
			NETP_ASSERT((m_flag&f_ch_close_called) ==0);
			m_flag &= ~(f_ch_close_pending|f_ch_close_write_pending);
			m_flag |= f_ch_close_called;

			NRP<netp::promise<int>> __p = m_close_p; //avoid nest set
			m_close_p = nullptr;
			m_ctx->close(__p);
			return;
		}

		if ((m_flag & f_ch_close_write_pending) && m_outlets_to_socket_ch.size() == 0) {
			NETP_ASSERT( m_flag & f_tls_ch_close_called);
			NETP_ASSERT( m_flag& f_ch_handler_close_write_called );
			NETP_ASSERT((m_flag & (f_ch_close_called|f_ch_close_write_called)) == 0);
			m_flag &= ~f_ch_close_write_pending;
			m_flag |= f_ch_close_write_called;

			NRP<netp::promise<int>> __p = m_close_write_p; //avoid nest set
			m_close_write_p = nullptr;
			m_ctx->close_write(__p);
			return;
		}

		_try_socket_ch_flush();
	}

	void tls_handler::_try_socket_ch_flush() {
		NETP_ASSERT( (m_flag&(f_ch_write_closed|f_ch_closed)) ==0 );
		if ( (m_flag&f_ch_write_idle) && (m_outlets_to_socket_ch.size())) {
			NETP_ASSERT( (m_flag&f_ch_writing)==0 );
			m_flag |= f_ch_writing;
			m_flag &= ~f_ch_write_idle;

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
		if (m_flag&(f_ch_write_closed|f_ch_closed)) {
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
		//@note: default policy respone with no_renegotiation
		//@after having received a
		//no_renegotiation alert that it is not willing to accept), it SHOULD
		//send a fatal alert to terminate the connection

		NETP_VERBOSE("Alert: %s", alert.type_string().c_str());
		Botan::TLS::Alert::Type t = alert.type();
		switch (t) {
		case Botan::TLS::Alert::Type::CLOSE_NOTIFY:
		{
			NETP_ASSERT(m_ctx != nullptr);
			close(netp::make_ref<netp::promise<int>>(), m_ctx);
		}
		break;
		default:
		{
			NETP_INFO("Alert: %s", alert.type_string().c_str());

			if (alert.is_fatal()) {
				//https://datatracker.ietf.org/doc/html/rfc5246#section-7.2.2, upon fatal alert, both side close transport immediately
				NETP_WARN("[tls_handler]fatal Alert: %s, do handler::close()", alert.type_string().c_str());
				NETP_ASSERT(m_ctx != nullptr);
				close(netp::make_ref<netp::promise<int>>(), m_ctx);
			}
		}
		break;
		}
	}

	void tls_handler::tls_record_received(uint64_t /*seq_no*/, const uint8_t buf[], size_t buf_size)
	{
		NETP_ASSERT(m_ctx != nullptr);
		//NETP_VERBOSE("received bytes: %d", buf_size);
		if (buf_size > 0) {
			m_ctx->fire_read(netp::make_ref<netp::packet>(buf, buf_size));
		}
	}

	void tls_handler::tls_log_error(const char* err) {
		NETP_WARN("[tls_handler]session: %s, tls error: %s", m_session_id.c_str(), err );
	}
	void tls_handler::tls_log_debug(const char* what) {
		NETP_VERBOSE("[tls_handler]session: %s, tls debug: %s", m_session_id.c_str(), what);
	}
}}

#endif