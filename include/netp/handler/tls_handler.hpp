#ifndef _NETP_HANDLER_TLS_HANDLER_HPP
#define _NETP_HANDLER_TLS_HANDLER_HPP

#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel_handler_context.hpp>

#ifdef NETP_WITH_BOTAN
#include <netp/handler/tls_config.hpp>
#include <netp/handler/tls_policy.hpp>

#include <botan/x509path.h>
#include <botan/ocsp.h>
#include <botan/tls_alert.h>
#include <botan/tls_callbacks.h>
#include <botan/tls_policy.h>
#include <botan/tls_client.h>
#include <botan/auto_rng.h>

#include <netp/handler/tls_credentials.hpp>

namespace netp { namespace handler {

	struct tls_context final :
		public ref_base
	{
		NRP<netp::handler::tls_config> tlsconfig;
		Botan::TLS::Protocol_Version tls_version;

		NSP<Botan::RandomNumberGenerator> rng;
		NSP<Botan::TLS::Session_Manager> session_mgr;
		NSP<Botan::Credentials_Manager> credentials_mgr;
		NSP<netp::handler::tls_policy> policy;

		NSP<Botan::TLS::Server_Information> server_info;//requried for verify, but we've to hack it for server verify cuz there is no 
		std::vector<std::string> next_protocols;
	};

	inline static NRP<tls_context> tls_context_with_tlsconfig( NRP<tls_config> const& tlsconfig ) {
		NRP<tls_context> _tlsctx = netp::make_ref<tls_context>();
		_tlsctx->tlsconfig = tlsconfig;

		_tlsctx->tls_version = Botan::TLS::Protocol_Version::TLS_V12;

		_tlsctx->rng = netp::make_shared<Botan::AutoSeeded_RNG>();
		_tlsctx->session_mgr = netp::make_shared<Botan::TLS::Session_Manager_In_Memory>(*(_tlsctx->rng));
		_tlsctx->policy = netp::make_shared<netp::handler::tls_policy>(_tlsctx->tlsconfig);

		_tlsctx->credentials_mgr = netp::make_shared<netp::handler::Basic_Credentials_Manager>(tlsconfig->use_system_store, tlsconfig->ca_path, *(_tlsctx->rng), tlsconfig->cert,tlsconfig->cert_privkey);
		_tlsctx->server_info = netp::make_shared<Botan::TLS::Server_Information>(tlsconfig->host, tlsconfig->port);
		_tlsctx->next_protocols = {};
		return _tlsctx;
	}

	enum tls_handler_flag {
		f_tls_is_client = 1 << 0,
		f_tls_ch_handshake = 1 << 1,
		f_tls_ch_established = 1 << 2,
		f_tls_ch_activated = 1 << 3,
		f_tls_ch_writing = 1 << 4,//
		f_tls_ch_writing_user_data = 1<<5,//barrier for tls_emit_data

		f_tls_handler_close_called = 1<<6,
		f_tls_handler_close_write_called = 1<<7,
		f_tls_channel_close_notified = 1 << 8, //for botan::tls_channel
		f_tls_ch_close_pending = 1<<9,
		f_tls_ch_close_write_pending = 1 << 10,

		f_tls_client_hello_sent =1<<11,
		f_tls_client_hello_received = 1<<12,
		f_tls_server_hello_sent =1<<13,
		f_tls_server_hello_received = 1<<14,

		f_ch_writing = 1<<15,
		f_ch_connected = 1<<16,
		f_ch_read_closed = 1<<17,
		f_ch_write_closed = 1 << 18,
		f_ch_closed = 1 <<19,

		f_ch_close_pending = 1<<22,
		f_ch_close_write_pending =1<<23
	};

	class tls_handler :
		public channel_handler_abstract,
		public Botan::TLS::Callbacks
	{

	protected:
		struct tls_ch_outlet {
			NRP<netp::packet> data;
			NRP<netp::promise<int>> write_p;
			int record_count;
		};

		struct socket_ch_outlet {
			NRP<netp::packet> data;
			bool is_userdata;
		};

		int m_flag;

		NNASP<Botan::TLS::Channel> m_tls_channel;

		typedef std::queue<tls_ch_outlet, std::deque<tls_ch_outlet, netp::allocator<tls_ch_outlet>>> tls_user_data_queue_t;
		typedef std::queue<socket_ch_outlet, std::deque<socket_ch_outlet, netp::allocator<socket_ch_outlet>>> tls_record_data_queue_t;

		tls_user_data_queue_t m_tls_outlets_user_data;
		tls_record_data_queue_t m_tls_outlets_records_data;

		NRP<netp::channel_handler_context> m_ctx;

		NRP<tls_context> m_tls_ctx;
		std::string m_session_id;
		std::string m_session_ticket;

		NRP<promise<int>> m_close_p;
		NRP<promise<int>> m_close_write_p;

	public:
		tls_handler(NRP<tls_context> const& tlsctx);
		virtual ~tls_handler();

		void connected(NRP<channel_handler_context> const& ctx) override { (void)ctx; };
		void closed(NRP<channel_handler_context> const& ctx) override;

		void write_closed(NRP<channel_handler_context> const& ctx)override;
		void read_closed(NRP<channel_handler_context> const& ctx)override;

		void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override;
		void write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;
		void close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) override;

		void close_write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx) override;

		void tls_verify_cert_chain(
			const std::vector<Botan::X509_Certificate>& cert_chain,
			const std::vector<std::shared_ptr<const Botan::OCSP::Response>>& ocsp,
			const std::vector<Botan::Certificate_Store*>& trusted_roots,
			Botan::Usage_Type usage,
			const std::string& hostname,
			const Botan::TLS::Policy& policy) override;

		void _tls_user_data_flush_done(int code);
		void _try_tls_user_data_flush();

		void _tls_record_data_flush_done(int code);
		void _try_tls_record_data_flush();

		void tls_inspect_handshake_msg(const Botan::TLS::Handshake_Message& message) override;
		bool tls_session_established(const Botan::TLS::Session& session) override;
		void tls_session_activated() override;
		void tls_emit_data(const uint8_t buf[], size_t length) override;
		void tls_alert(Botan::TLS::Alert alert) override;
		void tls_record_received(uint64_t /*seq_no*/, const uint8_t buf[], size_t buf_size) override;

		void tls_log_error(const char* err) override;
		void tls_log_debug(const char* what) override;
	};
}}

#endif //endof NETP_WITH_BOTAN
#endif