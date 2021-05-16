
#include <netp/handler/http.hpp>
#include <netp/logger_broker.hpp>
#include <netp/channel_handler_context.hpp>

namespace netp { namespace handler {

	void http::__setup_parser(bool is_httpu ) {
		NETP_ASSERT(m_http_parser == nullptr);
		m_http_parser = netp::make_ref<netp::http::parser>();
		m_http_parser->init(netp::http::HPT_BOTH);

		if (NETP_UNLIKELY(is_httpu)) {
			m_http_parser->on_headers_complete = std::bind(&http::http_on_headers_complete_from, NRP<http>(this), std::placeholders::_1, std::placeholders::_2);
			m_http_parser->on_body = std::bind(&http::http_on_body_from, NRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
			m_http_parser->on_message_complete = std::bind(&http::http_on_message_from_complete, NRP<http>(this), std::placeholders::_1);
		} else {
			m_http_parser->on_headers_complete = std::bind(&http::http_on_headers_complete, NRP<http>(this), std::placeholders::_1, std::placeholders::_2);
			m_http_parser->on_body = std::bind(&http::http_on_body, NRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
			m_http_parser->on_message_complete = std::bind(&http::http_on_message_complete, NRP<http>(this), std::placeholders::_1);
		}

		m_http_parser->on_chunk_header = std::bind(&http::http_on_chunk_header, NRP<http>(this), std::placeholders::_1);
		m_http_parser->on_chunk_complete = std::bind(&http::http_on_chunk_complete, NRP<http>(this), std::placeholders::_1);
	}

	void http::__finish_parser() {
		if (m_http_parser != nullptr) {
			m_http_parser->finish();
		}
	}

	void http::__unsetup_parser() {
		if (m_http_parser != nullptr ) {
			m_http_parser->finish();
			m_http_parser->cb_reset();
		}
	}

	void http::__http_parse(NRP<netp::channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT(m_http_parser != nullptr);
		NETP_ASSERT(income != nullptr);
		int http_parse_ec = HPE_OK;
		while (income->len() && http_parse_ec == netp::OK && m_ctx != nullptr /*http parser would be reset during parse phase by http evt listener*/) {
			http_parse_ec = m_http_parser->parse((char*)income->head(), income->len() );
			if (NETP_LIKELY(http_parse_ec == HPE_OK)) {
				income->skip( income->len() );
			} else {
				income->skip( m_http_parser->calc_parsed_bytes((char*)income->head()) );
				if (NETP_HTTP_IS_PARSE_ERROR(http_parse_ec)) {
					ctx->close();
					break;
				} else if (http_parse_ec == HPE_PAUSED_UPGRADE) {
					NETP_ASSERT( income->len() == 0 );
				}
			}
		}
	}

	void http::connected(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_ctx == nullptr);
		m_ctx = ctx;
		__setup_parser();
		event_broker_any::invoke<fn_http_activity_t>(E_CONNECTED, ctx);
	}

	void http::closed(NRP<netp::channel_handler_context> const& ctx) {
		m_ctx = nullptr;
		__unsetup_parser();
		event_broker_any::invoke<fn_http_activity_t>(E_CLOSED, ctx);
	}

	void http::error(NRP<netp::channel_handler_context> const& ctx, int err) {
		event_broker_any::invoke<fn_http_error_t>(E_ERROR, ctx, err);
	}

	void http::read_closed(NRP<channel_handler_context> const& ctx) {
		__finish_parser();
		event_broker_any::invoke<fn_http_activity_t>(E_READ_CLOSED, ctx);
	}
	void http::write_closed(NRP<channel_handler_context> const& ctx) {
		event_broker_any::invoke<fn_http_activity_t>(E_WRITE_CLOSED, ctx);
	}

	void http::read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		__http_parse(ctx, income);
	}
	
	void http::readfrom(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income, NRP<address> const& from) {
		//NETP_INFO("[http][recvfrom]from: %s: httpu: \n%s", from.to_string().c_str(), std::string((char*)income->head(), income->len()).c_str() );
		/*
		char test_header[] = 
			"HTTP/1.1 200 OK\r\n"
			"CACHE-CONTROL: max - age = 120\r\n"
			"ST: upnp:rootdevice\r\n"
			"USN: uuid:3ddcd1d3-2380-45f5-b069-04d9f595ff80::upnp:rootdevice\r\n"
			"EXT:\r\n"
			"SERVER: AsusWRT / 3.0.0.4 UPnP / 1.1 MiniUPnPd / 1.9\r\n"
			"LOCATION: http ://192.168.50.1:52442/rootDesc.xml\r\n"
			"OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns = 01\r\n"
			"01-NLS: 1\r\n"
			"BOOTID.UPNP.ORG: 1\r\n"
			"CONFIGID.UPNP.ORG: 1337\r\n\r\n";

		income->reset();
		income->write((netp::byte_t*)test_header, netp::strlen(test_header) );
		*/

		m_from_tmp = from->clone();
		m_ctx = ctx;
		__setup_parser(true); 
		__http_parse(ctx, income);
		m_ctx = nullptr;
		__unsetup_parser();
	}

	int http::http_on_headers_complete(NRP<netp::http::parser> const& p, NRP<netp::http::message> const& message)
	{
		(void)p;
		event_broker_any::invoke<fn_http_message_header_t>(E_MESSAGE_HEADER, m_ctx, message);
		return netp::OK;
	}

	int http::http_on_body(NRP<netp::http::parser> const& p, const char* data, netp::u32_t len) {
		event_broker_any::invoke<fn_http_message_body_t>(E_MESSAGE_BODY, m_ctx, data,len);
		(void)p;
		return netp::OK;
	}

	int http::http_on_message_complete(NRP<netp::http::parser> const& p) {
		event_broker_any::invoke<fn_http_message_end_t>(E_MESSAGE_END, m_ctx);
		(void)p;
		return netp::OK;
	}

	int http::http_on_headers_complete_from(NRP<netp::http::parser> const& p, NRP<netp::http::message> const& message)
	{
		(void)p;
		m_message_tmp = message;
		event_broker_any::invoke<fn_http_message_header_from_t>(E_MESSAGE_HEADER, m_ctx, message, m_from_tmp );
		return netp::OK;
	}

	int http::http_on_body_from(NRP<netp::http::parser> const& p, const char* data, netp::u32_t len) {
		event_broker_any::invoke<fn_http_message_body_from_t>(E_MESSAGE_BODY, m_ctx, data, len, m_from_tmp );
		(void)p;
		return netp::OK;
	}

	int http::http_on_message_from_complete(NRP<netp::http::parser> const& p) {
		//NETP_INFO("[http][recvfrom]invoke E_MESSAGE_FROM: \n%s", m_from_tmp.to_string().c_str() );

		event_broker_any::invoke<fn_http_message_end_from_t>(E_MESSAGE_END_FROM, m_ctx, m_from_tmp );
		(void)p;
		m_message_tmp = nullptr;
		return netp::OK;
	}

	int http::http_on_chunk_header(NRP<netp::http::parser> const& p ) {
		//NETP_ASSERT(!"TODO");
		(void)p;
		return netp::OK;
	}

	int http::http_on_chunk_complete(NRP<netp::http::parser> const& p) {
		//NETP_ASSERT(!"TODO");
		(void)p;
		return netp::OK;
	}
}}