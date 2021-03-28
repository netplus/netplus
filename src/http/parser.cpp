#include <netp/http/parser.hpp>


namespace netp { namespace http {

	//info->location = "http://192.168.50.1/rootDesc.xml";
	//result in : schema: http, host: 192.168.50.1, port: 80, path: /rootDesc.xml

	int parse_url(const char* url, size_t len, url_fields& urlfields, bool is_connect) {

		http_parser_url u;
		int rt = http_parser_parse_url(url, len, is_connect, &u);
		NETP_RETURN_V_IF_NOT_MATCH(NETP_NEGATIVE(rt), rt == 0);

		if (u.field_set & (1 << UF_SCHEMA)) {
			urlfields.schema = string_t(url + u.field_data[UF_SCHEMA].off, u.field_data[UF_SCHEMA].len);
		}

		if (u.field_set & (1 << UF_HOST)) {
			urlfields.host = string_t(url + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);
		}

		if (u.field_set & (1 << UF_PATH)) {
			urlfields.path = string_t(url + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len);
		}

		if (u.field_set & (1 << UF_QUERY)) {
			urlfields.query = string_t(url + u.field_data[UF_QUERY].off, u.field_data[UF_QUERY].len);
		}
		if (u.field_set & (1 << UF_USERINFO)) {
			urlfields.userinfo = string_t(url + u.field_data[UF_USERINFO].off, u.field_data[UF_USERINFO].len);
		}
		if (u.field_set & (1 << UF_FRAGMENT)) {
			urlfields.fragment = string_t(url + u.field_data[UF_FRAGMENT].off, u.field_data[UF_FRAGMENT].len);
		}

		if (u.field_set & (1 << UF_PORT)) {
			urlfields.port = u.port;
		}
		else {
			urlfields.port = netp::iequals(urlfields.schema, string_t("https")) ? 443 : 80;
		}

		return netp::OK;
	}
	
	inline static int _on_message_begin(http_parser* p_) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		p->message_tmp = netp::make_ref<netp::http::message>();
		p->message_tmp->H = netp::make_ref<netp::http::header>();
		return 0;
	}

	inline static int _on_url(http_parser* p_, const char* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);

		//for HEAD update
		//p->type = (u8_t)p_->type;
		NETP_ASSERT(p_->type == HPT_REQ);
		switch (p_->method) {
		case HTTP_DELETE:
		{
			p->message_tmp->opt = http::option::O_DELETE;
		}
		break;
		case HTTP_GET:
		{
			p->message_tmp->opt = http::option::O_GET;
		}
		break;

		case HTTP_HEAD:
		{
			p->message_tmp->opt = http::option::O_HEAD;
		}
		break;
		case HTTP_POST:
		{
			p->message_tmp->opt = http::option::O_POST;
		}
		break;
		case HTTP_PUT:
		{
			p->message_tmp->opt = http::option::O_PUT;
		}
		break;
		case HTTP_CONNECT:
		{
			p->message_tmp->opt = http::option::O_CONNECT;
		}
		break;
		case HTTP_OPTIONS:
		{
			p->message_tmp->opt = http::option::O_OPTIONS;
		}
		break;
		case HTTP_TRACE:
		{
			p->message_tmp->opt = http::option::O_TRACE;
		}
		break;
		case HTTP_MSEARCH:
		{
			p->message_tmp->opt = http::option::O_M_SEARCH;
		}
		break;
		case HTTP_NOTIFY:
		{
			p->message_tmp->opt = http::option::O_NOTIFY;
		}
		break;
		case HTTP_SUBSCRIBE:
		{
			p->message_tmp->opt = http::option::O_SUBSCRIBE;
		}
		break;
		case HTTP_UNSUBSCRIBE:
		{
			p->message_tmp->opt = http::option::O_UNSUBSCRIBE;
		}
		break;
		default:
		{
			NETP_ASSERT(!"UNKNOWN HTTP METHOD");
		}
		break;
		}

		p->message_tmp->url = string_t(data, len);

