#include <netp/core.hpp>
#include <netp/logger_broker.hpp>
#include <netp/socket_channel.hpp>

namespace netp {

	int socket_channel::open() {
		NETP_ASSERT(m_fd == NETP_INVALID_SOCKET);
		int rt = socket_open_impl();
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
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
		NETP_ASSERT(!m_laddr||m_laddr->is_null());
		NETP_ASSERT((m_family) == addr->family());
		int rt = socket_bind_impl(addr);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		m_laddr = addr;
		return netp::OK;
	}

	int socket_channel::bind_any() {
		NRP<address >_any_ = netp::make_ref<address>();
		NETP_ASSERT(m_family != NETP_AF_UNSPEC);
		_any_->setfamily(m_family);
		int rt = bind(_any_);
		if (rt != netp::OK) {
			return rt;
		}
		rt = load_sockname();
		NETP_TRACE_SOCKET("[socket][%s]socket bind rt: %d", ch_info().c_str(), rt);
		return rt;
	}

	int socket_channel::connect(NRP<address> const& addr) {
		if (m_chflag & (int(channel_flag::F_CONNECTING) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_LISTENING) | int(channel_flag::F_CLOSED)) ) {
			return netp::E_SOCKET_INVALID_STATE;
		}
		NETP_ASSERT((m_chflag & int(channel_flag::F_ACTIVE)) == 0);
		m_raddr = addr;
		channel::ch_set_active();
		int rt= socket_connect_impl(addr);
		if (rt == netp::OK) {
			m_chflag |= int(channel_flag::F_CONNECTED);
			return netp::OK;
		}

		rt = netp_socket_get_last_errno();
		if (netp::E_EINPROGRESS==(rt)) {
			m_chflag |= int(channel_flag::F_CONNECTING);
		} else {
			ch_errno() = rt;
		}
		return rt;
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
		NETP_TRACE_SOCKET("[socket][%s]socket listen rt: %d", ch_info().c_str(), rt);
		return rt;
	}

	int socket_channel::set_snd_buffer_size(u32_t size) {
		NETP_ASSERT(m_fd > 0);

		if (size == 0) {
			return netp::OK;
		} else if (size < u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE);
		} else if (size > u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE);
		}

		int rt;
#ifdef _DEBUG
		rt = get_snd_buffer_size();
		NETP_TRACE_SOCKET("[socket_base][#%d]snd buffer size: %u, try to set: %u", m_fd, rt, size);
#endif

		rt = socket_setsockopt_impl(SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size));
		if (rt == NETP_SOCKET_ERROR) {
			return netp_socket_get_last_errno();
		}
		return netp::OK;
	}

	int socket_channel::get_snd_buffer_size() const {
		NETP_ASSERT(m_fd > 0);
		int size;
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

		if (size == 0) {
			return netp::OK;
		} else if (size < u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE);
		} else if (size > u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE);
		}

		int rt;
#ifdef _DEBUG
		rt = get_rcv_buffer_size();
		NETP_TRACE_SOCKET("[socket_base][#%d]rcv buffer size: %u, try to set: %u", m_fd, rt, size);
