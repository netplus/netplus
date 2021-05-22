#ifndef _NETP_DNS_RESOLVER_HPP
#define _NETP_DNS_RESOLVER_HPP

#include <vector>
#include <unordered_map>

#include <netp/core.hpp>
#include <netp/string.hpp>

#define _NETP_USE_C_ARES

#ifdef _NETP_USE_UDNS
	#include "../../3rd/udns/0.4/udns.h"
#elif defined _NETP_USE_C_ARES
	#include "../../3rd/c-ares/c-ares-1.17.1/include/ares.h"
#endif

#include <netp/smart_ptr.hpp>
#include <netp/singleton.hpp>
#include <netp/promise.hpp>
#include <netp/ipv4.hpp>
#include <netp/io_monitor.hpp>

namespace netp {
	class socket_channel;
	struct io_ctx;
	class io_event_loop;
	class timer;

	typedef netp::promise< std::tuple<int, std::vector<netp::ipv4_t,netp::allocator<netp::ipv4_t>>>> dns_query_promise;

	class dns_resolver;
	struct async_dns_query
	{
		dns_resolver* dnsr;
		NRP<dns_query_promise> dnsquery_p;
	};

#ifdef _NETP_USE_C_ARES

#ifdef _NETP_USE_C_ARES
	enum ares_fd_monitor_flag {
		f_watch_read = 1 << 0,
		f_watch_write = 1 << 1
	};

	struct ares_fd_monitor final :
		public io_monitor
	{
		dns_resolver& _dnsr;
		int flag;
		SOCKET fd;
		io_ctx* ctx;

	public:
		ares_fd_monitor(dns_resolver& _dnsr_, SOCKET fd_);
		virtual void io_notify_terminating(int, io_ctx*) override;
		virtual void io_notify_read(int status, io_ctx*) override;
		virtual void io_notify_write(int status, io_ctx*) override;
	};

#endif
#endif

	class dns_resolver :
		public netp::singleton<dns_resolver>
	{
		friend class app;
		friend struct ares_fd_monitor;
		friend struct async_dns_query;
		enum dns_resolver_flag {
			f_timeout_timer = 1,
			f_stop_called = 1<<1,
			f_launching = 1<<2,
			f_running = 1<<3
		};

		NRP<io_event_loop> L;

#ifdef _NETP_USE_UDNS
		NRP<netp::socket_channel> m_so;
		struct dns_ctx* m_dns_ctx;
#endif

#ifdef _NETP_USE_C_ARES
		ares_channel m_ares_channel;
#endif

		NRP<netp::timer> m_tm_dnstimeout;
		std::vector<std::string> m_ns; //for restart
		u8_t m_flag;

#ifdef _NETP_USE_C_ARES
		typedef std::unordered_map<SOCKET, NRP<ares_fd_monitor>> ares_fd_monitor_map_t;
		typedef std::pair<SOCKET, NRP<ares_fd_monitor>> ares_fd_monitor_pair_t;
		ares_fd_monitor_map_t m_ares_fd_monitor_map;

	public:
		void __ares_wait();
		int __ares_socket_create_cb(ares_socket_t socket_fd, int type);
		void __ares_socket_state_cb(ares_socket_t socket_fd, int readable, int writable);
#endif

	private:
		void reset( NRP<io_event_loop> const& L );
		void _do_add_name_server();

		void _do_start(NRP<netp::promise<int>> const& p);
		NRP<netp::promise<int>> start();

		void _do_stop(NRP<netp::promise<int>> const& p);
		NRP<netp::promise<int>> stop();

		void cb_dns_timeout(NRP<netp::timer> const& t);
		void async_read_dns_reply( int status,io_ctx* ctx);

		void _do_resolve(string_t const& domain, NRP<dns_query_promise> const& p);

	public:
		dns_resolver();
		NRP<netp::promise<int>> add_name_server(std::vector<std::string> const& ns);
		NRP<dns_query_promise> resolve(string_t const& domain);
	};
}

#endif