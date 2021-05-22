#include <netp/core.hpp>

#include <netp/dns_resolver.hpp>
#include <netp/socket_api.hpp>
#include <netp/io_event_loop.hpp>

namespace netp {

#ifdef _NETP_USE_C_ARES
	const static int dns_error_map[] = {
		0,
		netp::E_DNS_TEMPORARY_ERROR,
		netp::E_DNS_PROTOCOL_ERROR,
		netp::E_DNS_DOMAIN_NAME_NOT_EXISTS,
		netp::E_DNS_DOMAIN_NO_DATA,
		netp::E_DNS_NOMEM,
		netp::E_DNS_BADQUERY
	};
#endif

	ares_fd_monitor::ares_fd_monitor(dns_resolver& _dnsr_, SOCKET fd_) :
		dnsr(_dnsr_),
		flag(0),
		fd(fd_),
		ctx(0)
	{}

	void ares_fd_monitor::io_end() {
		if (flag & f_watch_read) {
			dnsr.L->io_do(io_action::READ, ctx);
			flag &= ~f_watch_read;
		}

		if (flag & f_watch_write) {
			dnsr.L->io_do(io_action::WRITE, ctx);
			flag &= ~f_watch_write;
		}
		dnsr.L->io_end(ctx);
		NETP_CLOSE_SOCKET(fd);
		flag |= f_closed;
		dnsr.m_ares_fd_monitor_map.erase(fd);
	}

	void ares_fd_monitor::io_notify_terminating(int, io_ctx*) {
		io_end();
	}
	void ares_fd_monitor::io_notify_read(int status, io_ctx*) {
		if (status == netp::OK) {
			ares_process_fd(dnsr.m_ares_channel, fd, ARES_SOCKET_BAD);
		} else {
			io_end();
		}
	}
	void ares_fd_monitor::io_notify_write(int status, io_ctx*) {
		if (status == netp::OK) {
			ares_process_fd(dnsr.m_ares_channel, ARES_SOCKET_BAD, fd);
		} else {	
			io_end();
		}
	}

	void dns_resolver::reset( NRP<io_event_loop> const& L_ ) {

		NETP_ASSERT( L_== nullptr || (L_->poller_type() != io_poller_type::T_IOCP && L_->poller_type() != io_poller_type::T_POLLER_CUSTOM_1 && L_->poller_type() != io_poller_type::T_POLLER_CUSTOM_2));
		
		L = L_;
		m_ns.clear();
		m_flag = 0;

#ifdef _NETP_USE_U_DNS
		NETP_ASSERT(m_so == nullptr, "dns resolver check m_so failed");
		NETP_ASSERT(m_tm_dnstimeout == nullptr, "dns resolver check m_tm_dnstimeout failed");
#endif
	}

	dns_resolver::dns_resolver() :
		L(nullptr),
#ifdef _NETP_USE_UDNS
		m_so(nullptr),
		m_dns_ctx(&dns_defctx),
#endif
		m_flag(0)
	{
#ifdef _NETP_USE_UDNS
		dns_init(m_dns_ctx, 0);
#endif
	}

	void dns_resolver::_do_add_name_server() {
		NETP_ASSERT(L->in_event_loop());
		std::for_each(m_ns.begin(), m_ns.end(), [&](std::string const& serv) {
#ifdef _NETP_USE_UDNS
			dns_add_serv(m_dns_ctx, serv.c_str());
#endif
			NETP_DEBUG("[dns_resolver]add dns serv: %s", serv.c_str());
		});
	}

	NRP<netp::promise<int>> dns_resolver::add_name_server(std::vector<std::string> const& ns) {
		NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
		L->execute([dnsr = this, ns, p]() {
			dnsr->m_ns.insert(dnsr->m_ns.begin(), ns.begin(), ns.end());
			p->set(netp::OK);
		});
		return p;
	}

#ifdef _NETP_USE_C_ARES
	static void ___ares_socket_state_cb(void* data, ares_socket_t socket_fd, int readable, int writable) {
		NETP_ASSERT(data != 0);
		dns_resolver* dnsr = (dns_resolver*)data;
		dnsr->__ares_socket_state_cb(socket_fd, readable, writable);
	}
	static int ___ares_socket_create_cb(ares_socket_t socket_fd, int type, void* data) {
		NETP_ASSERT(data != 0);
		dns_resolver* dnsr = (dns_resolver*)data;
		return dnsr->__ares_socket_create_cb(socket_fd, type);
	}
#endif

