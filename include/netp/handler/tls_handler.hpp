#ifndef _NETP_HANDLER_TLS_HANDLER_HPP
#define _NETP_HANDLER_TLS_HANDLER_HPP

#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel_handler_context.hpp>

#ifdef NETP_WITH_BOTAN
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
		Botan::TLS::Protocol_Version tls_version;

		NSP<Botan::RandomNumberGenerator> rng;
		NSP<Botan::TLS::Session_Manager> session_mgr;
		NSP<Botan::Credentials_Manager> credentials_mgr;
		NSP<Botan::TLS::Policy> policy;

		NSP<Botan::TLS::Server_Information> server_info;
		std::vector<std::string> next_protocols;
	};

	inline static NRP<tls_context> default_tls_context() {
		NRP<tls_context> tlsctx = netp::make_ref<tls_context>();
		tlsctx->tls_version = Botan::TLS::Protocol_Version::TLS_V12;
		tlsctx->rng = netp::make_shared<Botan::AutoSeeded_RNG>();
		tlsctx->session_mgr = netp::make_shared<Botan::TLS::Session_Manager_In_Memory>(*(tlsctx->rng));
		tlsctx->policy = netp::make_shared<Botan::TLS::Default_Policy>();

		tlsctx->credentials_mgr = nullptr;
		tlsctx->server_info = nullptr;
		tlsctx->next_protocols = {};
		return tlsctx;
	}

	inline static NRP<tls_context> default_tls_server_context(std::string const& ca_path, std::string const& privkey ) {
		NRP<tls_context> tlsctx = default_tls_context();
		tlsctx->credentials_mgr = netp::make_shared<netp::handler::Basic_Credentials_Manager>(*(tlsctx->rng), ca_path, privkey );
		return tlsctx;
	}

	inline static NRP<tls_context> default_tls_client_context(std::string const& host, netp::u16_t port) {
		NRP<tls_context> _tlsctx = default_tls_context();
		_tlsctx->credentials_mgr = netp::make_shared<netp::handler::Basic_Credentials_Manager>(true, "");
		_tlsctx->server_info = netp::make_shared<Botan::TLS::Server_Information>(host, port);
		return _tlsctx;
	}


	enum tls_handler_flag {
		f_tls_ch_handshake = 1 << 0,
		f_tls_ch_established = 1 << 1,
		f_tls_ch_activated = 1 << 2,
		f_tls_ch_write_idle = 1 << 3,
		f_tls_ch_writing_user_data = 1<<4,

		f_write_idle = 1<<5,
		f_writing = 1<<6,
		f_connected = 1<<7,
		f_read_closed = 1<<8,
		f_write_closed = 1 << 9,
		f_closed = 1 <<10,
	};

	class tls_handler :
		public channel_handler_abstract,
		public Botan::TLS::Callbacks
	{

	protected:
		struct tls_ch_outlet {
			NRP<netp::packet> data;
			NRP<netp::promise<int>> write_p;
		};

		struct socket_ch_outlet {
			NRP<netp::packet> data;
			bool is_userdata;
		};

		int m_flag;

		NSP<Botan::TLS::Channel> m_tls_channel;

		typedef std::queue<tls_ch_outlet, std::deque<tls_ch_outlet, netp::allocator<tls_ch_outlet>>> tls_ch_outlet_queue_t;
		typedef std::queue<socket_ch_outlet, std::deque<socket_ch_outlet, netp::allocator<socket_ch_outlet>>> socket_ch_outlets_queue_t;

		tls_ch_outlet_queue_t m_outlets_to_tls_ch;
		socket_ch_outlets_queue_t m_outlets_to_socket_ch;

		NRP<netp::channel_handler_context> m_ctx;

		NRP<tls_context> m_tls_ctx;
	public:
		tls_handler( NRP<tls_context> const& tlsctx ) :
			channel_handler_abstract(CH_ACTIVITY_CONNECTED | CH_ACTIVITY_CLOSED | CH_ACTIVITY_READ_CLOSED|CH_ACTIVITY_WRITE_CLOSED | CH_INBOUND_READ | CH_OUTBOUND_WRITE|CH_OUTBOUND_CLOSE),
			m_flag(f_tls_ch_write_idle|f_closed| f_write_closed|f_read_closed),
			m_ctx(nullptr),
			m_tls_channel(nullptr),
			m_tls_ctx(tlsctx)
		{}

		void _do_clean();

		void _tls_ch_flush_done(int code);
		void _try_tls_ch_flush();

		void _socket_ch_flush_done(int code);
		void _try_socket_ch_flush();

		bool tls_session_established(const Botan::TLS::Session& session) override;
		void tls_session_activated() override;
		void tls_emit_data(const uint8_t buf[], size_t length) override;
		void tls_alert(Botan::TLS::Alert alert) override;
		void tls_record_received(uint64_t /*seq_no*/, const uint8_t buf[], size_t buf_size) override;

		void tls_verify_cert_chain(
			const std::vector<Botan::X509_Certificate>& cert_chain,
			const std::vector<std::shared_ptr<const Botan::OCSP::Response>>& ocsp,
			const std::vector<Botan::Certificate_Store*>& trusted_roots,
			Botan::Usage_Type usage,
			const std::string& hostname,
			const Botan::TLS::Policy& policy) override
		{
		}

		//void connected(NRP<channel_handler_context> const& ctx) override;
		void closed(NRP<channel_handler_context> const& ctx) override;

		void write_closed(NRP<channel_handler_context> const& ctx)override;
		void read_closed(NRP<channel_handler_context> const& ctx)override;

		void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override;
		void write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;
		void close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) override;
	};
}}

#endif //endof NETP_WITH_BOTAN
#endif