#endif

		rt = socket_setsockopt_impl(SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size));
		if (rt == NETP_SOCKET_ERROR) {
			return netp_socket_get_last_errno();
		}
		return netp::OK;
	}

	int socket_channel::get_rcv_buffer_size() const {
		NETP_ASSERT(m_fd > 0);
		int size;
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
		socklen_t length;

		int rt = socket_getsockopt_impl(IPPROTO_IP, IP_TOS, (char*)&_tos, &length);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		tos = IPTOS_TOS(_tos);
		return netp::OK;
	}

	int socket_channel::set_tos(u8_t tos) {
		NETP_ASSERT(m_fd > 0);
		u8_t _tos = IPTOS_TOS(tos) | 0xe0;
		int rt = socket_setsockopt_impl(IPPROTO_IP, IP_TOS, (char*)&_tos, sizeof(_tos));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	void socket_channel::_tmcb_BDL(NRP<timer> const& t) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_outbound_limit > 0);
		NETP_ASSERT(m_chflag&int(channel_flag::F_BDLIMIT_TIMER) );
		m_chflag &= ~int(channel_flag::F_BDLIMIT_TIMER);
		if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_ERROR)|int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING))) {
			return;
		}

		NETP_ASSERT(m_outbound_limit>=m_outbound_budget);
		u32_t tokens = m_outbound_limit / (1000/NETP_SOCKET_BDLIMIT_TIMER_DELAY_DUR);
		if ( m_outbound_limit < (tokens+ m_outbound_budget)) {
			m_outbound_budget = m_outbound_limit;
		} else {
			m_chflag |= int(channel_flag::F_BDLIMIT_TIMER);
			m_outbound_budget += tokens;
			L->launch(t);
		}

		if (m_chflag & int(channel_flag::F_BDLIMIT)) {
			NETP_ASSERT( !(m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE))));
			m_chflag &= ~int(channel_flag::F_BDLIMIT);
			m_chflag |= int(channel_flag::F_WRITE_BARRIER);
			__do_io_write(netp::OK, m_io_ctx);
			m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
		}
	}

	void socket_channel::do_listen_on(NRP<promise<int>> const& intp, NRP<address> const& addr, fn_channel_initializer_t const& fn_accepted_initializer, NRP<socket_cfg> const& ccfg, int backlog ) {
		if (!L->in_event_loop()) {
			L->schedule([_this=NRP<socket_channel>(this), addr, fn_accepted_initializer, intp,ccfg, backlog]() ->void {
				_this->do_listen_on(intp, addr, fn_accepted_initializer, ccfg, backlog);
			});
			return;
		}

		//int rt = -10043;
		int rt = socket_channel::bind(addr);
		if (rt != netp::OK) {
			m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
			NETP_WARN("[socket]socket::bind(): %d, addr: %s", rt, addr->to_string().c_str() );
			intp->set(rt);
			return;
		}

		rt = socket_channel::listen(backlog);
		if (rt != netp::OK) {
			m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
			NETP_WARN("[socket]socket::listen(%u): %d, addr: %s",backlog, rt, addr->to_string().c_str());
			intp->set(rt);
			return;
		}

		NETP_ASSERT(rt == netp::OK);
		ccfg->family = m_family;
		ccfg->type = m_type;
		ccfg->proto = m_protocol;

		ch_io_begin([intp, fn_accepted_initializer,ccfg, ch = NRP<socket_channel>(this)]( int status, io_ctx*){
			intp->set(status);
			if(status == netp::OK) {
				ch->ch_io_accept(fn_accepted_initializer, ccfg);
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
			int rt = so->connect(addr);
			if (rt == netp::OK) {
				NETP_TRACE_SOCKET("[socket][%s]socket connected directly", so->ch_info().c_str());
				so->__do_io_dial_done(fn_initializer, dialp, netp::OK, so->m_io_ctx);
				return;
			} else if (netp::E_EINPROGRESS==(rt)) {
				auto fn_connect_done = std::bind(&socket_channel::__do_io_dial_done, so, fn_initializer, dialp, std::placeholders::_1, std::placeholders::_2);
				so->ch_io_connect(fn_connect_done);
				return;
			}
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
			NETP_ASSERT(status != netp::OK);
			m_chflag |= int(channel_flag::F_WRITE_ERROR);
			ch_io_end_connect();
			NETP_ERR("[socket][%s]socket dial error: %d", ch_info().c_str(), status);
			dialp->set(status);
			return;
		}

		if( m_chflag& int(channel_flag::F_CLOSED)) {
			NETP_ERR("[socket][%s]socket closed already, dial promise set abort, errno: %d", ch_info().c_str(), ch_errno() );
			status = netp::E_ECONNABORTED;
			goto _set_fail_and_return;
		}

		status = load_sockname();
		if (status != netp::OK ) {
			goto _set_fail_and_return;
		}

#ifdef _NETP_WIN
		//not sure linux os behaviour, to test
		if (0 == local_addr()->ipv4() && m_type != u8_t(NETP_SOCK_USERPACKET/*FOR BFR*/) ) {
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
			NETP_WARN("[socket][%s]socket selfconnected", ch_info().c_str());
			goto _set_fail_and_return;
		}

		try {
			if (NETP_LIKELY(fn_initializer != nullptr)) {
				fn_initializer(NRP<channel>(this));
			}
		} catch(netp::exception const& e) {
			NETP_ASSERT(e.code() != netp::OK );
			status = e.code();
			goto _set_fail_and_return;
		} catch(std::exception const& e) {
			status = netp_socket_get_last_errno();
			if (status == netp::OK) {
				status = netp::E_UNKNOWN;
			}
			NETP_ERR("[socket][%s]dial error: %d: %s", ch_info().c_str(), status, e.what());
			goto _set_fail_and_return;
		} catch(...) {
			status = netp_socket_get_last_errno();
			if (status == netp::OK) {
				status = netp::E_UNKNOWN;
			}
			NETP_ERR("[socket][%s]dial error: %d: unknown", ch_info().c_str(), status);
			goto _set_fail_and_return;
		}

		NETP_TRACE_SOCKET("[socket][%s]async connected", ch_info().c_str());
		NETP_ASSERT( m_chflag& (int(channel_flag::F_CONNECTING)|int(channel_flag::F_CONNECTED)) );
		
		ch_set_connected();
		ch_io_end_connect();

		dialp->set(netp::OK);

		ch_fire_connected();
		ch_io_read();
	}

	void socket_channel::__do_io_accept_impl(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& cfg, int status, io_ctx* ) {

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
			
			NRP<io_event_loop> LL = io_event_loop_group::instance()->next(L->poller_type());
			LL->execute([LL,fn_initializer,nfd, laddr, raddr, cfg]() {
				NRP<socket_cfg> cfg_ = netp::make_ref<socket_cfg>();
				cfg_->fd = nfd;
				cfg_->family = cfg->family;
				cfg_->type = cfg->type;
				cfg_->proto = cfg->proto;
				cfg_->laddr = laddr;
				cfg_->raddr = raddr;

				cfg_->L = LL;
				cfg_->option = cfg->option;
				cfg_->kvals = cfg->kvals;
				cfg_->sock_buf = cfg->sock_buf;
				cfg_->bdlimit = cfg->bdlimit;
				int rt;
				NRP<socket_channel> so;
				std::tie(rt,  so) = create_socket_channel(cfg_);
				if (rt != netp::OK) {
					NETP_CLOSE_SOCKET(nfd);
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
			if (NETP_UNLIKELY(m_chflag & (int(channel_flag::F_READ_SHUTDOWN) | int(channel_flag::F_CLOSE_PENDING)/*ignore the left read buffer, cuz we're closing it*/))) { return; }
			netp::u32_t nbytes = socket_recvfrom_impl(m_rcv_buf_ptr, m_rcv_buf_size, m_raddr, status);
			if (NETP_LIKELY(nbytes > 0)) {
				channel::ch_fire_readfrom(netp::make_ref<netp::packet>(m_rcv_buf_ptr, nbytes), m_raddr) ;
			}
		}
		___do_io_read_done(status);
	}

	void socket_channel::__do_io_read(int status, io_ctx*) {
		//NETP_INFO("READ IN");
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(!ch_is_listener());

		//in case socket object be destructed during ch_read
		while (status == netp::OK) {
			NETP_ASSERT( (m_chflag&(int(channel_flag::F_READ_SHUTDOWNING))) == 0);
			if (NETP_UNLIKELY(m_chflag & (int(channel_flag::F_READ_SHUTDOWN)|int(channel_flag::F_READ_ERROR) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING)/*ignore the left read buffer, cuz we're closing it*/))) { return; }
			netp::u32_t nbytes = socket_recv_impl(m_rcv_buf_ptr, m_rcv_buf_size, status);
			if (NETP_LIKELY(nbytes > 0)) {
				channel::ch_fire_read(netp::make_ref<netp::packet>(m_rcv_buf_ptr, nbytes));
			}
		}
		___do_io_read_done(status);
	}

	void socket_channel::__do_io_write( int status, io_ctx*) {
		NETP_ASSERT( (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_BDLIMIT)|int(channel_flag::F_CLOSING) )) == 0 );
		//NETP_TRACE_SOCKET("[socket][%s]__do_io_write, write begin: %d, flag: %u", ch_info().c_str(), status , m_chflag );
		if (status == netp::OK) {
			if (m_chflag&int(channel_flag::F_WRITE_ERROR)) {
				NETP_ASSERT(m_outbound_entry_q.size() != 0);
				NETP_ASSERT(ch_errno() != netp::OK);
				NETP_WARN("[socket][%s]__do_io_write(), but socket error already: %d, m_chflag: %u", ch_info().c_str(), ch_errno(), m_chflag);
				return ;
			} else if (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWN))) {
				NETP_ASSERT( m_outbound_entry_q.size() == 0);
				NETP_WARN("[socket][%s]__do_io_write(), but socket write closed already: %d, m_chflag: %u", ch_info().c_str(), ch_errno(), m_chflag);
				return;
			} else {
				m_chflag |= int(channel_flag::F_WRITING);
				status = (is_udp() ? socket_channel::___do_io_write_to() : socket_channel::___do_io_write());
				m_chflag &= ~int(channel_flag::F_WRITING);
			}
		}
		__do_io_write_done(status);
	}

	//write until error
	//<0, is_error == (errno != E_CHANNEL_WRITING)
	//==0, write done
	//this api would be called right after a check of writeable of the current socket
	int socket_channel::___do_io_write() {

		NETP_ASSERT(m_outbound_entry_q.size() != 0, "%s, flag: %u", ch_info().c_str(), m_chflag);
		NETP_ASSERT( m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) );
		NETP_ASSERT( (m_chflag&int(channel_flag::F_BDLIMIT)) ==0);

		//there might be a chance to be blocked a while in this loop, if set trigger another write
		int _errno = netp::OK;
		while ( _errno == netp::OK && m_outbound_entry_q.size() ) {
			NETP_ASSERT( (m_noutbound_bytes) > 0);
			socket_outbound_entry& entry = m_outbound_entry_q.front();
			u32_t dlen = u32_t(entry.data->len());
			u32_t wlen = (dlen);
			if (m_outbound_limit !=0 && (m_outbound_budget<wlen)) {
				wlen =m_outbound_budget;
				if (wlen == 0) {
					NETP_ASSERT(m_chflag& int(channel_flag::F_BDLIMIT_TIMER));
					return netp::E_CHANNEL_BDLIMIT;
				}
			}

			NETP_ASSERT((wlen > 0) && (wlen <= m_noutbound_bytes));
			netp::u32_t nbytes = socket_send_impl( entry.data->head(), u32_t(wlen), _errno);
			if (NETP_LIKELY(nbytes > 0)) {
				m_noutbound_bytes -= nbytes;
				if (m_outbound_limit != 0 ) {
					m_outbound_budget -= nbytes;

					if (!(m_chflag & int(channel_flag::F_BDLIMIT_TIMER)) && m_outbound_budget < (m_outbound_limit >> 1)) {
						m_chflag |= int(channel_flag::F_BDLIMIT_TIMER);
						L->launch(netp::make_ref<netp::timer>(std::chrono::milliseconds(NETP_SOCKET_BDLIMIT_TIMER_DELAY_DUR), &socket_channel::_tmcb_BDL, NRP<socket_channel>(this), std::placeholders::_1));
					}
				}

				if (NETP_LIKELY(nbytes == dlen)) {
					NETP_ASSERT(_errno == netp::OK);
					entry.write_promise->set(netp::OK);
					m_outbound_entry_q.pop_front();
				} else {
					entry.data->skip(nbytes); //ewouldblock or bdlimit
					NETP_ASSERT(entry.data->len());
				}
			}
		}
		return _errno;
	}

	int socket_channel::___do_io_write_to() {

		NETP_ASSERT(m_outbound_entry_q.size() != 0, "%s, flag: %u", ch_info().c_str(), m_chflag);
		NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)));

		//there might be a chance to be blocked a while in this loop, if set trigger another write
		int _errno = netp::OK;
		while (_errno == netp::OK && m_outbound_entry_q.size() ) {
			NETP_ASSERT(m_noutbound_bytes > 0);

			socket_outbound_entry& entry = m_outbound_entry_q.front();
			NETP_ASSERT((entry.data->len() > 0) && (entry.data->len() <= m_noutbound_bytes));
			netp::u32_t nbytes = socket_sendto_impl(entry.data->head(), (u32_t)entry.data->len(), entry.to, _errno);
			//hold a copy before we do pop it from queue
			nbytes == entry.data->len() ? NETP_ASSERT(_errno == netp::OK):NETP_ASSERT(_errno != netp::OK);
			m_noutbound_bytes -= u32_t(entry.data->len());
			entry.write_promise->set(_errno);
			m_outbound_entry_q.pop_front();
		}
		return _errno;
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
		m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CONNECTED));
		NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);

		_ch_do_close_read();
		_ch_do_close_write();

		m_chflag &= ~int(channel_flag::F_CLOSING);

		NETP_ASSERT(m_outbound_entry_q.size() == 0);
		NETP_ASSERT(m_noutbound_bytes == 0);

		//close read, close write might result in F_CLOSED
		ch_rdwr_shutdown_check();
	}

	void socket_channel::ch_close_write_impl(NRP<promise<int>> const& closep) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(!ch_is_listener());
		int prt = netp::OK;
		if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_CLOSING)| int(channel_flag::F_CLOSE_PENDING)) ) {
			prt = (netp::E_OP_INPROCESS);
		} else if (m_chflag&int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
			NETP_ASSERT((m_chflag&int(channel_flag::F_WRITE_ERROR)) == 0);
			NETP_ASSERT(m_chflag&(int(channel_flag::F_WRITING)|int(channel_flag::F_WATCH_WRITE)) );
			NETP_ASSERT(m_outbound_entry_q.size());
			prt = (netp::E_CHANNEL_WRITE_SHUTDOWNING);
		} else if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN)) {
			prt = (netp::E_CHANNEL_WRITE_CLOSED);
		} else if (m_chflag & (int(channel_flag::F_WRITING)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_BDLIMIT)) ) {
			//write set ok might trigger ch_close_write|ch_close
			NETP_ASSERT(m_outbound_entry_q.size());
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
			prt = (netp::E_CHANNEL_WRITE_SHUTDOWNING);
		} else {
			NETP_ASSERT(((m_chflag&(int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED))) == (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED))) ?
				m_outbound_entry_q.size():
				true);

			_ch_do_close_write();
		}

		if (closep) { closep->set(prt); }
	}

	void socket_channel::ch_close_impl(NRP<promise<int>> const& closep) {
		NETP_ASSERT(L->in_event_loop());
		int prt = netp::OK;
		if (m_chflag&int(channel_flag::F_CLOSED)) {
			prt = (netp::E_CHANNEL_CLOSED);
		} else if (ch_is_listener()) {
			_ch_do_close_listener();
		} else if (m_chflag & (int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_CLOSING)) ) {
			prt = (netp::E_OP_INPROCESS);
		} else if (m_chflag & (int(channel_flag::F_WRITING)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_BDLIMIT)) ) {
			//wait for write done event
			NETP_ASSERT( ((m_chflag&int(channel_flag::F_WRITE_ERROR)) == 0) );

			NETP_ASSERT(m_outbound_entry_q.size());
			m_chflag |= int(channel_flag::F_CLOSE_PENDING);
			prt = (netp::E_CHANNEL_CLOSING);
		} else {
			if ((m_chflag & int(channel_flag::F_CONNECTING)) && ch_errno() == netp::OK) {
				m_chflag |= int(channel_flag::F_WRITE_ERROR);
				ch_errno() = (netp::E_CHANNEL_ABORT);
			}

			if (ch_errno() != netp::OK) {
				NETP_ASSERT(m_chflag & (int(channel_flag::F_READ_ERROR) | int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING)));
				NETP_ASSERT( ((m_chflag&(int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED))) == (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_CONNECTED))) ?
					m_outbound_entry_q.size() :
					true);
				//only check write error for q.size
			} else {
				NETP_ASSERT(m_outbound_entry_q.size() == 0);
			}
			_ch_do_close_read_write();
		}

		if (closep) { closep->set(prt); }
	}

