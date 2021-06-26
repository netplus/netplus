#ifndef _NETP_HTTP_CLIENT_HPP_
#define _NETP_HTTP_CLIENT_HPP_

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/mutex.hpp>
#include <netp/promise.hpp>

#include <netp/address.hpp>
#include <netp/socket.hpp>

#include <netp/channel_handler.hpp>
#include <netp/channel_handler_context.hpp>
#include <netp/handler/tls_client.hpp>
#include <netp/handler/http.hpp>

#include <netp/http/message.hpp>
#include <netp/http/parser.hpp>


#define DEFAULT_HTTP_REQUEST_TIMEOUT (60)

#ifndef NETP_WITH_BOTAN
namespace netp { namespace handler {
	class tls_context : public netp::ref_base {};
}}

#endif

namespace netp { namespace http {

	//@todo, implement a http write stream feature
	
	//when message end, notify a nullptr data with len of -1
	typedef std::function<void(char* data, u32_t len)> fn_notify_body_t;
	typedef std::function<void(NRP<message> const& m)> fn_notify_header_t;

	class request_promise :
		public netp::promise<std::tuple<int, NRP<message>>>
	{
		friend class client;
		fn_notify_header_t fn_header;
		fn_notify_body_t fn_body;
		long nbytes_notified;

	public:
		request_promise():
			fn_header(nullptr),
			fn_body(nullptr),
			nbytes_notified(0)
		{}

		request_promise( fn_notify_header_t const& header_end, fn_notify_body_t const& body ):
			fn_header(header_end),
			fn_body(body),
			nbytes_notified(0)
		{}
	};

	enum class http_request_state {
		S_REQUESTING,
		S_DONE
	};

	struct http_request_ctx:
		netp::non_atomic_ref_base
	{
		http_request_state state;
		NRP<netp::http::request_promise> reqp;
		NRP<netp::promise<int>> writep;
		NRP<netp::http::message> reqm;
	};

	typedef std::vector<NRP<http_request_ctx>,netp::allocator<NRP<http_request_ctx>>> request_ctx_vector_t;


	struct tls_cfg {
		NRP<netp::handler::tls_context> tlsctx;
	};

	struct http_cfg {
		bool close_on_response_done;
		bool dump_in;
		bool dump_out;
		tls_cfg tls;
		NRP<socket_cfg> cfg;
	};

	class client;
	typedef netp::promise<std::tuple<int, NRP<client>>> client_dial_promise;

	enum http_client_flag {
		f_close_on_response_done = 1
	};

