#include <netp/core.hpp>

#include <netp/dns_resolver.hpp>
#include <netp/socket.hpp>
#include <netp/io_event_loop.hpp>
#include <netp/timer.hpp>

namespace netp {
	const static int dns_error_map[] = {
		0,
		netp::E_DNS_TEMPORARY_ERROR,
		netp::E_DNS_PROTOCOL_ERROR,
		netp::E_DNS_DOMAIN_NAME_NOT_EXISTS,
		netp::E_DNS_DOMAIN_NO_DATA,
		netp::E_DNS_NOMEM,
		netp::E_DNS_BADQUERY
	};

	void dns_resolver::reset(NRP<io_event_loop> const& L) {
		m_loop = L;
		m_ns.clear();
		m_flag = 0;

		NETP_ASSERT(m_so == nullptr);
		NETP_ASSERT(m_tm_dnstimeout == nullptr);
	}

	dns_resolver::dns_resolver() :
		m_loop(nullptr),
		m_so(nullptr),
		m_dns_ctx(&dns_defctx),
		m_flag(0)
	{
		dns_init(m_dns_ctx, 0);
	}

	void dns_resolver::_do_add_name_server() {
		NETP_ASSERT(m_loop->in_event_loop());
		std::for_each(m_ns.begin(), m_ns.end(), [&](std::string const& serv) {
			dns_add_serv(m_dns_ctx, serv.c_str());
			NETP_DEBUG("[dns_resolver]add dns serv: %s", serv.c_str());
		});
	}

	NRP<netp::promise<int>> dns_resolver::add_name_server(std::vector<std::string> const& ns) {
		NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
		m_loop->execute([dnsr = this, ns, p]() {
			dnsr->m_ns.insert(dnsr->m_ns.begin(), ns.begin(), ns.end());
			p->set(netp::OK);
		});
		return p;
	}

	void dns_resolver::_do_start(NRP<netp::promise<int>> const& p) {

		if ( m_flag &( u8_t(dns_resolver_flag::f_launching|dns_resolver_flag::f_running|dns_resolver_flag::f_stop_called)) ) {
			p->set(netp::E_INVALID_STATE);
			return;
		}
		m_flag = dns_resolver_flag::f_launching;
		NETP_ASSERT(m_dns_ctx != NULL);
		_do_add_name_server();
		int fd = dns_open(m_dns_ctx);
		if (fd < 0) {
			NETP_ERR("[dns_resolver]dns open failed: %d", fd);
			p->set(fd);
			return;
		}

		m_tm_dnstimeout = netp::make_ref<netp::timer>(std::chrono::milliseconds(200), &dns_resolver::cb_dns_timeout, this, std::placeholders::_1);
		//tm always finished before loop terminated

		NRP<socket_cfg> cfg = netp::make_ref<socket_cfg>(m_loop);
		cfg->fd = fd;
		cfg->family = NETP_AF_INET;
		cfg->type = NETP_SOCK_DGRAM;
		cfg->proto = NETP_PROTOCOL_UDP;

		int rt;
		std::tie(rt, m_so) = socket::create(cfg);
		if (rt != netp::OK) {
			p->set(rt);
			return;
		}

		//libudns do not support iocp
		m_so->ch_set_active();
		m_so->ch_set_connected();
		m_so->aio_begin([dnsr=this, p](const int aiort) {
			NETP_ASSERT(dnsr->m_flag & dns_resolver_flag::f_launching);
			dnsr->m_flag &= dns_resolver_flag::f_launching;
			if (aiort == netp::OK) {
				NETP_DEBUG("[dns_resolver][%s]init done", dnsr->m_so->info().c_str());
				dnsr->m_flag |= dns_resolver_flag::f_running;
				dnsr->m_so->ch_aio_read(std::bind(&dns_resolver::async_read_dns_reply, dns_resolver::instance(), std::placeholders::_1));
				dnsr->m_so->ch_close_promise()->if_done([dnsr](int const&) {
					dnsr->m_flag &= ~dns_resolver_flag::f_running;
					dnsr->m_so = nullptr;
					dnsr->_do_start(netp::make_ref<netp::promise<int>>());
				});
				p->set(netp::OK);
			} else {
				p->set(aiort);
			}
		});
	}

	NRP<netp::promise<int>> dns_resolver::start() {
		NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
		m_loop->execute([dnsr=this,p]() {
			dnsr->_do_start(p);
		});
		return p;
	}

	void dns_resolver::_do_stop(NRP<netp::promise<int>> const& p) {
		NETP_ASSERT(m_loop->in_event_loop());
		if ((m_flag& dns_resolver_flag::f_running) == 0) {
			p->set(netp::E_INVALID_STATE);
			return;
		}
		m_flag &= ~dns_resolver_flag::f_running;
		m_so->ch_close();//force close fd first
		NETP_ASSERT(m_dns_ctx != nullptr);
		while (dns_active(m_dns_ctx) > 0) {
			//clear all pending request
			dns_timeouts(m_dns_ctx, -1, std::time(0) + (24*36000) );
		}

		NETP_ASSERT(dns_active(m_dns_ctx) == 0);
		dns_close(m_dns_ctx);
		m_tm_dnstimeout = nullptr;

		NETP_INFO("[dns_resolver]exit");
		p->set(netp::OK);
	}