		return 0;
	}

	inline static int _on_status(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);

		NETP_ASSERT(p_->status_code != 0);
		p->message_tmp->code = p_->status_code;
		p->message_tmp->status = string_t(data, len);
		return 0;
	}

	inline static int _on_header_field(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		if (len == 0) {
			return netp::E_HTTP_EMPTY_FILED_NAME;
		}

		if (p->last_h == last_header_element::VALUE) {
			NETP_ASSERT(p->field_tmp.length());
			p->message_tmp->H->set(p->field_tmp, p->field_value_tmp);
			p->field_tmp.clear();
			p->field_value_tmp.clear();
		}

		p->field_tmp += string_t(data, len);
		p->last_h = last_header_element::FIELD;
		return 0;
	}

	inline static int _on_header_value(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		NETP_ASSERT(p->field_tmp.length());
		p->field_value_tmp += string_t(data,len);
		p->last_h = last_header_element::VALUE;
		return 0;
	}

	inline static int _on_headers_complete(http_parser* p_) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);

		if (p->last_h == last_header_element::VALUE) {
			NETP_ASSERT(p->field_tmp.length());
			p->message_tmp->H->set(p->field_tmp, p->field_value_tmp);
			p->field_tmp.clear();
			p->field_value_tmp.clear();
			p->last_h = last_header_element::NONE;
		}

		p->message_tmp->type = (p_->type == HTTP_REQUEST) ? T_REQ : (p_->type == HTTP_RESPONSE) ? T_RESP: NETP_THROW("invalid message type") ;
		p->message_tmp->ver.major = p_->http_major;
		p->message_tmp->ver.minor = p_->http_minor;
		NETP_ASSERT(p->on_headers_complete != nullptr);
		return p->on_headers_complete(NRP<parser>(p), p->message_tmp);
	}

	inline static int _on_body(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		return p->on_body == nullptr?-1: p->on_body(NRP<parser>(p), data, (u32_t)len);
	}

	inline static int _on_message_complete(http_parser* p_) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		return p->on_message_complete ==nullptr ? -1: p->on_message_complete(NRP<parser>(p));
	}

	inline static int _on_chunk_header(http_parser* p_) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		return p->on_chunk_header == nullptr ? 0: 
			p->on_chunk_header(NRP<parser>(p));
	}

	inline static int _on_chunk_complete(http_parser* p_) {
		parser* p = (parser*)p_->data;
		NETP_ASSERT(p != nullptr);
		return p->on_chunk_complete == nullptr ? 0: 
			p->on_chunk_complete(NRP<parser>(p));
	}

	parser::parser() :
		_p(nullptr),
		ctx(nullptr),
		on_headers_complete(nullptr),
		on_body(nullptr),
		on_message_complete(nullptr),
		on_chunk_header(nullptr),
		on_chunk_complete(nullptr),
		last_h(last_header_element::NONE)
	{
	}

	parser::~parser() {
		deinit();
	}

	void parser::init(http_parse_type type_ = HPT_BOTH) {
		NETP_ASSERT(_p == nullptr);
		_p = (http_parser*) netp::allocator<http_parser>::malloc(1);
		NETP_ALLOC_CHECK(_p, sizeof(http_parser));

		if (type_ == HPT_REQ) {
			http_parser_init(_p, HTTP_REQUEST);
		} else if (type_ == HPT_RESP) {
			http_parser_init(_p, HTTP_RESPONSE);
		} else {
			http_parser_init(_p, HTTP_BOTH);
		}
		_p->data = this;
	}

	void parser::deinit() {
		if (_p != nullptr) {
			//reserve ec first
			_http_errno = _p->http_errno;
			netp::allocator<http_parser>::free(_p);
			_p = nullptr;
		}

		//ctx = nullptr;
		reset();
	}

	void parser::reset() {
		on_headers_complete = nullptr;
		on_body = nullptr;
		on_message_complete = nullptr;

		on_chunk_header = nullptr;
		on_chunk_complete = nullptr;
	}

	//return number of parsed bytes
	netp::u32_t parser::parse(char const* const data, netp::u32_t len, int& ec) {
		NETP_ASSERT(_p != nullptr);

		static http_parser_settings _settings;
		_settings.on_message_begin = _on_message_begin;
		_settings.on_url = _on_url;
		_settings.on_status = _on_status;
		_settings.on_header_field = _on_header_field;
		_settings.on_header_value = _on_header_value;
		_settings.on_headers_complete = _on_headers_complete;
		_settings.on_body = _on_body;
		_settings.on_message_complete = _on_message_complete;

		_settings.on_chunk_header = _on_chunk_header;
		_settings.on_chunk_complete = _on_chunk_complete;

		::size_t nparsed = http_parser_execute(_p, &_settings, data, len);

		if (NETP_LIKELY(_p != 0)) { _http_errno = _p->http_errno; };
		ec = _http_errno;
		return u32_t(nparsed);
	}

}}