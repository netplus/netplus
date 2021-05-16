#ifndef _NETP_HANDLER_HTTP_HPP
#define _NETP_HANDLER_HTTP_HPP

#include <functional>

#include <netp/event_broker.hpp>
#include <netp/channel_handler.hpp>

#include <netp/http/message.hpp>
#include <netp/http/parser.hpp>

#include <netp/address.hpp>

namespace netp { namespace handler {

	class http final:
		public netp::channel_handler_abstract,
		public netp::event_broker_any
	{

	public:
		enum http_event {
			E_CONNECTED,
			E_READ_CLOSED,
			E_WRITE_CLOSED,
			E_CLOSED,
			E_ERROR,
				
			//we do not need this notify any more..
			E_MESSAGE_HEADER,
			E_MESSAGE_BODY,
			E_MESSAGE_END,

			//NOTE: 
			//some message only has header ,,
			//if it don't has a content-length field,,,we may never get a message_complete notify
			E_MESSAGE_HEADER_FROM,
			E_MESSAGE_BODY_FROM,
			E_MESSAGE_END_FROM
		};

		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx)> fn_http_activity_t;
		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx, int err)> fn_http_error_t;

		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx, NRP<netp::http::message> const& m)> fn_http_message_header_t;
		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx, const char* data, u32_t len )> fn_http_message_body_t;
		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx)> fn_http_message_end_t;

		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx, NRP<netp::http::message> const& m, NRP<address> const& from)> fn_http_message_header_from_t;
		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx, const char* data, u32_t len, NRP<address> const& from)> fn_http_message_body_from_t;
		typedef std::function<void(NRP<netp::channel_handler_context> const& ctx, NRP<address> const& from)> fn_http_message_end_from_t;

	private:

		NRP<netp::http::parser> m_http_parser;
		NRP<netp::channel_handler_context> m_ctx;

		//for httpu
		NRP<netp::http::message> m_message_tmp;
		NRP<address> m_from_tmp;

		void __setup_parser( bool is_httpu = false );
		void __unsetup_parser();
		void __http_parse(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income);
	public:
		http() :
			channel_handler_abstract(CH_ACTIVITY_CONNECTED|CH_ACTIVITY_CLOSED|CH_ACTIVITY_ERROR|CH_ACTIVITY_READ_CLOSED|CH_ACTIVITY_WRITE_CLOSED|CH_INBOUND_READ|CH_INBOUND_READ_FROM)
		{}
		~http() {}
		void connected(NRP<netp::channel_handler_context> const& ctx) override ;
		void closed(NRP<netp::channel_handler_context> const& ctx) override;
		void error(NRP<channel_handler_context> const& ctx, int err) override ;
		void read_closed(NRP<channel_handler_context> const& ctx) override;
		void write_closed(NRP<channel_handler_context> const& ctx) override;

		void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) override;
		
		//for httpu
		void readfrom(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income, NRP<address> const& from) override;

		int http_on_headers_complete(NRP<netp::http::parser> const& p, NRP<netp::http::message> const& message);
		int http_on_body(NRP<netp::http::parser> const& p, const char* data, netp::u32_t len);
		int http_on_message_complete(NRP<netp::http::parser> const& p);

		int http_on_headers_complete_from(NRP<netp::http::parser> const& p, NRP<netp::http::message> const& message);
		int http_on_body_from(NRP<netp::http::parser> const& p, const char* data, netp::u32_t len);
		int http_on_message_from_complete(NRP<netp::http::parser> const& p);

		int http_on_chunk_header(NRP<netp::http::parser> const& p);
		int http_on_chunk_complete(NRP<netp::http::parser> const& p);
	};
}}
#endif