#define __CH_WRITEABLE_CHECK__( outlet, chp)  \
		NETP_ASSERT(outlet->len() > 0); \
		NETP_ASSERT(chp != nullptr); \
 \
		if (m_chflag&(int(channel_flag::F_READ_ERROR) | int(channel_flag::F_WRITE_ERROR))) { \
			chp->set(netp::E_CHANNEL_READ_WRITE_ERROR); \
			return ; \
		} \
 \
		if ((m_chflag&int(channel_flag::F_WRITE_SHUTDOWN)) != 0) { \
			chp->set(netp::E_CHANNEL_WRITE_CLOSED); \
			return; \
		} \
 \
		if (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWN_PENDING)|int(channel_flag::F_WRITE_SHUTDOWNING) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING)) ) { \
			chp->set(netp::E_CHANNEL_WRITE_SHUTDOWNING); \
			return ; \
		} \
 \
		const u32_t outlet_len = (u32_t)outlet->len(); \
		/*set the threshold arbitrarily high, the writer have to check the return value if */ \
		if ( (m_noutbound_bytes > 0) && ( (m_noutbound_bytes + outlet_len) > /*m_sock_buf.sndbuf_size,*/u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE))) { \
			NETP_ASSERT(m_noutbound_bytes > 0); \
			NETP_ASSERT(m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE))); \
			chp->set(netp::E_CHANNEL_WRITE_BLOCK); \
			return; \
		} \

	void socket_channel::ch_write_impl(NRP<promise<int>> const& intp, NRP<packet> const& outlet)
	{
		NETP_ASSERT(L->in_event_loop());
		__CH_WRITEABLE_CHECK__(outlet, intp)
		m_outbound_entry_q.push_back({
			netp::make_ref<netp::packet>(outlet->head(), outlet_len),
			intp
		});
		m_noutbound_bytes += outlet_len;

		if (m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_BDLIMIT))) {
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

	void socket_channel::ch_write_to_impl( NRP<promise<int>> const& intp, NRP<packet> const& outlet,NRP<netp::address >const& to) {
		NETP_ASSERT(L->in_event_loop());

		__CH_WRITEABLE_CHECK__(outlet, intp)
		m_outbound_entry_q.push_back({
			netp::make_ref<netp::packet>(outlet->head(), outlet_len),
			intp,
			to,
		});
		m_noutbound_bytes += outlet_len;

		if (m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) ) {
			return;
		}

#ifdef NETP_ENABLE_FAST_WRITE
		//fast write
		m_chflag |= int(channel_flag::F_WRITE_BARRIER);
		__do_io_write(netp::OK,m_io_ctx);
		m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
#else
		ch_io_write();
#endif
	}

	void socket_channel::io_notify_terminating(int status, io_ctx*) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(status == netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING);
		//terminating notify, treat as a error
		NETP_ASSERT(m_chflag & int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE));
		m_chflag |= (int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING));
		m_cherrno = netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING;
		m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
		ch_close_impl(nullptr);
	}

	void socket_channel::io_notify_read(int status, io_ctx* ctx) {
		if (m_chflag & int(channel_flag::F_USE_DEFAULT_READ)) {
			is_udp() ? __do_io_read_from(status, ctx):__do_io_read(status, ctx);
			return;
		}
		NETP_ASSERT( m_fn_read != nullptr );
		(*m_fn_read)(status, ctx);
	}

	void socket_channel::io_notify_write(int status, io_ctx* ctx) {
		if (m_chflag & int(channel_flag::F_USE_DEFAULT_WRITE)) {
			__do_io_write(status, ctx);
			return;
		}
		NETP_ASSERT(m_fn_write != nullptr);
		(*m_fn_write)(status, ctx);
	}

	void socket_channel::__ch_clean() {
		NETP_ASSERT(m_fn_read == nullptr);
		NETP_ASSERT(m_fn_write == nullptr);
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
			ch_errno() = netp::E_IO_BEGIN_FAILED;
			m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
			ch_close(nullptr);
			fn_begin_done(netp::E_IO_BEGIN_FAILED, 0);
			return;
		}
		__io_begin_done(m_io_ctx);
		fn_begin_done(netp::OK, m_io_ctx);
	}

		void socket_channel::ch_io_end() {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_outbound_entry_q.size() == 0);
			NETP_ASSERT(m_noutbound_bytes == 0);
			NETP_ASSERT(m_chflag & int(channel_flag::F_CLOSED));
			NETP_ASSERT((m_chflag & (int(channel_flag::F_WATCH_READ) | int(channel_flag::F_WATCH_WRITE))) == 0);
			NETP_TRACE_SOCKET("[socket][%s]io_action::END, flag: %d", ch_info().c_str(), m_chflag);

			ch_fire_closed(close());
			//delay one tick to hold this
			L->schedule([so = NRP<socket_channel>(this)]() {
				so->__ch_clean();
			});
		}

		//we can override this function to enable custom accept way
		void socket_channel::ch_io_accept(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& cfg, fn_io_event_t const& fn) {
			if (fn != nullptr) {
				ch_io_read(fn);
				(void)fn_initializer;
				(void)cfg;
				return;
			}
			//posix impl
			ch_io_read(std::bind(&socket_channel::__do_io_accept_impl, NRP<socket_channel>(this), fn_initializer, cfg, std::placeholders::_1, std::placeholders::_2));
		}

		void socket_channel::ch_io_read(fn_io_event_t const& fn_read) {
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
				if(fn_read != nullptr) fn_read(rt, nullptr);
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
				L->io_do(io_action::END_READ, m_io_ctx);
				m_chflag &= ~(int(channel_flag::F_USE_DEFAULT_READ) | int(channel_flag::F_WATCH_READ));
				netp::allocator<fn_io_event_t>::trash(m_fn_read);
				m_fn_read = nullptr;
				NETP_TRACE_IOE("[socket][%s]io_action::END_READ", ch_info().c_str());
			}
		}

		void socket_channel::ch_io_write(fn_io_event_t const& fn_write ) {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this), fn_write]()->void {
					_so->ch_io_write(fn_write);
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				NETP_ASSERT(m_chflag & int(channel_flag::F_CONNECTED));
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
				if (fn_write != nullptr) fn_write(rt, nullptr);
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
				L->io_do(io_action::END_WRITE, m_io_ctx);
				m_chflag &= ~(int(channel_flag::F_USE_DEFAULT_WRITE) | int(channel_flag::F_WATCH_WRITE));
				netp::allocator<fn_io_event_t>::trash(m_fn_write);
				m_fn_write = nullptr;
				NETP_TRACE_IOE("[socket][%s]io_action::END_WRITE", ch_info().c_str());
			}
		}
} //end of ns
