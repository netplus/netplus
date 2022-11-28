#include <netp/core.hpp>
#include <netp/app.hpp>
#include <netp/socket_channel.hpp>

namespace netp {

	int socket_channel::open() {
		NETP_ASSERT(m_chflag & int(channel_flag::F_CLOSED));
		NETP_ASSERT(m_fd == NETP_INVALID_SOCKET);
		int rt = socket_open_impl();
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		m_chflag &= ~int(channel_flag::F_CLOSED);
		return netp::OK;
	}

	int socket_channel::close() {
		int rt = socket_close_impl();
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}
	int socket_channel::shutdown(int flag) {
		int rt = socket_shutdown_impl(flag);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_channel::bind(NRP<address> const& addr) {
		NETP_ASSERT(!m_laddr||m_laddr->is_af_unspec());
		NETP_ASSERT((m_family) == addr->family());
		int rt = socket_bind_impl(addr);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		m_laddr = addr->clone();
		return netp::OK;
	}

#define __NETP_UDP_PORT_MIN (netp::u16_t(32768))
#define __NETP_UDP_PORT_MAX (netp::u16_t(65535))

	int socket_channel::bind_any() {
		NRP<address >_any_ = netp::make_ref<address>();
		NETP_ASSERT(m_family != NETP_AF_UNSPEC);
		_any_->setfamily(m_family);
		_any_->setipv4(dotiptoip("0.0.0.0"));
		int rt;
		if (m_protocol == NETP_PROTOCOL_UDP) {
			do {
				_any_->setport(netp::port_t(netp::random(__NETP_UDP_PORT_MIN, __NETP_UDP_PORT_MAX)));
				rt = bind(_any_);
				if (rt == netp::E_EADDRINUSE || rt == netp::E_EACCESS) {
					netp::this_thread::yield();
					continue;
				}
				break;
			} while (true);
		} else {
			rt = bind(_any_);
		}

		if (rt != netp::OK) {
			return rt;
		}

		rt = load_sockname();
		NETP_TRACE_SOCKET("[socket][%s]bind rt: %d", ch_info().c_str(), rt);
		return rt;
	}

	int socket_channel::connect(NRP<address> const& addr) {
		if (m_chflag & (int(channel_flag::F_CONNECTING) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_LISTENING) | int(channel_flag::F_CLOSED)) ) {
			return netp::E_SOCKET_INVALID_STATE;
		}
		
		if ( (sock_protocol() == NETP_PROTOCOL_UDP) && (!m_laddr||m_laddr->is_af_unspec())) {
			//@note: if bind happens after connect, it shall always fail with 10022 on win
			int status = bind_any();
			if (status != netp::OK) {
				return status;
			}
		}

		NETP_ASSERT( m_raddr == nullptr || m_raddr->is_af_unspec() );
		m_raddr = addr->clone();
		int rt = socket_connect_impl(m_raddr);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_channel::listen(int backlog) {
		NETP_ASSERT(L->in_event_loop());
		if (m_chflag & int(channel_flag::F_ACTIVE)) {
			return netp::E_SOCKET_INVALID_STATE;
		}
		if (is_udp()) { return netp::OK; }
		NETP_ASSERT((m_fd > 0) && (m_chflag & int(channel_flag::F_LISTENING)) == 0);
		m_chflag |= int(channel_flag::F_LISTENING);
		int rt = socket_listen_impl(backlog);
		NETP_TRACE_SOCKET("[socket][%s]listen rt: %d", ch_info().c_str(), rt);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_channel::set_snd_buffer_size(u32_t size) {
		NETP_ASSERT(m_fd > 0);
		bool force_reload_buf_size = false;
		if (size == 0) {//0 for default
			force_reload_buf_size = (m_snd_buf_size == 0);
			goto _label_reload; //nothing changed
		} else if (size < u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE);
		} else if (size > u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE);
		}

		int rt;
#ifdef _NETP_DEBUG
		//rt = get_snd_buffer_size();
		//NETP_TRACE_SOCKET("[socket_base][#%d]snd buffer size: %u, try to set: %u", m_fd, rt, size);
#endif

		rt = socket_setsockopt_impl(SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size));
		if (rt == NETP_SOCKET_ERROR) {
			return netp_socket_get_last_errno();
		}
		force_reload_buf_size = true;
_label_reload:
		if (force_reload_buf_size) {
			return _cfg_load_snd_buf_size();
		} else {
			return netp::OK;
		}
	}

	int socket_channel::get_snd_buffer_size() const {
		NETP_ASSERT(m_fd > 0);
		int size=0;
		socklen_t opt_length = sizeof(u32_t);
		int rt = socket_getsockopt_impl(SOL_SOCKET, SO_SNDBUF, (char*)&size, &opt_length);
		if (rt == NETP_SOCKET_ERROR) {
			return netp_socket_get_last_errno();
		}
		return size;
	}

	/*
int socket_base::get_left_snd_queue() const {
#ifdef _NETP_GNU_LINUX
		if (m_fd ==NETP_INVALID_SOCKET ) {
			return netp::E_INVALID_OPERATION;
		}

		int size;
		int rt = ::ioctl(m_fd, TIOCOUTQ, &size);

		NETP_RETURN_V_IF_MATCH(socket_get_last_errno(), rt == NETP_SOCKET_ERROR );
		return size;
#else
		NETP_THROW("this operation does not supported on windows");
#endif
	}
	*/

	int socket_channel::set_rcv_buffer_size(u32_t size) {
		NETP_ASSERT(m_fd != NETP_INVALID_SOCKET);
		bool force_reload_buf_size = false;
		if (size == 0) {//0 for default
			force_reload_buf_size = (m_rcv_buf_size == 0);
			goto _label_reload;
		} else if (size < u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE);
		} else if (size > u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE);
		}

		int rt;
#ifdef _NETP_DEBUG
		//rt = get_rcv_buffer_size();
		//NETP_TRACE_SOCKET("[socket_base][#%d]rcv buffer size: %u, try to set: %u", m_fd, rt, size);