	void dns_resolver::_do_start(NRP<netp::promise<int>> const& p) {
		NETP_ASSERT(L->in_event_loop());

		if ( m_flag &( u8_t(dns_resolver_flag::f_launching|dns_resolver_flag::f_running|dns_resolver_flag::f_stop_called)) ) {
			p->set(netp::E_INVALID_STATE);
			return;
		}

		m_flag |= dns_resolver_flag::f_launching;
#ifdef _NETP_USE_C_ARES
		int ares_init_rt = ares_library_init(ARES_LIB_INIT_ALL);
		if (ares_init_rt != ARES_SUCCESS) {
			p->set(ares_init_rt);
			return;
		}
		ares_options ares_opt;
		std::memset(&ares_opt, 0, sizeof(ares_opt));

		int ares_flag =ARES_OPT_SOCK_STATE_CB;
		ares_opt.sock_state_cb = ___ares_socket_state_cb;
		ares_opt.sock_state_cb_data = this;

		ares_flag |= ARES_OPT_FLAGS;
		ares_opt.flags = ARES_FLAG_STAYOPEN;
		
		ares_init_rt = ares_init_options(&m_ares_channel, &ares_opt, ares_flag);
		if (ares_init_rt != ARES_SUCCESS) {
			p->set(ares_init_rt);
			return;
		}

		ares_set_socket_callback(m_ares_channel, ___ares_socket_create_cb, this);

		NETP_ASSERT(m_tm_dnstimeout == nullptr);
		m_tm_dnstimeout = netp::make_ref<netp::timer>(std::chrono::milliseconds(250), &dns_resolver::cb_dns_timeout, this, std::placeholders::_1);
		m_flag |= f_timeout_timer;
		L->launch(m_tm_dnstimeout);

		m_flag &= ~dns_resolver_flag::f_launching;

		m_flag |= dns_resolver_flag::f_running;
		p->set(netp::OK);
#endif
		
#ifdef _NETP_USE_UDNS		
		NETP_ASSERT(m_dns_ctx != NULL);
		_do_add_name_server();
		int fd = dns_open(m_dns_ctx);
		if (fd < 0) {
			NETP_ERR("[dns_resolver]dns open failed: %d", fd);
			p->set(fd);
			return;
		}

		NRP<socket_cfg> cfg = netp::make_ref<socket_cfg>(L);
		cfg->fd = fd;
		cfg->family = NETP_AF_INET;
		cfg->type = NETP_SOCK_DGRAM;
		cfg->proto = NETP_PROTOCOL_UDP;
		cfg->L = L;

		int rt;
		std::tie(rt,m_so) = netp::create_socket_channel(cfg);
		if ( rt  != netp::OK ) {
			m_so->ch_close();
			m_so = nullptr;
			p->set(rt);
			return;
		}

		rt = m_so->bind_any();
		if (rt != netp::OK) {
			m_so->ch_close();
			m_so = nullptr;
			p->set(rt);
			return;
		}
		//libudns do not support iocp
		m_so->ch_set_active();
		m_so->ch_set_connected();
		
		m_so->ch_io_begin([dnsr=this, p](int status , io_ctx*) {
			NETP_ASSERT(dnsr->L->in_event_loop());
			NETP_ASSERT(dnsr->m_flag & dns_resolver_flag::f_launching);
			dnsr->m_flag &= dns_resolver_flag::f_launching;
			if (status == netp::OK) {
				NETP_INFO("[dns_resolver][%s]init done", dnsr->m_so->ch_info().c_str());
				dnsr->m_flag |= dns_resolver_flag::f_running;
				dnsr->m_so->ch_io_read(std::bind(&dns_resolver::async_read_dns_reply, dns_resolver::instance(), std::placeholders::_1, std::placeholders::_2));
				dnsr->m_so->ch_close_promise()->if_done([dnsr](int const&) {
					NETP_INFO("[dns_resolver][%s]dns socket_channel closed", dnsr->m_so->ch_info().c_str());
					dnsr->m_flag &= ~dns_resolver_flag::f_running;
					dnsr->m_so = nullptr;
					dnsr->m_tm_dnstimeout = nullptr;
					dnsr->_do_start(netp::make_ref<netp::promise<int>>());
				});

				dnsr->m_tm_dnstimeout = netp::make_ref<netp::timer>(std::chrono::milliseconds(200), &dns_resolver::cb_dns_timeout, dnsr, std::placeholders::_1);
				//tm always finished before loop terminated
				p->set(netp::OK);
			} else {
				p->set(status);
			}
		});
#endif
	}

	NRP<netp::promise<int>> dns_resolver::start() {
		NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
		L->execute([dnsr=this,p]() {
			dnsr->_do_start(p);
		});
		return p;
	}

