#ifndef _NETP_HANDLER_TLS_HPP
#define _NETP_HANDLER_TLS_HPP

#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel_handler_context.hpp>
#include <netp/handler/tls_credentials.hpp>

#ifdef NETP_WITH_BOTAN

#include <botan/tls_alert.h>
#include <botan/tls_callbacks.h>
#include <botan/tls_policy.h>
#include <botan/tls_client.h>

namespace netp { namespace handler {

	class Policy final :
		public Botan::TLS::Policy 
	{
		Botan::TLS::Protocol_Version m_version;
		public:
			Policy(Botan::TLS::Protocol_Version const& ver):
			m_version(ver)
			{}

			std::vector<std::string> allowed_ciphers() const override {
				if (m_version.supports_aead_modes() == false) {
					return { "AES-256","AES-128" };
				}
				return Botan::TLS::Policy::allowed_ciphers();
			}
			bool allow_tls10() const override { return m_version == Botan::TLS::Protocol_Version::TLS_V10; }
			bool allow_tls11() const override { return m_version == Botan::TLS::Protocol_Version::TLS_V11; }
			bool allow_tls12() const override { return m_version == Botan::TLS::Protocol_Version::TLS_V12; }
	};

	struct tls_context final :
		public ref_base
	{
		NSP<Botan::Credentials_Manager> credentials_mgr;
		NSP<Botan::RandomNumberGenerator> rng;

		NSP<Botan::TLS::Session_Manager> session_mgr;
		NSP<Botan::TLS::Policy>policy;
		NSP<Botan::TLS::Server_Information> server_info;
		Botan::TLS::Protocol_Version tls_version;
		std::vector<std::string> next_protocols;	
	};

	struct tls_outlet {
		NRP<netp::packet> outlet;
		NRP<netp::promise<int>> write_p;
	};

	class tls final :
		public channel_handler_abstract,
		public Botan::TLS::Callbacks
	{
		enum class tls_state {
			S_CLOSED,
			S_HANDSHAKE,
			S_TRANSFER,
		};

		enum class tls_write_state {
			S_WRITE_IDLE,
			S_APPDATE_WRITE_PREPARE,
			S_APPDATE_WRITING,
			S_INTERLEAVE_WRITING,
			S_WRITE_SHUTDOWN
		};

		tls_state m_state;
		tls_write_state m_write_state;

		std::queue<tls_outlet> m_outlets;
		std::queue<NRP<netp::packet>> m_interleave_outlets;

		NRP<netp::channel_handler_context> m_ctx;
		NSP<Botan::TLS::Client> m_tls_client;
		NRP<tls_context> m_tls_context;

		public:
			tls(NRP<tls_context> const& tls_ctx):
				channel_handler_abstract(CH_ACTIVITY_CONNECTED|CH_ACTIVITY_CLOSED|CH_ACTIVITY_WRITE_CLOSED|CH_INBOUND_READ|CH_OUTBOUND_WRITE),
				m_state(tls_state::S_CLOSED),
				m_write_state(tls_write_state::S_WRITE_IDLE),
				m_ctx(nullptr),
				m_tls_client(nullptr),
				m_tls_context(tls_ctx)
			{}

			void __tls_do_clean();
			void __tls_do_write_appdata_done(int code);
			void __tls_do_write_interleave_done(int code);

			void __tls_try_appdata_flush();
			void __tls_try_interleave_flush();

			bool tls_session_established(const Botan::TLS::Session& session) override;
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

			void connected(NRP<channel_handler_context> const& ctx) override;
			void closed(NRP<channel_handler_context> const& ctx) override;
			
			void write_closed(NRP<channel_handler_context> const& ctx)override;

			void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override;
			void write(NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet, NRP<promise<int>> const& chp) override;
	};
}}

#endif

#endif