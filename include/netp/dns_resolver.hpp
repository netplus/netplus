#ifndef _NETP_DNS_RESOLVER_HPP
#define _NETP_DNS_RESOLVER_HPP

#include <vector>
#include <unordered_map>

#include <netp/core.hpp>
#include <netp/string.hpp>

#include <netp/ipv4.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/promise.hpp>
#include <netp/io_monitor.hpp>

namespace netp {
	class event_loop;
	class socket_channel;
	struct io_ctx;

	typedef netp::promise< std::tuple<int, std::vector<netp::ipv4_t,netp::allocator<netp::ipv4_t>>>> dns_query_promise;

	class dns_resolver;
	struct async_dns_query
	{
		NRP<dns_resolver> dnsr;
		NRP<dns_query_promise> dnsquery_p;

#ifdef _NETP_DEBUG
		netp::string_t host;
#endif
	};

	enum ares_fd_monitor_flag {
		f_ares_fd_watch_read	= 1 << 0,
		f_ares_fd_watch_write	= 1 << 1,
		f_ares_fd_closed		= 1 << 2
	};

	struct ares_fd_monitor final :
		public io_monitor
	{
		dns_resolver& dnsr;
		int flag;
		SOCKET fd;
		io_ctx* ctx;

	public:
		ares_fd_monitor(dns_resolver& _dnsr_, SOCKET fd_);
		void io_end();
		virtual void io_notify_terminating(int, io_ctx*) override;
		virtual void io_notify_read(int status, io_ctx*) override;
		virtual void io_notify_write(int status, io_ctx*) override;
	};

	//@impl note:
	//memory pool not used for the currently impl
	//netp::allocator use tls data to store allocator pointer, if we share a dns resolver in between multi-eventloops, the following case should be taken into consideration
	//1, thread safe alloc/dealloc
	
	enum dns_resolver_flag {
		f_drf_stop_called = 1 << 0,
		f_drf_launching = 1 << 1,
		f_drf_running = 1 << 2,
		f_drf_restarting = 1 << 3,
		f_drf_restarting_pending = 1 << 4,
		f_drf_timeout_timer = 1 << 5,
		f_drf_timeout_barrier = 1 << 6 //pending restart action
	};

	class event_loop;
	class dns_resolver :
		public netp::ref_base
	{
		friend class event_loop;
		friend struct ares_fd_monitor;
		friend struct async_dns_query;
		
		NRP<event_loop> L;

		void* m_ares_channel;
		long m_ares_active_query;

		NRP<netp::timer> m_tm_dnstimeout;
		std::vector<netp::string_t, netp::allocator<netp::string_t>> m_ns; //for restart
		u8_t m_flag;

		typedef std::unordered_map<SOCKET, NRP<ares_fd_monitor>, std::hash<SOCKET>, std::equal_to<SOCKET>, netp::allocator<std::pair<const SOCKET, NRP<ares_fd_monitor>>>> ares_fd_monitor_map_t;
		typedef std::pair<SOCKET, NRP<ares_fd_monitor>> ares_fd_monitor_pair_t;
		ares_fd_monitor_map_t m_ares_fd_monitor_map;

	public:
		SOCKET __ares_socket_create(int af, int type, int proto);
		int __ares_socket_close(SOCKET fd);
		void __ares_socket_state_cb(SOCKET socket_fd, int readable, int writable);
		inline void __ares_done() { NETP_ASSERT(m_ares_active_query>0); --m_ares_active_query; }
		void __ares_check_timeout();

	private:
		void _do_add_name_server();

		void _do_start(NRP<netp::promise<int>> const& p);
		NRP<netp::promise<int>> start();

		void _do_stop(NRP<netp::promise<int>> const& p);
		NRP<netp::promise<int>> stop();

		void restart();

		void cb_dns_timeout(NRP<netp::timer> const& t);
		void _do_resolve(string_t const& domain, NRP<dns_query_promise> const& p);

	public:
		dns_resolver(NRP<event_loop> const& L_);
		~dns_resolver();
		
		void init();
		void deinit();

		NRP<netp::promise<int>> add_name_server(std::vector<netp::string_t, netp::allocator<netp::string_t>> const& ns);
		NRP<dns_query_promise> resolve(string_t const& domain);

		static void __ares_gethostbyname_cb(void* arg, int status, int timeouts, struct hostent* hostent);
	};
}

#endif