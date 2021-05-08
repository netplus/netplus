#ifndef _NETP_HTTP_PARSER_HPP
#define _NETP_HTTP_PARSER_HPP

#include <string>
#include <functional>

#define NETP_USE_LLHTTP

#ifdef NETP_USE_LLHTTP
	#include "./../../../3rd/llhttp/llhttp.h"
#else
	#include "./../../../3rd/http_parser/http_parser.h"
#endif

#include <netp/core.hpp>
#include <netp/http/message.hpp>

namespace netp { namespace http {

	enum http_parse_type {
		HPT_REQ = HTTP_REQUEST,
		HPT_RESP = HTTP_RESPONSE,
		HPT_BOTH = HTTP_BOTH
	};

	struct parser;
	typedef std::function<int(NRP<parser> const&, const char* data, netp::u32_t len)> parser_cb_data;
	typedef std::function<int(NRP<parser> const&)> parser_cb;
	typedef std::function<int(NRP<parser> const&, NRP<netp::http::message> const&)> parser_cb_header;

	int parse_url(const char* url, size_t len, url_fields& urlfields, bool is_connect = false );

	enum class last_header_element {
		NONE = 0,
		FIELD,
		VALUE
	};

#ifdef NETP_USE_LLHTTP
	#define netp_http_parser_t llhttp_t
	#define netp_http_parser_settings llhttp_settings_t
	#define netp_http_parser_init llhttp_init
	#define netp_http_parser_settings_init llhttp_settings_init
	#define netp_http_parser_execute llhttp_execute
#else
	typedef http_parser netp_http_parser_t;
	#define netp_http_parser_init http_parser_init
	#define netp_http_parser_execute http_parser_execute
#endif

	struct parser final :
		public netp::ref_base
	{
		netp_http_parser_t* _p;
		int _http_errno;
		NRP<netp::ref_base> ctx;

		parser_cb_header on_headers_complete;
		parser_cb_data on_body;
		parser_cb on_message_complete; //notify

		parser_cb on_chunk_header;
		parser_cb on_chunk_complete;

		NRP<netp::http::message> message_tmp;

		last_header_element last_h;
		string_t field_tmp;//for header field
		string_t field_value_tmp;

		parser();
		~parser();

		void init(http_parse_type type);
		void deinit();
		void reset();

		netp::u32_t parse(char const* const data, netp::u32_t len, int& ec);
	};

}}
#endif