#endif

		rt = socket_setsockopt_impl(SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size));
		if (rt == NETP_SOCKET_ERROR) {
			return netp_socket_get_last_errno();
		}
		force_reload_buf_size = true;
	_label_reload:
		if (force_reload_buf_size) {
			return _cfg_load_rcv_buf_size();
		} else {
			return netp::OK;
		}
	}

	int socket_channel::get_rcv_buffer_size() const {
		NETP_ASSERT(m_fd > 0);
		int size=0;
		socklen_t opt_length = sizeof(size);
		int rt = socket_getsockopt_impl(SOL_SOCKET, SO_RCVBUF, (char*)&size, &opt_length);
		if (rt == NETP_SOCKET_ERROR) {
			return netp_socket_get_last_errno();
		}
		return size;
	}

	/*
	int socket_base::get_left_rcv_queue() const {
		if (m_fd == NETP_INVALID_SOCKET ) {
			return netp::E_INVALID_OPERATION;
		}
		int size=0;
#ifdef _NETP_GNU_LINUX
		int rt = ioctl(m_fd, FIONREAD, size);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
#else
		u_long ulsize;
		int rt = ::ioctlsocket(m_fd, FIONREAD, &ulsize);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		size = ulsize & 0xFFFFFFFF;
#endif
		return size;
	}
	*/

	int socket_channel::get_linger(bool& on_off, int& linger_t) const {
		NETP_ASSERT(m_fd > 0);
		struct linger soLinger;
		socklen_t opt_length = sizeof(soLinger);
		int rt = socket_getsockopt_impl(SOL_SOCKET, SO_LINGER, (char*)&soLinger, &opt_length);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);

		on_off = (soLinger.l_onoff != 0);
		linger_t = soLinger.l_linger;
		return netp::OK;
	}

	int socket_channel::set_linger(bool on_off, int linger_t /* in seconds */) {
		struct linger soLinger;
		NETP_ASSERT(m_fd > 0);
		soLinger.l_onoff = on_off;
		soLinger.l_linger = (linger_t & 0xFFFF);
		int rt = socket_setsockopt_impl(SOL_SOCKET, SO_LINGER, (char*)&soLinger, sizeof(soLinger));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_channel::get_tos(u8_t& tos) const {
		u8_t _tos;
		socklen_t length=sizeof(u8_t);

		int rt = socket_getsockopt_impl(IPPROTO_IP, IP_TOS, (char*)&_tos, &length);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		tos = IPTOS_TOS(_tos);
		return netp::OK;
	}

	int socket_channel::cfg_tos(u8_t tos) {
		NETP_ASSERT(m_fd > 0);
		u8_t _tos = (IPTOS_TOS(tos) | 0xe0);
		int rt = socket_setsockopt_impl(IPPROTO_IP, IP_TOS, (char*)&_tos, sizeof(_tos));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	void socket_channel::_tmcb_tx_limit(NRP<timer> const& t) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT( (m_tx_limit>0) && (m_chflag&int(channel_flag::F_TX_LIMIT_TIMER)) );

		m_chflag &= ~int(channel_flag::F_TX_LIMIT_TIMER);
		if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN))) {
			return;
		}
		NETP_ASSERT( (m_chflag & int(channel_flag::F_WRITE_ERROR)) == 0);
		//netp::now<bfr_duration_t, bfr_clock_t>().time_since_epoch().count()
		const long long usnow = netp::now<netp::microseconds_duration_t, netp::steady_clock_t>().time_since_epoch().count();
		const long long txlimit_delta = ( (usnow - m_tx_limit_last_tp));

		m_tx_limit_last_tp = usnow;
		u32_t tokens = u32_t((m_tx_limit/1000000.0f)* txlimit_delta);
		if ( m_tx_limit <= (tokens+m_tx_budget)) {
			m_tx_budget = m_tx_limit;
		} else {
			m_chflag |= int(channel_flag::F_TX_LIMIT_TIMER);
			m_tx_budget += tokens;

			NRP<netp::promise<int>> lp = netp::make_ref<netp::promise<int>>();
			lp->if_done([ch=NRP<socket_channel>(this)]( int rt) {
				if (rt != netp::OK) {
					ch->m_chflag |= int(channel_flag::F_WRITE_ERROR);
					ch->ch_errno() = rt;
					ch->ch_close_impl(nullptr);
				}
			});
			L->launch(t);
		}

		if (m_chflag & int(channel_flag::F_TX_LIMIT)) {
			NETP_ASSERT( !(m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE))));
			m_chflag &= ~int(channel_flag::F_TX_LIMIT);

#ifdef NETP_ENABLE_FAST_WRITE
			m_chflag |= int(channel_flag::F_WRITE_BARRIER);
			ch_is_connected() ? __do_io_write(netp::OK, m_io_ctx) : __do_io_write_to(netp::OK, m_io_ctx);
			m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
#else
			ch_io_write();
