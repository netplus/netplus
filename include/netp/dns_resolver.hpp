#ifndef _NETP_DNS_RESOLVER_HPP
#define _NETP_DNS_RESOLVER_HPP

#include <vector>
#include <netp/core.hpp>
#include <netp/string.hpp>

#include "../../3rd/udns/0.4/udns.h"
#include <netp/smart_ptr.hpp>
#include <netp/singleton.hpp>
#include <netp/promise.hpp>
#include <netp/ipv4.hpp>

namespace netp {
	class socket;
	struct aio_ctx;
	class io_event_loop;
	class timer;

	typedef netp::promise< std::tuple<int, std::vector<netp::ipv4_t,netp::allocator<netp::ipv4_t>>>> dns_query_promise;

	class dns_resolver;
	struct async_dns_query
	{
		dns_resolver* dnsr;
		NRP<dns_query_promise> dnsquery_p;
	};

	class dns_resolver :
		public netp::singleton<dns_resolver>
	{
		friend class app;
		friend struct async_dns_query;
		enum dns_resolver_flag {
			f_timeout_timer = 1,
			f_stop_called = 1<<1,
			f_launching = 1<<2,
			f_running = 1<<3
		};

		NRP<io_event_loop> m_loop;
		NRP<netp::socket> m_so;
		struct dns_ctx* m_dns_ctx;
		NRP<netp::timer> m_tm_dnstimeout;
		std::vector<std::string> m_ns; //for restart
		u8_t m_flag;

	private:
		void reset(NRP<io_event_loop> const& L);
		void _do_add_name_server();

		void _do_start(NRP<netp::promise<int>> const& p);
		NRP<netp::promise<int>> start();

		void _do_stop(NRP<netp::promise<int>> const& p);
		NRP<netp::promise<int>> stop();

		void cb_dns_timeout(NRP<netp::timer> const& t);
		void async_read_dns_reply( int aiort_,aio_ctx* ctx);

		void _do_resolve(string_t const& domain, NRP<dns_query_promise> const& p);

	public:
		dns_resolver();
		NRP<netp::promise<int>> add_name_server(std::vector<std::string> const& ns);
		NRP<dns_query_promise> resolve(string_t const& domain);
	};
}

#endif