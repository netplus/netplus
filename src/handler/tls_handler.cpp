#include <netp/handler/tls_handler.hpp>
#include <netp/socket_channel.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/hex.h>
#include <botan/tls_handshake_msg.h>
#include <botan/tls_exceptn.h>

namespace netp { namespace handler {

	tls_handler::tls_handler(NRP<tls_context> const& tlsctx) :
		channel_handler_abstract(CH_ACTIVITY_CONNECTED | CH_ACTIVITY_CLOSED | CH_ACTIVITY_READ_CLOSED | CH_ACTIVITY_WRITE_CLOSED | CH_INBOUND_READ | CH_OUTBOUND_WRITE | CH_OUTBOUND_CLOSE | CH_OUTBOUND_CLOSE_WRITE),
		m_flag(f_ch_closed | f_ch_write_closed | f_ch_read_closed),
		m_tls_channel(nullptr),
		m_ctx(nullptr),
		m_tls_ctx(tlsctx)
	{}

	tls_handler::~tls_handler() {}

	void tls_handler::closed(NRP<channel_handler_context> const& ctx) {

		m_flag &= ~f_ch_close_pending;
		m_flag |= f_ch_closed;
		if (m_close_p != nullptr) {
			m_close_p->set(netp::OK);
			m_close_p = nullptr;
		}
		NETP_ASSERT( m_close_write_p == nullptr );

		if (m_flag&f_ch_connected) {
			ctx->fire_closed();
		} else {
			NETP_ERR("[tls_handler][#%s]tls handshake failed", ctx->ch->ch_info().c_str());
			ctx->fire_error(netp::E_TLS_HANDSHAKE_FAILED);
		}

		NETP_ASSERT(m_tls_channel != nullptr);
		m_tls_channel = nullptr;

		// tls_handler::read migh result in m_ctx->close(), (write handshake failed with write error);
		// if m_tls_ctx is reset during read, we got memory issue
		// suggestion to botan team: botan should use ptr for the channel context, not reference
		// m_tls_ctx = nullptr;
		// but this ptr is safe to be derefrenced untile the next round right after this api [closed()]
		NETP_ASSERT(m_ctx != nullptr);
		m_ctx = nullptr;
	}

	void tls_handler::write_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		NETP_ASSERT(m_ctx == ctx);
		m_flag &= ~f_ch_close_write_pending;
		m_flag |= f_ch_write_closed;

		if (m_tls_outlets_records_data.size()) {
			while (m_tls_outlets_records_data.size()) {
				//just clear it
				m_tls_outlets_records_data.pop();
			}
			NETP_WARN("[tls_handler]cancel tls records: %u", m_tls_outlets_records_data.size());
		}

		while (m_tls_outlets_user_data.size()) {
			tls_ch_outlet& outlet = m_tls_outlets_user_data.front();
			NETP_WARN("[tls]cancel write, nbytes: %u", outlet.data->len());
			outlet.write_p->set(netp::E_CHANNEL_CLOSED);
			m_tls_outlets_user_data.pop();
		}

		if (m_close_write_p != nullptr) {
			m_close_write_p->set(netp::OK);
			m_close_write_p = nullptr;
		}

		if (m_flag&f_ch_connected) {
			m_ctx->fire_write_closed();
		}
	}

	void tls_handler::read_closed(NRP<channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx != nullptr);
		NETP_ASSERT(m_ctx == ctx);
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