#endif
		}
	}

	void socket_channel::do_listen_on(NRP<promise<int>> const& intp, NRP<address> const& addr, fn_channel_initializer_t const& fn_accepted_initializer, NRP<socket_cfg> const& listener_cfg, int backlog ) {
		if (!L->in_event_loop()) {
			L->schedule([_this=NRP<socket_channel>(this), addr, fn_accepted_initializer, intp, listener_cfg, backlog]() ->void {
				_this->do_listen_on(intp, addr, fn_accepted_initializer, listener_cfg, backlog);
			});
			return;
		}

		//udp socket do not support this kinds of operation
		//only tcp or user defined protocol that that has implement accept feature by custom channel
		if (m_protocol == NETP_PROTOCOL_UDP) {
			m_chflag |= int(channel_flag::F_READ_ERROR);
			ch_errno() = netp::E_INVALID_OPERATION;
			ch_close_impl(nullptr);
			intp->set(netp::E_INVALID_OPERATION);
			return;
		}

		//int rt = -10043;
		int rt = socket_channel::bind(addr);
		if (rt != netp::OK) {
			NETP_WARN("[socket][#%d]bind(%s): %d", m_fd, addr->to_string().c_str(), rt );
			m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
			ch_errno() = rt;
			ch_close_impl(nullptr);
			intp->set(rt);
			return;
		}

		rt = socket_channel::listen(backlog);
		if (rt != netp::OK) {
			NETP_WARN("[socket][#%d]listen(%u): %d, addr: %s", m_fd, backlog, rt, addr->to_string().c_str());
			m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
			ch_errno() = rt;
			ch_close_impl(nullptr);
			intp->set(rt);
			return;
		}

		NETP_ASSERT(rt == netp::OK);

		NRP<socket_cfg> _lcfg = listener_cfg->clone();
		_lcfg->family = m_family;
		_lcfg->type = m_type;
		_lcfg->proto = m_protocol;

		ch_io_begin([intp, fn_accepted_initializer, _lcfg, ch = NRP<socket_channel>(this)]( int status, io_ctx*){
			intp->set(status);
			if(status == netp::OK) {
				ch->ch_io_accept(fn_accepted_initializer, _lcfg);
			}
		});
	}

	void socket_channel::do_dial(NRP<promise<int>> const& dialp, NRP<address> const& addr, fn_channel_initializer_t const& fn_initializer ) {
		NETP_ASSERT(L->in_event_loop());
		ch_io_begin([dialp, so=NRP<socket_channel>(this),addr, fn_initializer](int status, io_ctx*) {
			NETP_ASSERT(so->L->in_event_loop());
			if (status != netp::OK) {
				dialp->set(status);
				return;
			}
			so->ch_set_active();
			int rt = so->connect(addr);
			if (rt == netp::OK) {
				NETP_TRACE_SOCKET("[socket][%s]connected directly", so->ch_info().c_str());
				so->__do_io_dial_done(fn_initializer, dialp, netp::OK, so->m_io_ctx);
				return;
			}

			//rt = netp_socket_get_last_errno();
			if (netp::E_EINPROGRESS==rt) {
				//note: F_CONNECTING would be cleared in the following case
				//1, dial failed
				
				//not the following case:
				//1, dial canceled
				//2, io_do(write) failed
				
				NETP_ASSERT(so->sock_protocol() != NETP_PROTOCOL_UDP);
				so->ch_flag() |= int(channel_flag::F_CONNECTING);
				auto fn_connect_done = std::bind(&socket_channel::__do_io_dial_done, so, fn_initializer, dialp, std::placeholders::_1, std::placeholders::_2);
				so->ch_io_connect(fn_connect_done);
				return;
			}

#ifdef _NETP_DEBUG
			NETP_ASSERT( (so->ch_flag() &( int(channel_flag::F_CONNECTING)|int(channel_flag::F_CONNECTED) )) ==0 );
#endif
			so->ch_flag() |= int(channel_flag::F_WRITE_ERROR);
			so->ch_errno() = rt;
			so->ch_close_impl(nullptr);
			dialp->set(rt);
		});
	}

	SOCKET socket_channel::accept( NRP<address>& raddr, NRP<address>& laddr ) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_chflag & int(channel_flag::F_LISTENING));
		if ( (m_chflag & int(channel_flag::F_LISTENING)) ==0 ) {
			netp_socket_set_last_errno(netp::E_SOCKET_INVALID_STATE);
			return SOCKET(NETP_SOCKET_ERROR);
		}

		return socket_accept_impl(raddr, laddr);
	}

	void socket_channel::__do_io_dial_done(fn_channel_initializer_t const& fn_initializer, NRP<promise<int>> const& dialp_, int status, io_ctx*) {
		NETP_ASSERT(L->in_event_loop());
		NRP<promise<int>> dialp = dialp_;

		if (status != netp::OK) {
		_set_fail_and_return:
			NETP_ASSERT( 0==(m_chflag&int(channel_flag::F_CONNECTED)) );
			m_chflag |= int(channel_flag::F_WRITE_ERROR);
			m_chflag &= ~int(channel_flag::F_CONNECTING);
			m_cherrno = status;
			ch_io_end_connect();
			ch_close_impl(netp::make_ref<netp::promise<int>>());
			NETP_WARN("[socket][%s]dial error: %d", ch_info().c_str(), status);
			dialp->set(status);
			return;
		}

		if( m_chflag& int(channel_flag::F_CLOSED)) {
			NETP_WARN("[socket][%s]closed already, dial promise set abort, errno: %d", ch_info().c_str(), ch_errno() );
			status = netp::E_ECONNABORTED;
			goto _set_fail_and_return;
		}

		status = load_sockname();

		if (status != netp::OK ) {
			goto _set_fail_and_return;
		}

#ifdef _NETP_WIN
		//not sure linux os behaviour, to test
		if (0 == local_addr()->ipv4().u32 && m_family != (NETP_AF_USER/*FOR BFR*/) ) {
			status = netp::E_WSAENOTCONN;
			goto _set_fail_and_return;
		}
#endif

		NRP<netp::address> raddr;
		status = socket_getpeername_impl(raddr);
		if (status != netp::OK ) {
			status = netp_socket_get_last_errno();
			goto _set_fail_and_return;
		}
		if ( *raddr != *m_raddr) {
			status = netp::E_UNKNOWN;
			goto _set_fail_and_return;
		}

		if ( *(local_addr()) == *(remote_addr()) ) {
			status = netp::E_SOCKET_SELF_CONNCTED;
			NETP_WARN("[socket][%s]self connected", ch_info().c_str());
			goto _set_fail_and_return;
		}

		try {
			if (NETP_LIKELY(fn_initializer != nullptr)) {
				fn_initializer(NRP<channel>(this));
			}

			ch_io_end_connect();
			//@note: for handlers that have handshake protocol, connected evt happens after dialp->set(netp::OK)
			ch_set_connected();
			dialp->set(netp::OK);
		} catch(netp::exception const& e) {
			NETP_ASSERT(e.code() != netp::OK );
			status = e.code();
			NETP_ERR("[socket][%s]dial netp::exception: %d: %s", ch_info().c_str(), status, e.what());
			goto _set_fail_and_return;
		} catch(std::exception const& e) {
			status = netp_socket_get_last_errno();
			if (status == netp::OK) {
				status = netp::E_UNKNOWN;
			}
			NETP_ERR("[socket][%s]dial std::exception: %d: %s", ch_info().c_str(), status, e.what());
			goto _set_fail_and_return;
		} catch(...) {
			status = netp_socket_get_last_errno();
			if (status == netp::OK) {
				status = netp::E_UNKNOWN;
			}
			NETP_ERR("[socket][%s]dial unknown exception: %d", ch_info().c_str(), status);
			goto _set_fail_and_return;
		}

		_CH_FIRE_ACTION_CLOSE_AND_RETURN_IF_EXCEPTION(ch_fire_connected(), this, "ch_fire_connected");

		NETP_TRACE_SOCKET("[socket][%s]async connected", ch_info().c_str());
		//it's safe to close read in connected() callback
		ch_io_read();
	}

	void socket_channel::__do_io_accept_impl(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& listener_cfg, int status, io_ctx* ) {

		NETP_ASSERT(L->in_event_loop());
		/*ignore the left fds, cuz we're closed*/
		if (NETP_UNLIKELY( m_chflag&int(channel_flag::F_CLOSED)) ) { return; }

		NETP_ASSERT(fn_initializer != nullptr);
		while (status == netp::OK) {
			NRP<address> raddr;
			NRP<address> laddr;
			SOCKET nfd = socket_accept_impl( raddr,laddr);
			if (nfd == NETP_INVALID_SOCKET) {
				status = netp_socket_get_last_errno();
				_NETP_REFIX_EWOULDBLOCK(status);
				if (status == netp::E_EINTR) {
					status = netp::OK;
					continue;
				} else {
					break;
				}
			}
			
			NRP<event_loop> LL = netp::app::instance()->def_loop_group()->next();
			LL->execute([LL,fn_initializer,nfd, laddr, raddr, listener_cfg]() {
				NRP<socket_cfg> cfg_ = netp::make_ref<socket_cfg>();
				cfg_->fd = nfd;
				cfg_->family = listener_cfg->family;
				cfg_->type = listener_cfg->type;
				cfg_->proto = listener_cfg->proto;
				cfg_->laddr = laddr;
				cfg_->raddr = raddr;

				cfg_->L = LL;
				cfg_->option = listener_cfg->option;
				cfg_->kvals = listener_cfg->kvals;
				cfg_->sock_buf = listener_cfg->sock_buf;
				cfg_->tx_limit = listener_cfg->tx_limit;
				int rt;
				NRP<socket_channel> so;
				std::tie(rt, so) = create_socket_channel(cfg_);
				if (rt != netp::OK) {
#ifdef _NETP_WIN
					NETP_TRACE_SOCKET_OC("[socket][__do_io_accept_impl][netp::close]error: %d, nfd: %zu, laddr: %s, raddr: %s", rt, nfd, laddr->to_string().c_str(), raddr->to_string().c_str() );
#else
					NETP_TRACE_SOCKET_OC("[socket][__do_io_accept_impl][netp::close]error: %d, nfd: %u, laddr: %s, raddr: %s", rt, nfd, laddr->to_string().c_str(), raddr->to_string().c_str());
#endif
					netp::close(nfd);
					return;
				}
				NETP_ASSERT(so != nullptr);
				so->__do_accept_fire(fn_initializer);
			});
		}

		if (netp::E_EWOULDBLOCK==(status)) {
			//TODO: check the following errno
			//ENETDOWN, EPROTO,ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH, EOPNOTSUPP
			return;
		}

		if(status == netp::E_EMFILE ) {
			NETP_WARN("[socket][%s]accept error, EMFILE", ch_info().c_str() );
			return ;
		}

		NETP_ERR("[socket][%s]accept error: %d", ch_info().c_str(), status);
		ch_errno()=(status);
		m_chflag |= int(channel_flag::F_READ_ERROR);
		ch_close_impl(nullptr);
	}

	void socket_channel::__do_io_read_from(int status, io_ctx* ) {
		NETP_ASSERT(m_protocol == u8_t(NETP_PROTOCOL_UDP));
		while (status == netp::OK) {
			NETP_ASSERT((m_chflag & (int(channel_flag::F_READ_SHUTDOWNING))) == 0);
			/*@NOTE: ch_fire_read might result in F_WATCH_READ BE RESET*/

			if (NETP_UNLIKELY( int(channel_flag::F_WATCH_READ) !=(m_chflag & (int(channel_flag::F_WATCH_READ)|int(channel_flag::F_READ_SHUTDOWN) | int(channel_flag::F_CLOSE_PENDING)/*ignore the left read buffer, cuz we're closing it*/) )))
			{ return; }
			NRP<netp::address> __address_nonnullptr_ = netp::make_ref<netp::address>();
			NRP<netp::packet>& loop_buf = L->channel_rcv_buf();
			const int nbytes = socket_recvfrom_impl(loop_buf->head(), loop_buf->left_right_capacity(), __address_nonnullptr_);
			if (NETP_UNLIKELY(nbytes<0)) {
				status = nbytes;
				break;
			}
			loop_buf->incre_write_idx(nbytes);
			NRP<netp::packet> __tmp =netp::make_ref<netp::packet>(L->channel_rcv_buf_size());
			__tmp.swap(loop_buf);
			channel::ch_fire_readfrom(std::move(__tmp), __address_nonnullptr_);
		}
		___do_io_read_done(status);
	}

	void socket_channel::__do_io_read(int status, io_ctx* ioctx) {
		//NETP_INFO("READ IN");
#ifdef _NETP_DEBUG
		NETP_ASSERT(!ch_is_listener());
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(L->channel_rcv_buf()->len() == 0 && L->channel_rcv_buf()->left_right_capacity() == L->channel_rcv_buf_size());
#endif

		//in case socket object be destructed during ch_read

		const int size = L->channel_rcv_buf_size();
		int nbytes = size; //trick to skip the frist check
		//refer to https://man7.org/linux/man-pages/man7/epoll.7.html tip 9
		//if it is stream based, return value nbytes<size indicate that the buf has been exhausted
		//socket_recv_impl set status to non-zero iff ::recv return -1
		while ( (status == netp::OK) && ( (nbytes==size)|| !is_stream()) ) {
			NETP_ASSERT( (m_chflag&(int(channel_flag::F_READ_SHUTDOWNING))) == 0);

			/*@NOTE: ch_fire_read might result in F_WATCH_READ BE RESET*/
			if (NETP_UNLIKELY(int(channel_flag::F_WATCH_READ) != (m_chflag & (int(channel_flag::F_WATCH_READ)|int(channel_flag::F_READ_SHUTDOWN)|int(channel_flag::F_READ_ERROR) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING)/*ignore the left read buffer, cuz we're closing it*/)) ))
			{ return; }
			
			NRP<netp::packet>& loop_buf = L->channel_rcv_buf();
			nbytes = socket_recv_impl(loop_buf->head(), size);
			if (NETP_UNLIKELY(nbytes < 0)) {
				status = nbytes;
				break;
			} else if (nbytes == 0 ) {
				if (is_tcp()) {
					status = netp::E_SOCKET_GRACE_CLOSE;
					break;
				} else if (!is_udp()) {
					status = netp::E_UNKNOWN;
					break;
				}
			}

			//@note: udp socket might receive a 0 len pkt
			loop_buf->incre_write_idx(nbytes);
			NRP<netp::packet> __tmp = netp::make_ref<netp::packet>(size);
			__tmp.swap(loop_buf);
			channel::ch_fire_read(std::move(__tmp));
		}

		//for epoll et, (nbytes<size && rdhub is set)
