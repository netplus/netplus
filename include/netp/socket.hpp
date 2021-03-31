#ifndef _NETP_SOCKET_HPP
#define _NETP_SOCKET_HPP

#include <netp/socket_channel.hpp>

#ifdef NETP_HAS_POLLER_IOCP
	#include <netp/socket_channel_iocp.hpp>
#endif

namespace netp {
	struct socket_url_parse_info {
		string_t proto;
		string_t host;
		port_t port;
	};

	extern int parse_socket_url(const char* url, size_t len, socket_url_parse_info& info);
	extern std::tuple<int, u8_t, u8_t, u16_t> inspect_address_info_from_dial_str(const char* dialstr);

	//we must make sure that the creation of the socket happens on its thead(L)
	extern void do_async_create_socket_channel(NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> const& p,NRP<netp::socket_cfg> const& cfg );
	extern NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> async_create_socket_channel(NRP<netp::socket_cfg> const& cfg);

	extern void do_dial(NRP<channel_dial_promise> const& dialp, address const& addr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg);

	extern void do_dial(NRP<channel_dial_promise> const& dialp, netp::size_t idx, std::vector<address> const& addrs, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg);
	extern void do_dial(NRP<channel_dial_promise> const& dialp, const char* dialurl, size_t len, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg);

	inline static void do_dial(NRP<channel_dial_promise> const& dialp, std::string const& dialurl, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& ccfg) {
		do_dial(dialp,dialurl.c_str(), dialurl.length(), initializer, ccfg);
	}

	inline static NRP<channel_dial_promise> dial(const char* dialurl, size_t len, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& ccfg) {
		NRP<channel_dial_promise> dialp = netp::make_ref<channel_dial_promise>();
		do_dial(dialp,dialurl, len, initializer, ccfg);
		return dialp;
	}

	inline static NRP<channel_dial_promise> dial(std::string const& dialurl, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& ccfg) {
		NRP<channel_dial_promise> dialp = netp::make_ref<channel_dial_promise>();
		do_dial( dialp, dialurl.c_str(), dialurl.length(), initializer,ccfg);
		return dialp;
	}

	inline static NRP<channel_dial_promise> dial(std::string const& dialurl, fn_channel_initializer_t const& initializer) {
		NRP<channel_dial_promise> dialp = netp::make_ref<channel_dial_promise>();
		do_dial( dialp, dialurl.c_str(), dialurl.length(), initializer,netp::make_ref<socket_cfg>());
		return dialp;
	}

	/*
	 example:
		socket::listen("tcp://127.0.0.1:80", [](NRP<channel> const& ch) {
			NRP<netp::channel_handler_abstract> h = netp::make_ref<netp::handler::echo>():
			ch->pipeline()->add_last(h);
		});
	*/
	extern void do_listen_on(NRP<channel_listen_promise> const& listenp, address const& laddr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog);
	extern NRP<channel_listen_promise> listen_on(const char* listenurl, size_t len, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);

	inline static NRP<channel_listen_promise> listen_on(std::string const& listenurl, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG) {
		return listen_on(listenurl.c_str(), listenurl.length(), initializer, cfg, backlog);
	}

	inline static NRP<channel_listen_promise> listen_on(const char* listenurl, size_t len, fn_channel_initializer_t const& initializer) {
		return listen_on(listenurl, len, initializer, netp::make_ref<socket_cfg>(), NETP_DEFAULT_LISTEN_BACKLOG);
	}

	inline static NRP<channel_listen_promise> listen_on(std::string const& listenurl, fn_channel_initializer_t const& initializer) {
		return listen_on(listenurl.c_str(), listenurl.length(), initializer, netp::make_ref<socket_cfg>(), NETP_DEFAULT_LISTEN_BACKLOG);
	}
}
#endif