		try {
			tls_ch->received_data((uint8_t*)income->head(), income->len());
		} catch (Botan::TLS::TLS_Exception& e) {
			NETP_ERR("[tls_handler][#%s]tls excepton, code: %d, err: %s", ctx->ch->ch_info().c_str(), e.error_code(), e.what());
			ctx->close();
		} catch (std::exception& e) {
			NRP<netp::socket_channel> ch_ = netp::static_pointer_cast<netp::socket_channel>( ctx->ch );
			NETP_ERR("[tls_handler][#%s]tls excepton, err: %s", ctx->ch->ch_info().c_str(), e.what() );
			ctx->close();
		} catch (...) {
			NETP_ERR("[tls_handler][#%s]unknown tls exception", ctx->ch->ch_info().c_str() );
			ctx->close();
		}
	}

	void tls_handler::write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
		//we should not get here if f_tls_ch_activated not set
		NETP_ASSERT((m_flag & (f_tls_ch_activated | f_ch_connected)) == (f_tls_ch_activated | f_ch_connected));
		if (m_flag & (f_tls_handler_close_called | f_tls_handler_close_write_called | f_ch_closed | f_ch_write_closed)) {
			chp->set(netp::E_CHANNEL_WRITE_CLOSED);
			return;
		}

		NETP_ASSERT(ctx == m_ctx);
		m_tls_outlets_user_data.push({ outlet, chp, 0 });
		if ( (m_flag&f_tls_ch_writing) == 0 ) {
			_try_tls_user_data_flush();
		}
	}


	//close/close_write strategy
	//1) user call close, set a tls_close_pending
	//2) flush all user data if any
	//3) if there is no more user data, call tls_channel->close(), set a ch_close_pending/ch_close_write_pending

	//any tls exception result in a action to call ctx->close (close socket)
	//any pending user data would be checked in write_closed notification

	void tls_handler::close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) {
		//no connection fired, no outer reference to this handler
		NETP_ASSERT( (m_flag & (f_tls_ch_activated | f_ch_connected)) == (f_tls_ch_activated | f_ch_connected) );

		if (m_flag & (f_tls_handler_close_called | f_ch_closed)) {
			chp->set(netp::E_CHANNEL_CLOSED);
			return;
		}
		m_flag |= f_tls_handler_close_called;

		NETP_ASSERT(m_close_p == nullptr);
		NETP_ASSERT(ctx == m_ctx);
		if (m_flag & f_tls_ch_writing) {
			NETP_ASSERT(m_tls_outlets_user_data.size());
			m_flag |= f_tls_ch_close_pending;
			m_close_p = chp;
			return;
		}

		NETP_ASSERT(m_tls_outlets_user_data.size() == 0);
		NETP_ASSERT(m_tls_channel != nullptr);
		NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;

		if (m_flag& f_tls_channel_close_notified) {
			//if ctx->closed already , no side effect, just do it
			ctx->close(chp);
			return;
		}

		try {
			m_flag |= (f_tls_channel_close_notified | f_ch_close_pending);
			m_close_p = chp; //not in f_ch_closed, we shall have a chance to set
			tlsch->close();
			return;
		} catch (std::exception& e) {
			NETP_ERR("[tls]tls record write failed: %s", e.what());
			m_close_p = nullptr; 
			ctx->close(chp);
			return;
		}
	}

	void tls_handler::close_write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) {
		//no connection fired, no outer reference to this handler
		NETP_ASSERT((m_flag & (f_tls_ch_activated | f_ch_connected)) == (f_tls_ch_activated | f_ch_connected));

		//flush all pending data before do ctx->close_write();
		if (m_flag & (f_tls_handler_close_write_called | f_ch_write_closed)) {
			chp->set(netp::E_CHANNEL_HANDLER_INVALID_STATE);
			return;
		}
		m_flag |= f_tls_handler_close_write_called;

		NETP_ASSERT(m_close_write_p == nullptr);
		NETP_ASSERT(ctx == m_ctx);
		if (m_flag & f_tls_ch_writing) {
			NETP_ASSERT(m_tls_outlets_user_data.size());
			m_flag |= f_tls_ch_close_pending;
			m_close_write_p = chp;
			return;
		}

		NETP_ASSERT(m_tls_channel != nullptr);
		NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;
		NETP_ASSERT(m_tls_outlets_user_data.size() == 0);

		if (m_flag & f_tls_channel_close_notified) {
			ctx->close_write(chp);
			return;
		}

		try {
			m_flag |= (f_tls_channel_close_notified |f_ch_close_write_pending);
			m_close_write_p = chp;
			tlsch->close();
			return;
		} catch (std::exception& e) {
			NETP_ERR("[tls]tls record write failed: %s", e.what());
			//tls exception, close under-layer transport anyway
			m_close_write_p = nullptr;
			ctx->close(chp);
			return;
		}
	}

	void tls_handler::_try_tls_user_data_flush() {

		NETP_ASSERT( (m_flag&f_ch_write_closed) == 0);
		NETP_ASSERT(m_tls_outlets_user_data.size());
		NETP_ASSERT( (m_flag&f_tls_ch_writing) == 0 );
		m_flag |= f_tls_ch_writing;

		NETP_ASSERT(m_tls_channel != nullptr);
		NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;
		try {
			//send would result in multi tls_emit_data call
			NETP_ASSERT((m_flag & f_tls_ch_writing_user_data) == 0);
			m_flag |= f_tls_ch_writing_user_data;
			tls_ch_outlet& outlet = m_tls_outlets_user_data.front();
			tlsch->send((uint8_t*)outlet.data->head(), outlet.data->len());
			m_flag &= ~f_tls_ch_writing_user_data;

			_try_tls_record_data_flush();
		} catch (std::exception& e) {
			NETP_ERR("[tls]tls record write failed: %s, close socket channel", e.what());
			m_flag &= ~(f_tls_ch_writing_user_data);
			m_ctx->close();
		}
	}

	void tls_handler::_tls_user_data_flush_done(int code) {
		NETP_ASSERT(m_flag & f_tls_ch_writing);
		NETP_ASSERT(code != netp::E_CHANNEL_WRITE_BLOCK);

		NETP_ASSERT(m_tls_outlets_user_data.size());
		tls_ch_outlet& outlet = m_tls_outlets_user_data.front();
		NETP_ASSERT(outlet.write_p != nullptr);

		if (--(outlet.record_count) > 0) {
			return;
		}

		outlet.write_p->set(code);
		m_tls_outlets_user_data.pop();
		m_flag &= ~(f_tls_ch_writing);
		//NETP_VERBOSE("[tls]write userdata bytes: %d, code: %d", outlet.data->len(), code);

		if (code != netp::OK) {
			//under-layer would trigger close_write automatically
			NETP_WARN("[tls]write userdata failed: %d", code);
			return;
		}

		if (!m_tls_outlets_user_data.empty()) {
			_try_tls_user_data_flush();
			return;
		}

		if (m_flag& f_tls_ch_close_pending) {
			m_flag &= ~f_tls_ch_close_pending;
			m_flag |= (f_tls_channel_close_notified|f_ch_close_pending);

			NETP_ASSERT(m_tls_channel != nullptr);
			NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;
			tlsch->close();
			return;
		}
		else if (m_flag & f_tls_ch_close_write_pending) {
			m_flag &= ~f_tls_ch_close_write_pending;
			m_flag |= (f_tls_channel_close_notified | f_ch_close_write_pending);

			NETP_ASSERT(m_tls_channel != nullptr);
			NNASP<Botan::TLS::Channel> tlsch = m_tls_channel;
			tlsch->close();
			return;
		}
	}

	void tls_handler::tls_verify_cert_chain(
		const std::vector<Botan::X509_Certificate>& cert_chain,
		const std::vector<std::shared_ptr<const Botan::OCSP::Response>>& ocsp,
		const std::vector<Botan::Certificate_Store*>& trusted_roots,
		Botan::Usage_Type usage,
		const std::string& hostname,
		const Botan::TLS::Policy& policy)
	{
		if (!m_tls_ctx->tlsconfig->cert_verify_required) {
			return;
		}

		if (cert_chain.empty())
		{
			throw Botan::Invalid_Argument("Certificate chain was empty");
		}

		Botan::Path_Validation_Restrictions restrictions(
			policy.require_cert_revocation_info(),
			policy.minimum_signature_strength());

		auto ocsp_timeout = std::chrono::milliseconds(1000);

		Botan::Path_Validation_Result result = Botan::x509_path_validate(
			cert_chain,
			restrictions,
			trusted_roots,
			hostname,
			usage,
			std::chrono::system_clock::now(),
			ocsp_timeout,
			ocsp);

		NETP_VERBOSE("[tls_handler]Certificate validation status: %s", result.result_string().c_str());
		if (result.successful_validation())
		{
			auto status = result.all_statuses();
			if (status.size() > 0 && status[0].count(Botan::Certificate_Status_Code::OCSP_RESPONSE_GOOD))
			{
				NETP_VERBOSE("Valid OCSP response for this server");
			}
		}
		else {
			throw Botan::TLS::TLS_Exception(Botan::TLS::Alert::BAD_CERTIFICATE,
				"Certificate validation failure: " + result.result_string());
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
		//NETP_VERBOSE("[tls_handler]tls_session_established, session_id: %s, ticket: %s, ver: %s, using: %s", m_session_id.c_str(), m_session_ticket.c_str(), session.version().to_string().c_str(), session.ciphersuite().to_string().c_str());

		//if (flag_set("print-certs"))
		{
			const std::vector<Botan::X509_Certificate>& certs = session.peer_certs();
			for (size_t i = 0; i != certs.size(); ++i)
			{
				//NETP_VERBOSE("Certificate: %d/%d\n: %s\n%s", i + 1, certs.size(), certs[i].to_string().c_str(), certs[i].PEM_encode().c_str());
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

	void tls_handler::_tls_record_data_flush_done(int code) {
		NETP_ASSERT(m_tls_outlets_records_data.size());
		NETP_ASSERT((m_flag & f_ch_writing));
		NETP_ASSERT(code != netp::E_CHANNEL_WRITE_BLOCK);
		socket_ch_outlet& outlet = m_tls_outlets_records_data.front();
		if (outlet.is_userdata) {
			_tls_user_data_flush_done(code);
		}

		//this line should be after _tls_user_data_flush_done to avoid nested _try_tls_record_data_flush
		m_flag &= ~f_ch_writing;
		m_tls_outlets_records_data.pop();

		if (code != netp::OK) {
			NETP_VERBOSE("[tls]write failed: %d", code);
			//the under-layer would trigger write_closed automatically , just return
			return;
		}

		if (m_flag&f_ch_write_closed) {
			NETP_ASSERT( m_tls_outlets_records_data.empty() );
			return;
		}

		NETP_ASSERT(m_ctx != nullptr);
		if (!m_tls_outlets_records_data.empty()) {
			_try_tls_record_data_flush();
			return;
		}
		
		//f_ch_close_pending
		if ( m_flag&f_ch_close_pending ) {
			NETP_ASSERT( m_flag& f_tls_channel_close_notified);
			m_flag &= ~(f_ch_close_pending|f_ch_close_write_pending);
			NRP<netp::promise<int>> __p = m_close_p; //avoid nest set
			m_close_p = nullptr;
			m_ctx->close(__p);
			return;
		}

		if (m_flag&f_ch_close_write_pending) {
			NETP_ASSERT( m_flag & f_tls_channel_close_notified);
			m_flag &= ~f_ch_close_write_pending;
			NRP<netp::promise<int>> __p = m_close_write_p; //avoid nest set
			m_close_write_p = nullptr;
			m_ctx->close_write(__p);
			return;
		}
	}

	void tls_handler::_try_tls_record_data_flush() {
		NETP_ASSERT( (m_flag&(f_ch_write_closed|f_ch_closed)) ==0 );
		if ( ((m_flag&f_ch_writing) ==0) && (m_tls_outlets_records_data.size())) {
			m_flag |= f_ch_writing;
			socket_ch_outlet& outlet = m_tls_outlets_records_data.front();
			NRP<netp::promise<int>> f = netp::make_ref<netp::promise<int>>();
			f->if_done([TLS_H = NRP<tls_handler>(this), L = m_ctx->L](int const& rt) {
				NETP_ASSERT(L->in_event_loop());
				TLS_H->_tls_record_data_flush_done(rt);
			});
			m_ctx->write(f,outlet.data);
		}
	}

	void tls_handler::tls_emit_data(const uint8_t buf[], size_t length) {
		if (m_flag&(f_ch_write_closed|f_ch_closed)) {
			NETP_WARN("[tls]handler shutdown already, ignore write, nbytes: %u", length);
			return;
		}

		NETP_ASSERT(m_ctx != nullptr);
		if ((m_flag&f_tls_ch_writing_user_data)) {
			NETP_ASSERT( m_tls_outlets_user_data.size() );
			tls_ch_outlet& front_ = m_tls_outlets_user_data.front();
			++front_.record_count;
			m_tls_outlets_records_data.push({ netp::make_ref<netp::packet>(buf, netp::u32_t(length)) , true });
		} else {
			m_tls_outlets_records_data.push({ netp::make_ref<netp::packet>(buf, netp::u32_t(length)) , false });
			_try_tls_record_data_flush();
		}
	}

	void tls_handler::tls_alert(Botan::TLS::Alert alert)
	{
		//@note: default policy respone with no_renegotiation
		//@after having received a
		//no_renegotiation alert that it is not willing to accept), it SHOULD
		//send a fatal alert to terminate the connection

		//NETP_VERBOSE("Alert: %s", alert.type_string().c_str());
		if (m_flag & f_ch_closed) {
			NETP_ASSERT(m_ctx == nullptr);
			return;
		}

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
			NETP_INFO("Alert: %s", alert.type_string().c_str() );

			if (alert.is_fatal()) {
				//https://datatracker.ietf.org/doc/html/rfc5246#section-7.2.2, upon fatal alert, both side close transport immediately
				NETP_WARN("[tls_handler]fatal Alert: %s, do handler::close()", alert.type_string().c_str());
				NETP_ASSERT(m_ctx != nullptr);
				if ((m_flag & (f_tls_ch_activated | f_ch_connected)) == (f_tls_ch_activated | f_ch_connected)) {
					close(netp::make_ref<netp::promise<int>>(), m_ctx);
				} else {
					if (m_ctx) { m_ctx->close(); }
				}
			}
		}
		break;
		}
	}

	void tls_handler::tls_record_received(uint64_t /*seq_no*/, const uint8_t buf[], size_t buf_size)
	{
		if (m_flag & f_ch_read_closed) {
			NETP_WARN("[tls_handler]received bytes after read closed, bytes: %d", buf_size);
			return;
		}
		NETP_ASSERT(m_ctx != nullptr);
		if (netp::u32_t(buf_size) > 0) {
			m_ctx->fire_read(netp::make_ref<netp::packet>(buf, netp::u32_t(buf_size)));
		}
	}

	void tls_handler::tls_log_error(const char* err) {
		NETP_WARN("[tls_handler]session: %s, tls error: %s", m_session_id.c_str(), err );
	}
	void tls_handler::tls_log_debug(const char* what) {
		NETP_VERBOSE("[tls_handler]session: %s, tls debug: %s", m_session_id.c_str(), what);
		(void)what;
	}
}}

#endif