#if defined(NETP_DEFAULT_POLLER_TYPE_IS_EPOLL)
		if ( (ioctx->flag&io_flag::IO_READ_HUP) && (status == netp::OK) ) {
			status = netp::E_SOCKET_GRACE_CLOSE;
		}
#else
		(void)ioctx;
#endif
		___do_io_read_done(status);
	}

	//we always do ch_io_end for close_write_impl, so if we're here, there must be no (WRITE_ERROR|F_WRITE_SHUTDOWN)
	void socket_channel::__do_io_write( int status, io_ctx*) {
		NETP_ASSERT( (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_TX_LIMIT)|int(channel_flag::F_CLOSING) | int(channel_flag::F_WRITE_ERROR) |int(channel_flag::F_WRITE_SHUTDOWN) )) == 0 );
		//NETP_TRACE_SOCKET("[socket][%s]__do_io_write, write begin: %d, flag: %u", ch_info().c_str(), status , m_chflag );
		if (status == netp::OK) {
			status = socket_channel::___do_io_write();
		}
		__do_io_write_done(status);
	}

	//we always do ch_io_end for close_write_impl, so if we're here, there must be no (WRITE_ERROR|F_WRITE_SHUTDOWN)
	void socket_channel::__do_io_write_to(int status, io_ctx*) {
#ifdef _NETP_DEBUG
		NETP_ASSERT(is_udp());
#endif
		NETP_ASSERT((m_chflag & (int(channel_flag::F_WRITE_SHUTDOWNING) | int(channel_flag::F_TX_LIMIT) | int(channel_flag::F_CLOSING) | int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_WRITE_SHUTDOWN))) == 0);
		//NETP_TRACE_SOCKET("[socket][%s]__do_io_write, write begin: %d, flag: %u", ch_info().c_str(), status , m_chflag );
		if (status == netp::OK) {
			status = socket_channel::___do_io_write_to();
		}
		__do_io_write_done(status);
	}

	//write until error
	//<0, is_error == (errno != E_CHANNEL_WRITING)
	//==0, write done
	//this api would be called right after a check of writeable of the current socket
	int socket_channel::___do_io_write() {

#ifdef _NETP_DEBUG
		NETP_ASSERT( ch_is_connected() && m_tx_entry_q.size(), "%s, flag: %u", ch_info().c_str(), m_chflag);
#endif

		NETP_ASSERT( m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) );
		NETP_ASSERT( (m_chflag&int(channel_flag::F_TX_LIMIT)) ==0);

		//there might be a chance to be blocked a while in this loop, if set trigger another write
		while ( m_tx_entry_q.size() ) {
#ifdef _NETP_DEBUG
			NETP_ASSERT( is_udp() ? true: (m_tx_bytes) > 0 );
#endif
			socket_outbound_entry& entry = m_tx_entry_q.front();
			const u32_t dlen = (entry.data->len());
			u32_t wlen = (dlen-entry.written);
			if (m_tx_limit !=0 && (m_tx_budget<(wlen))) {
				if ( (m_tx_budget == 0) || is_udp()/*udp pkt could not be split into smaller pkt*/ ) {
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_chflag&int(channel_flag::F_TX_LIMIT_TIMER));
#endif
					//@note: for a tx_limit less than 64k/s, writing a 64k udp pkt would always be failed
					return netp::E_CHANNEL_TXLIMIT;
				}
				wlen = m_tx_budget;
			}

			const int nbytes = socket_send_impl( (entry.data->head()+entry.written), (wlen));
			if (NETP_UNLIKELY(nbytes < 0)) {
				return nbytes;
			}

			if ( is_udp() && (nbytes != dlen)) {
				//ignore the last write && retry
				continue;
			}

			m_tx_bytes -= nbytes;
			if (m_tx_limit != 0 ) {
				m_tx_budget -= nbytes;
				u32_t __tx_limit_clock_ms = netp::app::instance()->channel_tx_limit_clock();
				if (!(m_chflag & int(channel_flag::F_TX_LIMIT_TIMER)) && ( (m_tx_budget < ((m_tx_limit/(1000/__tx_limit_clock_ms))) ) ) ) {
					m_chflag |= int(channel_flag::F_TX_LIMIT_TIMER);
					m_tx_limit_last_tp = netp::now<netp::microseconds_duration_t, netp::steady_clock_t>().time_since_epoch().count();
					L->launch(netp::make_ref<netp::timer>(std::chrono::milliseconds(__tx_limit_clock_ms), &socket_channel::_tmcb_tx_limit, NRP<socket_channel>(this), std::placeholders::_1));
				}
			}

			entry.written += nbytes;
			if ((entry.written == dlen)) {
				entry.write_promise->set(netp::OK);
				m_tx_entry_q.pop_front();
			} else {
				NETP_ASSERT(!is_udp(), "proto: %u", sock_protocol() );
			}
		}
		return netp::OK;
	}

	int socket_channel::___do_io_write_to() {
#ifdef _NETP_DEBUG
		NETP_ASSERT( !ch_is_connected() && m_tx_entry_to_q.size() && m_tx_entry_q.empty(), "%s, flag: %u", ch_info().c_str(), m_chflag);
#endif
		NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)));

		//there might be a chance to be blocked a while in this loop, if set trigger another write
		int status = netp::OK;
		while ( m_tx_entry_to_q.size() ) {
			//@note: udp allow zero-len pkt
			socket_outbound_entry_to& entry = m_tx_entry_to_q.front();
			NETP_ASSERT((entry.data->len() <= m_tx_bytes));
			status = socket_sendto_impl(entry.data->head(), (u32_t)entry.data->len(), entry.to);
			if(status < 0) {
				return status;
			}
			m_tx_bytes -= u32_t(entry.data->len());
			entry.write_promise->set(netp::OK);
			m_tx_entry_to_q.pop_front();
		}
		return netp::OK;
	}

	void socket_channel::_ch_do_close_listener() {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_chflag & int(channel_flag::F_LISTENING));
		NETP_ASSERT((m_chflag & int(channel_flag::F_CLOSED)) ==0 );

		m_chflag |= int(channel_flag::F_CLOSED);
		ch_io_end_accept();
		ch_io_end();
		NETP_TRACE_SOCKET("[socket][%s]ch_do_close_listener end", ch_info().c_str());
	}

	inline void socket_channel::_ch_do_close_read_write() {
		NETP_ASSERT(L->in_event_loop());

		if (m_chflag & (int(channel_flag::F_CLOSING) | int(channel_flag::F_CLOSED))) {
			return;
		}

		//NETP_ASSERT((m_chflag & int(channel_flag::F_CLOSED)) == 0);
		m_chflag |= int(channel_flag::F_CLOSING);
		m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING));
		NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);

		_ch_do_close_read();
		_ch_do_close_write();

		NETP_ASSERT(m_tx_entry_q.empty() && m_tx_entry_to_q.empty() );
		NETP_ASSERT(m_tx_bytes == 0);

		//close read, close write might result in F_CLOSED
		m_chflag &= ~(int(channel_flag::F_CLOSING));
		ch_rdwr_shutdown_check();
	}

	void socket_channel::ch_close_read_impl(NRP<promise<int>> const& closep) {
		NETP_ASSERT(L->in_event_loop());
		NETP_TRACE_SOCKET("[socket][%s]ch_close_read_impl, _ch_do_close_read, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
		int prt = netp::OK;
		if ((m_chflag & int(channel_flag::F_READ_SHUTDOWN)) != 0) {
			prt = (netp::E_CHANNEL_READ_CLOSED);
		} else if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING) | int(channel_flag::F_CLOSING))) {
			prt = (netp::E_OP_INPROCESS);
		} else {
			_ch_do_close_read();
		}
		if (closep) { closep->set(prt); }
	}

	//if there is a write_error, we'll always do ch_close_impl(nullptr), ch_close_impl(null) might result in ch_close_read(), ch_close_read might result in ch_close_impl again
	//we would get to F_WRITE_SHUDOWN state in this exection(ch_close_impl) eventually
	//if there is no erorr, just pending shutdown
	void socket_channel::ch_close_write_impl(NRP<promise<int>> const& closep) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(!ch_is_listener());
		int prt = netp::OK;
		if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN)) {
			prt = (netp::E_CHANNEL_WRITE_CLOSED);
		} else if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_CLOSING)) ) {
			prt = (netp::E_OP_INPROCESS);
		} else if (m_chflag & (int(channel_flag::F_READ_ERROR) | int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_FIRE_ACT_EXCEPTION))) {
			goto __act_label_close_write;
		} else if (m_chflag&(int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_WRITE_SHUTDOWN_PENDING))) {
			//if we have a write_error, a immediate ch_close_impl would be take out
			NETP_ASSERT((m_chflag&int(channel_flag::F_WRITE_ERROR)) == 0);
			NETP_ASSERT(m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_TX_LIMIT)) );
			NETP_ASSERT((m_fn_write == nullptr) ? (m_tx_entry_q.size()||m_tx_entry_to_q.size()): true, "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno);
			prt = (netp::E_CHANNEL_WRITE_SHUTDOWNING);
		} else if (m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_TX_LIMIT)) ) {
			//write set ok might result in ch_close_write|ch_close
			//the pending action would be scheduled right in _do_write_done() which is right after every _io_do_write action
			//if a user defined write function is used, user have to take care of it by user self
			NETP_ASSERT((m_fn_write==nullptr) ? (m_tx_entry_q.size()||m_tx_entry_to_q.size()): true, "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno );
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
			prt = (netp::E_CHANNEL_WRITE_SHUTDOWNING);
		} else {
		__act_label_close_write:
			NETP_ASSERT(((m_chflag&(int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED) )) == (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_USE_DEFAULT_WRITE))) ?
				(m_tx_entry_q.size()||m_tx_entry_to_q.size()):
				true, "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno);
			_ch_do_close_write();
		}

		if (closep) { closep->set(prt); }
	}

	//ERROR FIRST
	void socket_channel::ch_close_impl(NRP<promise<int>> const& closep) {
		NETP_ASSERT(L->in_event_loop());

		int prt = netp::OK;
		if (m_chflag&int(channel_flag::F_CLOSED)) {
			prt = (netp::E_CHANNEL_CLOSED);
		} else if (m_chflag&int(channel_flag::F_CLOSING)) {
			prt = (netp::E_OP_INPROCESS);
		} else if (ch_is_listener()) {
			_ch_do_close_listener();
		} else if (m_chflag & (int(channel_flag::F_READ_ERROR) | int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_FIRE_ACT_EXCEPTION))) {
			NETP_ASSERT( ch_errno() != netp::OK );
			NETP_ASSERT(((m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_USE_DEFAULT_WRITE))) == (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_USE_DEFAULT_WRITE))) ?
				(m_tx_entry_q.size()||m_tx_entry_to_q.size()):
				true, "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno);

			goto __act_label_close_read_write;
		} else if ( m_chflag & (int(channel_flag::F_CLOSE_PENDING)| int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) ) {
			NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER) | int(channel_flag::F_WATCH_WRITE) | int(channel_flag::F_TX_LIMIT)));
			NETP_ASSERT(m_chflag&(int(channel_flag::F_USE_DEFAULT_WRITE)) ? (m_tx_entry_q.size()||m_tx_entry_to_q.size()) : true, "[#%s]chflag: %d, cherrno: %d", ch_info().c_str(), m_chflag, m_cherrno);
			prt = (netp::E_OP_INPROCESS);
		} else if (m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_TX_LIMIT)) ) {
			//wait for write done event, we might in a write barrier
			//for a non-error close, do grace shutdown
			//for error close, we would not reach here
			NETP_ASSERT((m_fn_write == nullptr) ? (m_tx_entry_q.size()||m_tx_entry_to_q.size()) : true, "[#%s]chflag: %d, cherrno: %d", ch_info().c_str(),  m_chflag, m_cherrno);
			m_chflag |= int(channel_flag::F_CLOSE_PENDING);
			prt = (netp::E_CHANNEL_CLOSING);
		} else {
__act_label_close_read_write:
			_ch_do_close_read_write();
		}

		if (closep) { closep->set(prt); }
	}