	class client final:
		public netp::ref_base,
		public event_broker_any
	{
		enum class http_write_state {
			S_WRITE_CLOSED,
			S_WRITE_IDLE,
			S_WRITING,
			S_WRITE_BLOCKED,
		};
		
	private:
		NRP<netp::io_event_loop> m_loop;
		NRP<netp::channel_handler_context> m_ctx;
		NRP<netp::promise<int>> m_close_f;

		int m_flag;
		http_write_state m_wstate;
		string_t m_host;

		NRP<message> m_mtmp;
		request_ctx_vector_t m_reqs;
	private:
		void _do_request_done(int code, NRP<netp::http::message> const& r);
		void _do_close(NRP<netp::promise<int>> const& close_f);

	public:
		client( netp::string_t const& host, http_cfg const& http_cfg_, NRP<io_event_loop> const& L = nullptr ) :
			m_loop(L != nullptr ?L: io_event_loop_group::instance()->next()),
			m_flag(http_cfg_.close_on_response_done ? f_close_on_response_done : 0),
			m_wstate(http_write_state::S_WRITE_CLOSED),
			m_host(host)
		{
		}

		~client() {}

		NRP<io_event_loop> const& event_loop() const { return m_loop; }

		void http_cb_connected(NRP<netp::channel_handler_context> const& ctx_);
		void http_cb_closed(NRP<netp::channel_handler_context> const& ctx_);
		void http_write_closed(NRP<netp::channel_handler_context> const& ctx_);
		void http_cb_error(NRP<netp::channel_handler_context> const& ctx, int err);

		//void http_cb_parse_error(NRP<netp::channel_handler_context> const& ctx, int err);
		void http_cb_message_header(NRP<netp::channel_handler_context> const& ctx_, NRP<message> const& m);
		void http_cb_message_body(NRP<netp::channel_handler_context> const& ctx_, const char* data, u32_t len);
		void http_cb_message_end(NRP<netp::channel_handler_context> const& ctx_);

		void do_request(NRP<netp::http::request_promise> const& promise, NRP<netp::http::message> const& r, std::chrono::seconds timeout );

		void do_get(NRP<request_promise> const& reqp, const char* uri, size_t len, NRP<header> const& H, std::chrono::seconds const& timeout) {
			NRP<message> reqm = netp::make_ref<message>();
			reqm->url = string_t(uri, len);
			reqm->opt = option::O_GET;
			H != nullptr ? reqm->H = H : 0;
			do_request(reqp, reqm, timeout);
		}
		void do_get( NRP<request_promise> const& reqp, std::string const& uri, NRP<header> const& H,std::chrono::seconds const& timeout ) {
			do_get( reqp, uri.c_str(), uri.length(), H,timeout);
		}

		inline void do_get(NRP<request_promise> const& reqp, std::string const& uri, std::chrono::seconds const& timeout) {
			do_get( reqp, uri, nullptr,timeout);
		}

		inline NRP<request_promise> get(std::string const& uri, NRP<header> const& H, std::chrono::seconds const& timeout) {
			NRP<request_promise> rf = netp::make_ref<request_promise>();
			do_get( rf, uri, H,timeout);
			return rf;
		}

		inline NRP<request_promise> get(std::string const& uri, std::chrono::seconds const& timeout ) {
			NRP<request_promise> rf = netp::make_ref<request_promise>();
			do_get(rf, uri, timeout);
			return rf;
		}

		inline NRP<request_promise> get(std::string const& uri) {
			NRP<request_promise> rf = netp::make_ref<request_promise>();
			do_get(rf, uri, std::chrono::seconds(DEFAULT_HTTP_REQUEST_TIMEOUT));
			return rf;
		}

		void do_post(NRP<request_promise> const& reqp, const char* uri, size_t len, NRP<header> const& H, NRP<netp::packet> const& body, std::chrono::seconds const& timeout) {
			NRP<message> reqm = netp::make_ref<message>();
			reqm->url = string_t(uri, len);
			reqm->opt = option::O_POST;
			H != nullptr ? reqm->H = H : 0;
			body != nullptr ? reqm->body = body : 0;
			do_request( reqp, reqm,timeout);
		}

		void do_post( NRP<request_promise> const& reqp, std::string const& uri, NRP<header> const& H , NRP<netp::packet> const& body,std::chrono::seconds const& timeout ) {
			do_post( reqp, uri.c_str(), uri.length(), H, body,timeout);
		}

		inline NRP<netp::http::request_promise> post(std::string const& uri, NRP<header> const& H, NRP<netp::packet> const& body, std::chrono::seconds const& timeout ) {
			NRP<request_promise> rf = netp::make_ref<request_promise>();
			do_post( rf, uri, H, body,timeout);
			return rf;
		}

		inline NRP<netp::http::request_promise> post(std::string const& uri, NRP<header> const& H, NRP<netp::packet> const& body) {
			NRP<request_promise> rf = netp::make_ref<request_promise>();
			do_post( rf, uri, H, body,std::chrono::seconds(DEFAULT_HTTP_REQUEST_TIMEOUT) );
			return rf;
		}

		NRP<netp::promise<int>> close();
	};

	extern void do_dial(NRP<client_dial_promise> const& dp, const char* host, size_t len, http_cfg const& cfg = { false, false,false,{}, netp::make_ref<netp::socket_cfg>() });

	extern NRP<client_dial_promise> dial(const char* host, size_t len, http_cfg const& cfg = { false, false,false,{nullptr},netp::make_ref<netp::socket_cfg>() });
	extern NRP<client_dial_promise> dial(std::string const& host, http_cfg const& cfg = { false,false,false,{nullptr},netp::make_ref<netp::socket_cfg>() });

	extern void do_get(NRP<netp::http::request_promise> const& reqp, std::string const& url, std::chrono::seconds timeout = std::chrono::seconds(DEFAULT_HTTP_REQUEST_TIMEOUT));
	extern NRP<netp::http::request_promise> get(std::string const& url , std::chrono::seconds timeout = std::chrono::seconds(DEFAULT_HTTP_REQUEST_TIMEOUT) );

	extern void do_post(NRP<netp::http::request_promise> const& reqp, std::string const& url, NRP<header> const& H, NRP<netp::packet> const& body, std::chrono::seconds timeout = std::chrono::seconds(DEFAULT_HTTP_REQUEST_TIMEOUT));
	extern NRP<netp::http::request_promise> post(std::string const& url, NRP<header> const& H, NRP<netp::packet> const& body, std::chrono::seconds timeout = std::chrono::seconds(DEFAULT_HTTP_REQUEST_TIMEOUT));
}}

#endif