	NRP<netp::promise<int>> dns_resolver::stop() {
		NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
		m_loop->execute([dnsr=this,p]() {
			dnsr->m_flag |= f_stop_called;
			dnsr->_do_stop(p);
		});
		return p;
	}

	void dns_resolver::cb_dns_timeout(NRP<netp::timer> const& t) {
		NETP_ASSERT(m_flag&f_timeout_timer);
		m_flag &= ~f_timeout_timer;
		if ( (m_flag& dns_resolver_flag::f_running) == 0 ) {
			return;
		}
		dns_timeouts(m_dns_ctx, -1, 0);
		if (dns_active(m_dns_ctx)>0) {
			m_flag |= f_timeout_timer;
			m_loop->launch(t, netp::make_ref<netp::promise<int>>());
		}
	}

	void dns_resolver::async_read_dns_reply(const int aiort_) {
		NETP_ASSERT(m_loop->in_event_loop());
		if (aiort_ == netp::OK) {
			dns_ioevent(m_dns_ctx, 0);
			return;
		}

		NETP_ERR("[dns_resolver]dns read error: %d, restart", aiort_);
		_do_stop(netp::make_ref<netp::promise<int>>());
	}

	static void dns_submit_a4_cb(struct dns_ctx* ctx, struct dns_rr_a4* result, void* data) {
		NETP_ASSERT(ctx != NULL);
		NETP_ASSERT(data != NULL);
		async_dns_query* adq = (async_dns_query*)data;
		NETP_ASSERT(adq->dnsquery_p != nullptr);
		if (result == NULL) {
			int code = dns_status(ctx);
			NETP_ASSERT(code != netp::OK);
			NETP_ASSERT(code >= ::DNS_E_BADQUERY && code <= ::DNS_E_TEMPFAIL);
			NETP_ERR("[dns_resolver]dns resolve failed: %d:%s", code, dns_strerror(code));
			adq->dnsquery_p->set(std::make_tuple(dns_error_map[NETP_ABS(code)], std::vector<ipv4_t, netp::allocator<ipv4_t>>()));
			NETP_DELETE(adq);
			return;
		}

		std::vector<ipv4_t, netp::allocator<ipv4_t>> ipv4s;
		if (result->dnsa4_nrr > 0) {
			for (int i = 0; i < result->dnsa4_nrr; ++i) {
				ipv4s.push_back( ntohl(result->dnsa4_addr[i].s_addr) );
			}
		}

		if (ipv4s.size()) {
			adq->dnsquery_p->set(std::make_tuple(netp::OK, ipv4s));
		} else {
			adq->dnsquery_p->set(std::make_tuple(netp::E_DNS_DOMAIN_NO_DATA, ipv4s));
		}
		dns_free_ptr(result);
		NETP_DELETE(adq);
	}

	void dns_resolver::_do_resolve(string_t const& domain, NRP<dns_query_promise> const& p) {
		NETP_ASSERT(m_loop->in_event_loop());
		if ( (m_flag& dns_resolver_flag::f_running) == 0) {
			p->set(std::make_tuple(netp::E_INVALID_STATE,std::vector<ipv4_t,netp::allocator<ipv4_t>>()));
			return;
		}
		NETP_ASSERT(m_dns_ctx != NULL);

		async_dns_query* adq= new async_dns_query();
		adq->dnsr = this;
		adq->dnsquery_p = p;
		struct dns_query* q = dns_submit_a4(m_dns_ctx, domain.c_str(), 0, dns_submit_a4_cb, (void*)adq);
		if (q == NULL) {
			NETP_DELETE(adq);

			int code = dns_status(m_dns_ctx);
			NETP_ASSERT(code != netp::OK);
			NETP_ASSERT(code >= ::DNS_E_BADQUERY && code <= ::DNS_E_TEMPFAIL);
			NETP_ERR("[dns_resolver]dns resolve failed: %d:%s", code, dns_strerror(code));
			p->set(std::make_tuple(dns_error_map[NETP_ABS(code)],std::vector<ipv4_t, netp::allocator<ipv4_t>>()));
			return;
		}

		dns_timeouts(m_dns_ctx, -1, 0);
		if ( (m_flag&f_timeout_timer) == 0) {
			m_flag |= f_timeout_timer;
			m_loop->launch(m_tm_dnstimeout, netp::make_ref<netp::promise<int>>());
		}
	}

	NRP<dns_query_promise> dns_resolver::resolve(string_t const& domain) {
		NRP<dns_query_promise> dnsp = netp::make_ref<dns_query_promise>();
		m_loop->execute([dnsr=this, domain, dnsp]() {
			dnsr->_do_resolve(domain, dnsp);
		});
		return dnsp;
	}
}