#define __CH_WRITEABLE_CHECK__( outlet, chp) \
		if (m_chflag&(int(channel_flag::F_READ_ERROR)|int(channel_flag::F_WRITE_ERROR)|int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_SHUTDOWN_PENDING)|int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_CLOSING) ) ) { \
			chp->set(netp::E_CHANNEL_WRITE_ABORT); \
			return ; \
		} \
		const u32_t outlet_len = (u32_t)outlet->len(); \
		/*set the threshold arbitrarily high, the writer have to check the return value if */ \
		if ( (m_tx_bytes>0) && ( (m_tx_bytes + outlet_len) > m_snd_buf_size) ) { \
			NETP_ASSERT(m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE))); \
			chp->set(netp::E_CHANNEL_WRITE_BLOCK); \
			return; \
		} \

	void socket_channel::ch_write_impl(NRP<promise<int>> const& intp, NRP<packet> const& outlet)
	{
#ifdef _NETP_DEBUG
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT((intp != nullptr) && ( is_udp() ? true: (outlet->len() > 0)) );
		NETP_ASSERT(m_snd_buf_size>0);
#endif

		__CH_WRITEABLE_CHECK__(outlet, intp);

#ifdef _NETP_DEBUG
			NETP_ASSERT(ch_is_connected(),"socket[%s]flag: %u", ch_info().c_str(), m_chflag );
			NETP_ASSERT((m_chflag & (int(channel_flag::F_WATCH_WRITE) | int(channel_flag::F_TX_LIMIT))) ? m_tx_entry_q.size() : true, "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno);
#endif

		m_tx_entry_q.push_back({
			0,
			outlet,
			intp
		});
		m_tx_bytes += outlet_len;

		if (m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_TX_LIMIT))) {
			return;
		}

