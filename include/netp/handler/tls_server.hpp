#ifndef _NETP_HANDLER_TLS_SERVER_HPP
#define _NETP_HANDLER_TLS_SERVER_HPP

#include <netp/channel_handler.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/tls_server.h>

namespace netp { namespace handler {

	class tls_server :
		public channel_handler_abstract,
		public Botan::TLS::Callbacks
	{

	public:
		tls_server()
			:
			channel_handler_abstract(CH_ACTIVITY_CONNECTED | CH_ACTIVITY_CLOSED | CH_ACTIVITY_WRITE_CLOSED | CH_INBOUND_READ | CH_OUTBOUND_WRITE)
		{}

		void tls_emit_data(const uint8_t data[], size_t size) override;
		void tls_record_received(uint64_t seq_no, const uint8_t data[], size_t size) override;
		void tls_alert(Botan::TLS::Alert alert) override;
		void tls_session_activated() override;
		bool tls_session_established(const Botan::TLS::Session& session) override;


		void connected(NRP<channel_handler_context> const& ctx) override;
		void closed(NRP<channel_handler_context> const& ctx) override;

		void write_closed(NRP<channel_handler_context> const& ctx)override;

		void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override;
		void write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;

	};
}}

#endif

#endif