#ifndef _NETP_HTTP_PARSER_HPP
#define _NETP_HTTP_PARSER_HPP

#include <string>
#include <functional>

#include "./../../../3rd/llhttp/llhttp.h"

#include <netp/core.hpp>
#include <netp/http/chunk_encoder.hpp>
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

#define NETP_HTTP_IS_PARSE_ERROR( ec ) ( !(ec == HPE_OK || ec == HPE_PAUSED_UPGRADE || ec == HPE_PAUSED) )

	struct parser final :
		public netp::non_atomic_ref_base
	{
		llhttp_t* _llp;
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

		void cb_reset();

		int parse(char const* const data, netp::u32_t len);
		int finish();

		int should_keep_alive();
		//int pause();

		//it makes no sense to have this function here
		// (1) resume do not have a return value to tell the resume result
		// (2) we could use error_pos to resume llhttp_execute call to make progress anyway, in this case we have to make a new http parser

		void reset();
		void resume();
		void resume_after_upgrade();

		int get_errno();
		const char* get_error_pos();
		u32_t calc_parsed_bytes(const char* begin);
	};

}}
#endif