#ifdef NETP_ENABLE_FAST_WRITE
		//fast write
		m_chflag |= int(channel_flag::F_WRITE_BARRIER);
		__do_io_write(netp::OK, m_io_ctx);
		m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
#else
		ch_io_write();
#endif
	}

	//@note: udp could send zero-len pkt
	void socket_channel::ch_write_to_impl( NRP<promise<int>> const& intp, NRP<packet> const& outlet,NRP<netp::address >const& to) {
#ifdef _NETP_DEBUG
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(intp != nullptr);
#endif

		__CH_WRITEABLE_CHECK__(outlet, intp);

#ifdef _NETP_DEBUG
			NETP_ASSERT(!ch_is_connected(), "socket[%s]flag: %u", ch_info().c_str(), m_chflag);
#endif

		m_tx_entry_to_q.push_back({
			outlet,
			to,
			intp
		});
		m_tx_bytes += outlet_len;

		if (m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) ) {
			return;
		}

#ifdef NETP_ENABLE_FAST_WRITE
		//fast write
		m_chflag |= int(channel_flag::F_WRITE_BARRIER);
		__do_io_write_to(netp::OK,m_io_ctx);
		m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
#else
		ch_io_write();
#endif
	}

	//@NOTE:
	//if we have a socket that has pending outlet data in the pipe, and the remote peer do not read data from pipe from this point of time, the writing socket would be in established state with pending data in snd buffer
	//reproduce steps as below:
	//1, write data via fd (chrome download a file)
	//2, chrome pause download process
	//3, writing peer's snd buffer reach full eventually, then the writing fd will be added to poll list for a write evetn
	//4, as the chrome do not read data anymore, thus the writing peer will never get a chance to clear the snd buffer,

	void socket_channel::io_notify_terminating(int status, io_ctx* ctx_) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(status == netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING);
		//terminating notify, treat as a error
		NETP_ASSERT(m_chflag&int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE));
		m_chflag |= (int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING));
		
		//notify terminating is not a error
		//m_cherrno = netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING;
		if (m_chflag&int(channel_flag::F_CONNECTING)) {
			//cancel connect
			m_chflag |= (int(channel_flag::F_READ_ERROR) | int(channel_flag::F_WRITE_ERROR));
			m_cherrno = netp::E_CHANNEL_ABORT;
			__ch_io_cancel_connect(status, ctx_);
		}
		ch_close_impl(nullptr);
	}

	void socket_channel::io_notify_read(int status, io_ctx* ctx) {
		NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_READ), "[socket][%s]", ch_info().c_str() );
		if (m_chflag & int(channel_flag::F_USE_DEFAULT_READ)) {
			ch_is_connected() ? __do_io_read(status, ctx) : __do_io_read_from(status, ctx);
			return;
		}
		NETP_ASSERT( m_fn_read != nullptr );
		(*m_fn_read)(status, ctx);
	}

	void socket_channel::io_notify_write(int status, io_ctx* ctx) {
		NETP_ASSERT( m_chflag&int(channel_flag::F_WATCH_WRITE), "[socket][%s]", ch_info().c_str() );
		if (m_chflag & int(channel_flag::F_USE_DEFAULT_WRITE)) {
			m_chflag |= int(channel_flag::F_WRITE_BARRIER);
			ch_is_connected() ? __do_io_write(status, ctx) : __do_io_write_to(status, ctx);
			m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
			return;
		}
		NETP_ASSERT(m_fn_write != nullptr);
		(*m_fn_write)(status, ctx);
	}

	void socket_channel::__ch_clean() {
		NETP_ASSERT(m_fn_read == nullptr && m_fn_write == nullptr);
		ch_deinit();
		if (m_chflag & int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE)) {
			L->io_end(m_io_ctx);
		}
	}

	void socket_channel::ch_io_begin(fn_io_event_t const& fn_begin_done) {
		NETP_ASSERT(is_nonblocking());

		if (!L->in_event_loop()) {
			L->schedule([s = NRP<socket_channel>(this), fn_begin_done]() {
				s->ch_io_begin(fn_begin_done);
			});
			return;
		}

		NETP_ASSERT((m_chflag & (int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE))) == 0);

		m_io_ctx = L->io_begin(m_fd, NRP<io_monitor>(this));
		if (m_io_ctx == 0) {
			m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
			ch_errno() = netp::E_IO_BEGIN_FAILED;
			ch_close(nullptr);
			fn_begin_done(netp::E_IO_BEGIN_FAILED, 0);
			return;
		}
		__io_begin_done(m_io_ctx);
		fn_begin_done(netp::OK, m_io_ctx);
	}

		void socket_channel::ch_io_end() {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_tx_bytes == 0 && m_tx_entry_q.empty() && m_tx_entry_to_q.empty(), "[#%s]flag: %d, errno: %d, tx_bytes: %u, tx_entry_q.size(): %u, tx_entry_q_to.size(): %u", ch_info().c_str(), m_chflag, m_cherrno, m_tx_bytes, m_tx_entry_q.size(), m_tx_entry_to_q.size());
			NETP_ASSERT((m_chflag & (int(channel_flag::F_WATCH_READ) | int(channel_flag::F_WATCH_WRITE) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_CLOSED))) == int(channel_flag::F_CLOSED));
			NETP_TRACE_SOCKET("[socket][%s]io_action::END, flag: %d", ch_info().c_str(), m_chflag);

			//no more read|write event be watched by poller any more at this point, it's safe to do close
			ch_fire_closed(close());
			//@note: trick
			//delay one tick to hold this channel's ref
			//in case we're in handler's read/write/close call, if we remove this iom from iom list immediately, we might get memory issue
			L->schedule([so = NRP<socket_channel>(this)]() {
				so->__ch_clean();
			});
		}

		//we can override this function to enable custom accept way
		void socket_channel::ch_io_accept(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& listener_cfg, fn_io_event_t const& fn) {
			if (fn != nullptr) {
				ch_io_read(fn);
				(void)fn_initializer;
				(void)listener_cfg;
				return;
			}
			//posix impl
			ch_io_read(std::bind(&socket_channel::__do_io_accept_impl, NRP<socket_channel>(this), fn_initializer, listener_cfg, std::placeholders::_1, std::placeholders::_2));
		}

		void socket_channel::ch_io_read(fn_io_event_t const& fn_read) {
			//it's safe to call close read in an connected() callback

			if (!L->in_event_loop()) {
				L->schedule([s = NRP<socket_channel>(this), fn_read]()->void {
					s->ch_io_read(fn_read);
				});
				return;
			}
			NETP_ASSERT((m_chflag & int(channel_flag::F_READ_SHUTDOWNING)) == 0);
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s]io_action::READ, ignore, flag: %d", ch_info().c_str(), m_chflag);
				return;
			}

			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_READ)) == 0);
				if (fn_read != nullptr) fn_read(netp::E_CHANNEL_READ_CLOSED, nullptr);
				return;
			}
			int rt = L->io_do(io_action::READ, m_io_ctx);
			if (NETP_UNLIKELY(rt != netp::OK)) {
				NETP_ERR("[socket][%s]io_do(io_action::READ), rt: %d", ch_info().c_str(), rt);
				m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
				ch_errno() = rt;
				ch_close(nullptr);
				if (fn_read != nullptr) { fn_read(rt, nullptr); }
				return;
			}
			if (fn_read == nullptr) {
				m_chflag |= (int(channel_flag::F_USE_DEFAULT_READ)|int(channel_flag::F_WATCH_READ));
			} else {
				m_chflag &= ~int(channel_flag::F_USE_DEFAULT_READ);
				m_chflag |= int(channel_flag::F_WATCH_READ);
				m_fn_read = netp::allocator<fn_io_event_t>::make(fn_read);
			}
			NETP_TRACE_IOE("[socket][%s]io_action::READ", ch_info().c_str());
		}

		void socket_channel::ch_io_end_read() {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this)]()->void {
					_so->ch_io_end_read();
				});
				return;
			}

			if ((m_chflag & int(channel_flag::F_WATCH_READ))) {
				int rt = L->io_do(io_action::END_READ, m_io_ctx);
				m_chflag &= ~(int(channel_flag::F_USE_DEFAULT_READ) | int(channel_flag::F_WATCH_READ));
				netp::allocator<fn_io_event_t>::trash(m_fn_read);
				m_fn_read = nullptr;
				NETP_TRACE_IOE("[socket][%s]io_action::END_READ", ch_info().c_str());

				if (rt != netp::OK) {
					NETP_WARN("[socket][%s]io_action::END_READ, rt: %d, close socket_channel", ch_info().c_str(), rt );
					ch_errno() = rt;
					ch_close_impl(nullptr);
				}
			}
		}

		void socket_channel::ch_io_write(fn_io_event_t const& fn_write ) {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this), fn_write]()->void {
					_so->ch_io_write(fn_write);
				});
				return;
			}

			if (m_chflag&int(channel_flag::F_WATCH_WRITE)) {
				//for udp socket, it might be in non-connected state
				if (fn_write != nullptr) {
					fn_write(netp::E_SOCKET_OP_ALREADY, 0);
				}
				return;
			}

			if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_WRITE)) == 0);
				NETP_TRACE_SOCKET("[socket][%s]io_action::WRITE, cancel for wr closed already", ch_info().c_str());
				if (fn_write != nullptr) {
					fn_write(netp::E_CHANNEL_WRITE_CLOSED, 0);
				}
				return;
			}

			int rt = L->io_do(io_action::WRITE, m_io_ctx);
			if (NETP_UNLIKELY(rt != netp::OK)) {
				NETP_ERR("[socket][%s]io_do(io_action::WRITE), rt: %d", ch_info().c_str(),rt );
				m_chflag |= int(channel_flag::F_WRITE_ERROR);//for assert check
				ch_errno() = rt;
				ch_close(nullptr);
				if (fn_write != nullptr) { fn_write(rt, nullptr); }
				return;
			}
			if (fn_write == nullptr) {
				m_chflag |= (int(channel_flag::F_USE_DEFAULT_WRITE) | int(channel_flag::F_WATCH_WRITE));
			} else {
				m_chflag &= ~int(channel_flag::F_USE_DEFAULT_WRITE);
				m_chflag |= int(channel_flag::F_WATCH_WRITE);
				m_fn_write = netp::allocator<fn_io_event_t>::make(fn_write);
			}
			NETP_TRACE_IOE("[socket][%s]io_action::WRITE", ch_info().c_str());
		}

		void socket_channel::ch_io_end_write() {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this)]()->void {
					_so->ch_io_end_write();
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				int rt = L->io_do(io_action::END_WRITE, m_io_ctx);
				m_chflag &= ~(int(channel_flag::F_USE_DEFAULT_WRITE) | int(channel_flag::F_WATCH_WRITE));
				netp::allocator<fn_io_event_t>::trash(m_fn_write);
				m_fn_write = nullptr;
				NETP_TRACE_IOE("[socket][%s]io_action::END_WRITE, rt: %d", ch_info().c_str(), rt );

				if (rt != netp::OK) {
					NETP_WARN("[socket][%s]io_action::END_WRITE, rt: %d, close socket_channel", ch_info().c_str(), rt);
					ch_errno() = rt;
					ch_close_impl(nullptr);
				}
			}
		}

		//@note: if u wanna to have independent read&write thread, use dup with a different L
		//pay attention to r&w concurrent issue
		//please do ch_io_begin by manual

		NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> socket_channel::dup(NRP<event_loop> const& LL) {
			NETP_ASSERT( (L->poller_type() == LL->poller_type()) && (LL->poller_type() == NETP_DEFAULT_POLLER_TYPE) );

			NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> p =
				netp::make_ref<netp::promise<std::tuple<int, NRP<socket_channel>>>>();

			int __errno = netp::OK;
			NRP<socket_cfg> _cfg = netp::make_ref<socket_cfg>();

#ifdef _NETP_WIN
			WSAPROTOCOL_INFO proto_info;
#endif

			if (sock_family() == NETP_AF_USER) {
				__errno = netp::E_OP_NOT_SUPPORTED;
				goto __exit_with_errno;
			}

			if (m_chflag&int(channel_flag::F_CLOSED)) {
				__errno = netp::E_EBADF;
				goto __exit_with_errno;
			}

#ifdef _NETP_WIN
			__errno = WSADuplicateSocket(ch_id(), GetCurrentProcessId(), &proto_info);
			if (__errno != 0) {
				__errno = netp_socket_get_last_errno();
				goto __exit_with_errno;
			}

			_cfg->fd = WSASocket(sock_family(), sock_type(), NETP_PROTO_MAP_OS_PROTO[sock_protocol()], &proto_info, 0, WSA_FLAG_OVERLAPPED);
#else
			_cfg->fd = ::dup(ch_id());
#endif

			if (_cfg->fd == NETP_INVALID_SOCKET) {
				__errno = netp_socket_get_last_errno();
				goto __exit_with_errno;
			}

#ifdef _NETP_DEBUG
			channel_buf_cfg __src_ch_buf;
			__src_ch_buf.rcvbuf_size = get_rcv_buffer_size();
			__src_ch_buf.sndbuf_size = get_snd_buffer_size();
#endif

			_cfg->L = LL;
			//friend access
			_cfg->family = m_family;
			_cfg->type = m_type;
			_cfg->proto = m_protocol;
			_cfg->laddr = m_laddr;
			_cfg->raddr = m_raddr;
			_cfg->tx_limit = m_tx_limit;

#ifdef _NETP_DEBUG
			LL->execute([p, option = m_option, _cfg, __src_ch_buf]() {
#else
			LL->execute([p, option=m_option, _cfg]() {
#endif
				NRP<socket_channel> _dupch = default_socket_channel_maker(_cfg);
				NETP_ASSERT(_dupch != nullptr);
				int __errno = _dupch->ch_init(0, { 0,0,0 }, { 0,0 });
				if (__errno != netp::OK) {
					_dupch->ch_close();//just double confirm
					goto ____exit_with_errno;
				}

#ifdef _NETP_DEBUG
				channel_buf_cfg __dup_ch_buf;
				__dup_ch_buf.rcvbuf_size = _dupch->get_rcv_buffer_size();
				__dup_ch_buf.sndbuf_size = _dupch->get_snd_buffer_size();
				NETP_ASSERT(__dup_ch_buf.rcvbuf_size == __src_ch_buf.rcvbuf_size);
				NETP_ASSERT(__dup_ch_buf.sndbuf_size == __src_ch_buf.sndbuf_size);
#endif

				_dupch->m_option = option;

				NETP_ASSERT(_dupch->ch_errno() == netp::OK);
				p->set(std::make_tuple(netp::OK, _dupch));
				return;

		____exit_with_errno :
				if (__errno == netp::OK) { __errno = netp::E_UNKNOWN; }
				p->set(std::make_tuple(__errno, nullptr));
			});
			return p;

		__exit_with_errno:
			if (__errno == netp::OK) { __errno = netp::E_UNKNOWN; }
			p->set(std::make_tuple(__errno, nullptr));
			return p;
		}

} //end of ns