	void dns_resolver::_do_stop(NRP<netp::promise<int>> const& p) {
		NETP_ASSERT(L->in_event_loop());
		if ((m_flag& dns_resolver_flag::f_running) == 0) {
			p->set(netp::E_INVALID_STATE);
			return;
		}
		m_flag &= ~dns_resolver_flag::f_running;

		ares_cancel(m_ares_channel);
		ares_destroy(m_ares_channel);
		m_ares_channel = 0;
		__ares_wait();

		ares_library_cleanup();

#ifdef _NETP_USE_UDNS
		m_so->ch_close();//force close fd first
		NETP_ASSERT(m_dns_ctx != nullptr);
		while (dns_active(m_dns_ctx) > 0) {
			//clear all pending request
			dns_timeouts(m_dns_ctx, -1, std::time(0) + (24*36000) );
		}

		NETP_ASSERT(dns_active(m_dns_ctx) == 0);
		dns_close(m_dns_ctx);
#endif

		NETP_ASSERT(m_tm_dnstimeout != nullptr);
		m_tm_dnstimeout = nullptr;

		NETP_INFO("[dns_resolver]exit");
		p->set(netp::OK);
	}

	NRP<netp::promise<int>> dns_resolver::stop() {
		NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
		L->execute([dnsr=this,p]() {
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

#ifdef _NETP_USE_C_ARES
		ares_fd_monitor_map_t::iterator it = m_ares_fd_monitor_map.begin();
		while (it != m_ares_fd_monitor_map.end()) {
			NRP<ares_fd_monitor> m = it->second;
			++it;

			ares_socket_t read_fd = m->flag & f_watch_read ? m->fd : ARES_SOCKET_BAD;
			ares_socket_t write_fd = m->flag & f_watch_write ? m->fd : ARES_SOCKET_BAD;
			if ( m->flag &(f_watch_read | f_watch_write) ) {
				ares_process_fd(m->dnsr.m_ares_channel, read_fd, write_fd);
			}
		}
#endif

#ifdef _NETP_USE_UDNS
		dns_timeouts(m_dns_ctx, -1, 0);
		if (dns_active(m_dns_ctx)>0) {
			m_flag |= f_timeout_timer;
			L->launch(t, netp::make_ref<netp::promise<int>>());
		}
#endif
	}

	void dns_resolver::async_read_dns_reply(int status, io_ctx* ctx_) {
		NETP_ASSERT(L->in_event_loop());

#ifdef _NETP_USE_UDNS
		//NETP_ASSERT(status == netp::OK);
#ifdef NETP_HAS_POLLER_IOCP
		iocp_ctx* ctx = (iocp_ctx*)ctx_;
		if (status > 0) {
			dns_ioevent_with_udpdata_in(m_dns_ctx, 0, (unsigned char*) ctx->ol_r->wsabuf.buf, status, ctx->ol_r->from_ptr );
			dns_ioevent(m_dns_ctx, 0);
			return;
		}
#endif
		(void*)ctx_;
		if (status == netp::OK) {
			//struct sockaddr_in addr_in;
			//::memset(&addr_in, 0, sizeof(addr_in));
			//addr_in.sin_family = u16_t(addr.family());
			//addr_in.sin_port = addr.nport();
			//addr_in.sin_addr.s_addr = addr.nipv4();
			//dns_ioevent_with_udpdata_in(m_dns_ctx, 0, in->head(), in->len(), &addr_in );
			dns_ioevent(m_dns_ctx, 0);
			return;
		}
#endif

		NETP_ERR("[dns_resolver]dns read error: %d", status);
		_do_stop(netp::make_ref<netp::promise<int>>());
	}

//#define NETP_FREE_ASYNC_DNS_QUERY(Q) \
//	Q->~async_dns_query(); \
//	netp::allocator<async_dns_query>::free(Q); \
//	Q = nullptr;

#ifdef _NETP_USE_UDNS
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
			netp::allocator<async_dns_query>::trash(adq);
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
		netp::allocator<async_dns_query>::trash(adq);
	}
#endif

#ifdef _NETP_USE_C_ARES

	void dns_resolver::__ares_wait() {
		NETP_ASSERT(m_ares_fd_monitor_map.size() == 0);
	}

	void dns_resolver::__ares_socket_state_cb(ares_socket_t socket_fd, int readable, int writable) {
		NETP_DEBUG("[dns_resolver]__ares_state_cb, fd: %d, readable: %d, writeable: %d", socket_fd, readable, writable);

		if ((m_flag & dns_resolver_flag::f_running) ==0) { return; }

		ares_fd_monitor_map_t::iterator it = m_ares_fd_monitor_map.find(socket_fd);
		NETP_ASSERT(it != m_ares_fd_monitor_map.end());
		NRP<ares_fd_monitor> m = it->second;
		if (readable == 1 && (m->flag &f_watch_read) ==0) {
			int rt = L->io_do(io_action::READ, m->ctx);
			if (rt != netp::OK) {
				netp::close(socket_fd);
			} else {
				m->flag |= f_watch_read;
			}
		} else if(readable == 0 && (m->flag&f_watch_read) ==1){
			L->io_do(io_action::END_READ, m->ctx);
			m->flag &= ~f_watch_read;
		}

		if (writable == 1 && (m->flag&f_watch_write) == 0) {
			int rt = L->io_do(io_action::WRITE, m->ctx);
			if (rt != netp::OK) {
				netp::close(socket_fd);
			} else {
				m->flag |= f_watch_write;
			}
		} else if(writable == 0 && (m->flag&f_watch_write) == 1){
			L->io_do(io_action::END_WRITE, m->ctx);
			m->flag &= ~f_watch_write;
		}

		if ( (readable == 0 && writable == 0) && m->flag == 0 ) {
			m->io_end();
		}
	}

	int dns_resolver::__ares_socket_create_cb(ares_socket_t socket_fd, int type) {
		NETP_DEBUG("[dns_resolver]__ares_socket_create_cb, fd: %d, type: %d", socket_fd, type);
		NETP_ASSERT( L != nullptr );
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT((m_flag & dns_resolver_flag::f_running));

		NRP<netp::ares_fd_monitor> mm = netp::make_ref<netp::ares_fd_monitor>(*this, socket_fd);
		io_ctx* ctx = L->io_begin(socket_fd, mm);
		if (ctx == nullptr) {
			return -1;
		}

		mm->ctx = ctx;
		m_ares_fd_monitor_map.insert({socket_fd, mm});
		return netp::OK;
	}

	static void __ares_gethostbyname_cb(void* arg, int status, int timeouts, struct hostent* hostent)
	{
		async_dns_query* adq = (async_dns_query*)arg;

		if (status == ARES_SUCCESS) {
			std::vector<netp::ipv4_t, netp::allocator<netp::ipv4_t>> ipv4s;

			char** lpSrc;
			for (lpSrc = hostent->h_addr_list; *lpSrc; lpSrc++) {

				switch (hostent->h_addrtype) {
				case AF_INET:
				{
					char addr_buf[32] = {0};
					ares_inet_ntop(hostent->h_addrtype, *lpSrc, addr_buf, sizeof(addr_buf));
					ipv4s.push_back( dotiptoip(addr_buf));

					//u_long nip = u32_t(*lpSrc);
					//ipv4s.push_back( ntohl(nip) );
				}
				break;
				case AF_INET6:
				{
					NETP_TODO("AF_INET6");
				}
				break;
				}
			}

			if (ipv4s.size()) {
				adq->dnsquery_p->set(std::make_tuple(netp::OK, ipv4s));
			} else {
				adq->dnsquery_p->set(std::make_tuple(netp::E_DNS_DOMAIN_NO_DATA, ipv4s));
			}
		} else {
			adq->dnsquery_p->set(std::make_tuple( status, std::vector<netp::ipv4_t, netp::allocator<netp::ipv4_t>>()));
		}

		netp::allocator<async_dns_query>::trash(adq);
	}
#endif

	void dns_resolver::_do_resolve(string_t const& domain, NRP<dns_query_promise> const& p) {
		NETP_ASSERT(L->in_event_loop());
		if ( (m_flag& dns_resolver_flag::f_running) == 0) {
			p->set(std::make_tuple(netp::E_INVALID_STATE,std::vector<ipv4_t,netp::allocator<ipv4_t>>()));
			return;
		}

		async_dns_query* adq = netp::allocator<async_dns_query>::make();
		adq->dnsr = this;
		adq->dnsquery_p = p;

#ifdef _NETP_USE_C_ARES
		ares_gethostbyname(m_ares_channel, domain.c_str(), AF_INET, __ares_gethostbyname_cb, adq);
#endif

#ifdef _NETP_USE_UDNS
		NETP_ASSERT(m_dns_ctx != NULL);
		struct dns_query* q = dns_submit_a4(m_dns_ctx, domain.c_str(), 0, dns_submit_a4_cb, (void*)adq);
		if (q == NULL) {
			dns_free_ptr(q);
			netp::allocator<async_dns_query>::trash(adq);

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
			L->launch(m_tm_dnstimeout, netp::make_ref<netp::promise<int>>());
		}
#endif
	}

	NRP<dns_query_promise> dns_resolver::resolve(string_t const& domain) {
		NRP<dns_query_promise> dnsp = netp::make_ref<dns_query_promise>();
		L->execute([dnsr=this, domain, dnsp]() {
			dnsr->_do_resolve(domain, dnsp);
		});
		return dnsp;
	}
}