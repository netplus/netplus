#include <netp/core.hpp>
#include <netp/logger_broker.hpp>
#include <netp/socket.hpp>

namespace netp {

	int socket::connect(address const& addr) {
		if (m_chflag & (int(channel_flag::F_CONNECTING) | int(channel_flag::F_CONNECTED) | int(channel_flag::F_LISTENING) | int(channel_flag::F_CLOSED)) ) {
			return netp::E_SOCKET_INVALID_STATE;
		}
		NETP_ASSERT((m_chflag & int(channel_flag::F_ACTIVE)) == 0);
		channel::ch_set_active();
		int rt= socket_base::connect(addr);
		if (rt == netp::OK) {
			m_chflag |= int(channel_flag::F_CONNECTED);
			return netp::OK;
		} else if (IS_ERRNO_EQUAL_CONNECTING(rt)) {
			m_chflag |= int(channel_flag::F_CONNECTING);
		} else {
			ch_errno() = rt;
		}
		return rt;
	}

	void socket::do_async_connect(address const& addr, NRP<promise<int>> const& p) {
		NETP_ASSERT(L->in_event_loop());

		int rt = connect(addr);
		//NETP_INFO("connect rt: %d", rt);
                if (IS_ERRNO_EQUAL_CONNECTING(rt)) {
			ch_aio_connect([so=NRP<socket>(this), p](const int aiort_) {
				//NRP< promise<int>> __p__barrier(p);we dont need this line if act executed in Q
				so->ch_aio_end_connect();
				p->set(aiort_);
			});
		} else {
			//ok or error
			NETP_TRACE_SOCKET("[socket][%s]socket connected directly", info().c_str());
			p->set(rt);
		}
	}

	void socket::_tmcb_BDL(NRP<timer> const& t) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_outbound_limit > 0);
		NETP_ASSERT(m_chflag&int(channel_flag::F_BDLIMIT_TIMER) );
		m_chflag &= ~int(channel_flag::F_BDLIMIT_TIMER);
		if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_ERROR)|int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING))) {
			return;
		}

		NETP_ASSERT(m_outbound_limit>=m_outbound_budget);
		std::size_t tokens = m_outbound_limit / (1000/NETP_SOCKET_BDLIMIT_TIMER_DELAY_DUR);
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
			__cb_aio_write_impl(netp::OK);
			m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
		}
	}


	int socket::bind(netp::address const& addr) {
		if (m_chflag & int(channel_flag::F_CLOSED)) {
			return netp::E_SOCKET_INVALID_STATE;
		}
		int rt = socket_base::bind(addr);
		NETP_TRACE_SOCKET("[socket][%s]socket bind rt: %d", info().c_str(), rt );
		return rt;
	}

	int socket::listen( int backlog ) {
		NETP_ASSERT(L->in_event_loop());
		if (m_chflag & int(channel_flag::F_ACTIVE)) {
			return netp::E_SOCKET_INVALID_STATE;
		}

		NETP_ASSERT((m_fd > 0)&&(m_chflag & int(channel_flag::F_LISTENING)) == 0);
		m_chflag |= int(channel_flag::F_LISTENING);
		int rt = socket_base::listen( backlog);
		NETP_TRACE_SOCKET("[socket][%s]socket listen rt: %d", info().c_str(), rt);
		return rt;
	}

	void socket::do_listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted_initializer, NRP<promise<int>> const& chp, NRP<socket_cfg> const& ccfg, int backlog ) {
		if (!L->in_event_loop()) {
			L->schedule([_this=NRP<socket>(this), addr, fn_accepted_initializer, chp,ccfg, backlog]() ->void {
				_this->do_listen_on(addr, fn_accepted_initializer, chp, ccfg, backlog);
			});
			return;
		}

		//int rt = -10043;
		int rt = socket::bind(addr);
		if (rt != netp::OK) {
			NETP_WARN("[socket]socket::bind(): %d, addr: %s", rt, addr.to_string().c_str() );
			chp->set(rt);
			return;
		}

		rt = socket::listen(backlog);
		if (rt != netp::OK) {
			NETP_WARN("[socket]socket::listen(%u): %d, addr: %s",backlog, rt, addr.to_string().c_str());
			chp->set(rt);
			return;
		}

		NETP_ASSERT(rt == netp::OK);
		aio_begin([fn_accepted_initializer, ccfg,chp, so = NRP<socket>(this)](const int aiort_){
			chp->set(aiort_);
			if(aiort_ == netp::OK) {
				so->_do_aio_accept(fn_accepted_initializer, ccfg);
			}
		});
	}

	//NRP<promise<int>> socket::listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<socket_cfg> const& ccfg, int backlog) {
	//	NRP<promise<int>> ch_p = make_ref<promise<int>>();
	//	do_listen_on(addr, fn_accepted, ch_p, ccfg, backlog);
	//	return ch_p;
	//}

	void socket::do_dial(address const& addr, fn_channel_initializer_t const& initializer, NRP<promise<int>> const& so_dialf) {
		NETP_ASSERT(L->in_event_loop());
		socket::aio_begin([so=NRP<socket>(this),addr, initializer, so_dialf](const int aiort_) {
			NETP_ASSERT(so->L->in_event_loop());
			if (aiort_ != netp::OK) {
				so_dialf->set(aiort_);
				return;
			}
			NRP<promise<int>> connp = netp::make_ref<promise<int>>();
			connp->if_done([so,initializer, so_dialf, connp/*hold a copy*/](int const& rt) {
				so->_do_dial_done_impl(rt, initializer, so_dialf);
			});
			so->do_async_connect(addr, connp);
		});
	}

	/*
	NRP<promise<int>> socket::dial(address const& addr, fn_channel_initializer_t const& initializer) {
		NRP<promise<int>> f = make_ref<promise<int>>();
		if (L->in_event_loop()) {
			do_dial(addr, initializer, f);
		} else {
			L->execute([s=NRP<socket>(this),addr,initializer,f]() {
				s->do_dial(addr, initializer, f);
			});
		}
		return f;
	}
	*/

	SOCKET socket::accept( address& raddr ) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_chflag & int(channel_flag::F_LISTENING));
		if ( (m_chflag & int(channel_flag::F_LISTENING)) ==0 ) {
			netp_set_last_errno(netp::E_SOCKET_INVALID_STATE);
			return (SOCKET)NETP_SOCKET_ERROR;
		}

		return socket_base::accept(raddr);
	}

	void socket::_do_dial_done_impl(int rt, fn_channel_initializer_t const& fn_ch_initialize, NRP<promise<int>> const& so_dialf) {
		NETP_ASSERT(L->in_event_loop());

		if (rt != netp::OK) {
		_set_fail_and_return:
			NETP_ASSERT(rt != netp::OK);
			NETP_ERR("[socket][%s]socket dial error: %d", info().c_str(), rt );
			so_dialf->set(rt);
			return;
		}

		if( m_chflag& int(channel_flag::F_CLOSED)) {
			NETP_ERR("[socket][%s]socket closed already, dial promise set abort, errno: %d", info().c_str(), ch_errno() );
			so_dialf->set(netp::E_ECONNABORTED);
            return ;
		}

		rt = load_sockname();
		if (rt != netp::OK ) {
			goto _set_fail_and_return;
		}

#ifdef _NETP_WIN
		//not sure linux os behaviour, to test
		if (0 == local_addr().ipv4() && m_type != u8_t(NETP_SOCK_USERPACKET/*FOR BFR*/) ) {
			rt = netp::E_WSAENOTCONN;
			goto _set_fail_and_return;
		}
#endif
		netp::address raddr;
		rt = netp::getpeername(*m_api, m_fd, raddr);
		if (rt != netp::OK ) {
			goto _set_fail_and_return;
		}
		if (raddr != m_raddr) {
			rt = netp::E_UNKNOWN;
			goto _set_fail_and_return;
		}

		if (local_addr() == remote_addr()) {
			rt = netp::E_SOCKET_SELF_CONNCTED;
			NETP_WARN("[socket][%s]socket selfconnected", info().c_str());
			goto _set_fail_and_return;
		}

		try {
			if (NETP_LIKELY(fn_ch_initialize != nullptr)) {
				fn_ch_initialize(NRP<channel>(this));
			}
		} catch(netp::exception const& e) {
			NETP_ASSERT(e.code() != netp::OK );
			rt = e.code();
		} catch(std::exception const& e) {
			rt = netp_socket_get_last_errno();
			if (rt == netp::OK) {
				rt = netp::E_UNKNOWN;
			}
			NETP_ERR("[socket][%s]dial error: %d: %s", info().c_str(), rt, e.what());
			goto _set_fail_and_return;
		} catch(...) {
			rt = netp_socket_get_last_errno();
			if (rt == netp::OK) {
				rt = netp::E_UNKNOWN;
			}
			NETP_ERR("[socket][%s]dial error: %d: unknown", info().c_str(), rt);
			goto _set_fail_and_return;
		}

		NETP_TRACE_SOCKET("[socket][%s]async connected", info().c_str());
		NETP_ASSERT( m_chflag& (int(channel_flag::F_CONNECTING)|int(channel_flag::F_CONNECTED)) );
		ch_set_connected();
		so_dialf->set(netp::OK);

		NETP_ASSERT(m_sock_buf.rcvbuf_size > 0, "info: %s", ch_info().c_str() );
		NETP_ASSERT(m_sock_buf.sndbuf_size > 0, "info: %s", ch_info().c_str());
		ch_fire_connected();
		ch_aio_read();
	}

	void socket::__cb_aio_accept_impl(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& ccfg, int rt) {

		NETP_ASSERT(L->in_event_loop());
		/*ignore the left fds, cuz we're closed*/
		if (NETP_UNLIKELY( m_chflag&int(channel_flag::F_CLOSED)) ) { return; }

		NETP_ASSERT(fn_initializer != nullptr);
		while (rt == netp::OK) {
			address raddr;
			SOCKET nfd = socket_base::accept(raddr);
			if (nfd == NETP_SOCKET_ERROR) {
				rt = netp_socket_get_last_errno();
				if (rt == netp::E_EINTR) {
					rt = netp::OK;
					continue;
				} else {
					break;
				}
			}

			//patch for local addr
			address laddr;
			rt = netp::getsockname(*m_api, nfd, laddr);
			if (rt != netp::OK) {
				NETP_ERR("[socket][%s][accept]load local addr failed: %d", info().c_str(), netp_socket_get_last_errno());
				NETP_CLOSE_SOCKET(nfd);
				continue;
			}

			NETP_ASSERT(laddr.family() == (m_family));
			if (laddr == raddr) {
				NETP_ERR("[socket][%s][accept]laddr == raddr, force close", info().c_str());
				NETP_CLOSE_SOCKET(nfd);
				continue;
			}

			__do_create_accepted_socket(nfd, laddr, raddr, fn_initializer, ccfg);
		}

		if (IS_ERRNO_EQUAL_WOULDBLOCK(rt)) {
			//TODO: check the following errno
			//ENETDOWN, EPROTO,ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH, EOPNOTSUPP
			return;
		}

		if(rt == netp::E_EMFILE ) {
			NETP_WARN("[socket][%s]accept error, EMFILE", info().c_str() );
			return ;
		}

		NETP_ERR("[socket][%s]accept error: %d", info().c_str(), rt);
		ch_errno()=(rt);
		m_chflag |= int(channel_flag::F_READ_ERROR);
		ch_close_impl(nullptr);
	}

	void socket::__cb_aio_read_impl(const int aiort_) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(!ch_is_listener());
		int aiort = aiort_;
		if (m_protocol == u8_t(NETP_PROTOCOL_UDP)) {
			while (aiort == netp::OK) {
				NETP_ASSERT((m_chflag & (int(channel_flag::F_READ_SHUTDOWNING))) ==0 );
				if (NETP_UNLIKELY(m_chflag & ( int(channel_flag::F_READ_SHUTDOWN) | int(channel_flag::F_CLOSE_PENDING)/*ignore the left read buffer, cuz we're closing it*/))) { return; }
				netp::u32_t nbytes = socket_base::recvfrom(m_rcv_buf_ptr, m_rcv_buf_size, m_raddr, aiort);
				if (NETP_LIKELY(nbytes > 0)) {
					channel::ch_fire_readfrom(netp::make_ref<netp::packet>(m_rcv_buf_ptr, nbytes),m_raddr );
				}
			}
		} else {
			//in case socket object be destructed during ch_read
			while (aiort == netp::OK) {
				NETP_ASSERT( (m_chflag&(int(channel_flag::F_READ_SHUTDOWNING))) == 0);
				if (NETP_UNLIKELY(m_chflag & (int(channel_flag::F_READ_SHUTDOWN)|int(channel_flag::F_READ_ERROR) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING)/*ignore the left read buffer, cuz we're closing it*/))) { return; }
				netp::u32_t nbytes = socket_base::recv(m_rcv_buf_ptr, m_rcv_buf_size, aiort);
				if (NETP_LIKELY(nbytes > 0)) {
					channel::ch_fire_read(netp::make_ref<netp::packet>(m_rcv_buf_ptr, nbytes));
				}
			}
		}

		switch (aiort) {
			case netp::OK:
			case netp::E_SOCKET_READ_BLOCK:
			{}
			break;
			case netp::E_SOCKET_GRACE_CLOSE:
			{
				NETP_ASSERT(m_protocol != u8_t(NETP_PROTOCOL_UDP));
				m_chflag |= int(channel_flag::F_FIN_RECEIVED);
				ch_close_read_impl(nullptr);
			}
			break;
			default:
			{
				NETP_ASSERT(aiort < 0);
				ch_aio_end_read();
				m_chflag |= int(channel_flag::F_READ_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_BDLIMIT));
				ch_errno()=(aiort);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]__cb_aio_read_impl, _ch_do_close_read_write, read error: %d, close, flag: %u", info().c_str(), aiort, m_chflag );
			}
		}
	}

	void socket::__cb_aio_write_impl(const int aiort_) {
		int aiort = aiort_;
		NETP_ASSERT( (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_BDLIMIT)|int(channel_flag::F_CLOSING) )) == 0 );
		//NETP_TRACE_SOCKET("[socket][%s]__cb_aio_write_impl, write begin: %d, flag: %u", info().c_str(), aiort_ , m_chflag );
		if (aiort == netp::OK) {
			if (m_chflag&int(channel_flag::F_WRITE_ERROR)) {
				NETP_ASSERT(m_outbound_entry_q.size() != 0);
				NETP_ASSERT(ch_errno() != netp::OK);
				NETP_WARN("[socket][%s]__cb_aio_write_impl(), but socket error already: %d, m_chflag: %u", info().c_str(), ch_errno(), m_chflag);
				return ;
			} else if (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWN))) {
				NETP_ASSERT( m_outbound_entry_q.size() == 0);
				NETP_WARN("[socket][%s]__cb_aio_write_impl(), but socket write closed already: %d, m_chflag: %u", info().c_str(), ch_errno(), m_chflag);
				return;
			} else {
				m_chflag |= int(channel_flag::F_WRITING);
				aiort = u8_t(NETP_PROTOCOL_UDP) != m_protocol ?
					socket::_do_ch_write_impl():
					socket::_do_ch_write_to_impl() ;
				m_chflag &= ~int(channel_flag::F_WRITING);
			}
		}

		switch (aiort) {
		case netp::OK:
		{
			NETP_ASSERT( (m_chflag & int(channel_flag::F_BDLIMIT)) == 0);
			NETP_ASSERT( m_outbound_entry_q.size() == 0);
			if (m_chflag&int(channel_flag::F_CLOSE_PENDING)) {
				_ch_do_close_read_write();
				NETP_TRACE_SOCKET("[socket][%s]aio_write, end F_CLOSE_PENDING, _ch_do_close_read_write, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
			} else if (m_chflag&int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
				_ch_do_close_write();
				NETP_TRACE_SOCKET("[socket][%s]aio_write, end F_WRITE_SHUTDOWN_PENDING, ch_close_write, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
			} else {
				std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>>().swap(m_outbound_entry_q) ;
				ch_aio_end_write();
			}
		}
		break;
		case netp::E_SOCKET_WRITE_BLOCK:
		{
			NETP_ASSERT(m_outbound_entry_q.size() > 0);
#ifdef NETP_ENABLE_FAST_WRITE
			NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) );
			ch_aio_write();
#else
			NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_WRITE));
#endif
			//NETP_TRACE_SOCKET("[socket][%s]__cb_aio_write_impl, write block", info().c_str());
		}
		break;
		case netp::E_CHANNEL_BDLIMIT:
		{
			m_chflag |= int(channel_flag::F_BDLIMIT);
			ch_aio_end_write();
		}
		break;
		default:
		{
			ch_aio_end_write();
			m_chflag |= int(channel_flag::F_WRITE_ERROR);
			m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
			socket::ch_errno()=(aiort);
			ch_close_impl(nullptr);
			NETP_WARN("[socket][%s]__cb_aio_write_impl, call_ch_do_close_read_write, write error: %d, m_chflag: %u", info().c_str(), aiort, m_chflag);
		}
		break;
		}
	}

	//write until error
	//<0, is_error == (errno != E_CHANNEL_WRITING)
	//==0, write done
	//this api would be called right after a check of writeable of the current socket
	int socket::_do_ch_write_impl() {

		NETP_ASSERT(m_outbound_entry_q.size() != 0, "%s, flag: %u", info().c_str(), m_chflag);
		NETP_ASSERT( m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) );
		NETP_ASSERT( (m_chflag&int(channel_flag::F_BDLIMIT)) ==0);

		//there might be a chance to be blocked a while in this loop, if set trigger another write
		int _errno = netp::OK;
		while ( _errno == netp::OK && m_outbound_entry_q.size() ) {
			NETP_ASSERT( (m_noutbound_bytes) > 0);
			socket_outbound_entry& entry = m_outbound_entry_q.front();
			netp::size_t dlen = entry.data->len();
			netp::size_t wlen = (dlen);
			if (m_outbound_limit !=0 && m_outbound_budget<wlen) {
				wlen =m_outbound_budget;
				if (wlen == 0) {
					NETP_ASSERT(m_chflag& int(channel_flag::F_BDLIMIT_TIMER));
					return netp::E_CHANNEL_BDLIMIT;
				}
			}

			NETP_ASSERT((wlen > 0) && (wlen <= m_noutbound_bytes));
			netp::u32_t nbytes = socket_base::send(entry.data->head(), u32_t(wlen), _errno);
			if (NETP_LIKELY(nbytes > 0)) {
				m_noutbound_bytes -= nbytes;
				if (m_outbound_limit != 0 ) {
					m_outbound_budget -= nbytes;

					if (!(m_chflag & int(channel_flag::F_BDLIMIT_TIMER)) && m_outbound_budget < (m_outbound_limit >> 1)) {
						m_chflag |= int(channel_flag::F_BDLIMIT_TIMER);
						L->launch(netp::make_ref<netp::timer>(std::chrono::milliseconds(NETP_SOCKET_BDLIMIT_TIMER_DELAY_DUR), &socket::_tmcb_BDL, NRP<socket>(this), std::placeholders::_1));
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

	int socket::_do_ch_write_to_impl() {

		NETP_ASSERT(m_outbound_entry_q.size() != 0, "%s, flag: %u", info().c_str(), m_chflag);
		NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)));

		//there might be a chance to be blocked a while in this loop, if set trigger another write
		int _errno = netp::OK;
		while (_errno == netp::OK && m_outbound_entry_q.size() ) {
			NETP_ASSERT(m_noutbound_bytes > 0);

			socket_outbound_entry& entry = m_outbound_entry_q.front();
			NETP_ASSERT((entry.data->len() > 0) && (entry.data->len() <= m_noutbound_bytes));
			netp::u32_t nbytes = socket_base::sendto(entry.data->head(), (u32_t)entry.data->len(), entry.to, _errno);
			//hold a copy before we do pop it from queue
			nbytes == entry.data->len() ? NETP_ASSERT(_errno == netp::OK):NETP_ASSERT(_errno != netp::OK);
			m_noutbound_bytes -= entry.data->len();
			entry.write_promise->set(_errno);
			m_outbound_entry_q.pop_front();
		}
		return _errno;
	}

	void socket::_ch_do_close_listener() {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_chflag & int(channel_flag::F_LISTENING));
		NETP_ASSERT((m_chflag & int(channel_flag::F_CLOSED)) ==0 );

		m_chflag |= int(channel_flag::F_CLOSED);
		socket::_do_aio_end_accept();
		aio_end();
		NETP_TRACE_SOCKET("[socket][%s]ch_do_close_listener end", info().c_str());
	}

	inline void socket::_ch_do_close_read_write() {
		NETP_ASSERT(L->in_event_loop());

		if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING) | int(channel_flag::F_WRITE_SHUTDOWNING))) {
			return;
		}

		NETP_ASSERT((m_chflag & int(channel_flag::F_CLOSED)) == 0);
		m_chflag |= int(channel_flag::F_CLOSING);
		m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CONNECTED));
		NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read_write, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);

		_ch_do_close_read();
		_ch_do_close_write();

		m_chflag |= int(channel_flag::F_CLOSED);
		m_chflag &= ~int(channel_flag::F_CLOSING);

		NETP_ASSERT(m_outbound_entry_q.size() == 0);
		NETP_ASSERT(m_noutbound_bytes == 0);

		aio_end();
	}

	void socket::ch_close_write_impl(NRP<promise<int>> const& closep) {
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

	void socket::ch_close_impl(NRP<promise<int>> const& closep) {
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
				NETP_ASSERT(m_chflag & (int(channel_flag::F_READ_ERROR) | int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_IO_EVENT_LOOP_BEGIN_FAILED) | int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING)));
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
		if ( (m_noutbound_bytes > 0) && (m_noutbound_bytes + outlet_len > m_sock_buf.sndbuf_size)) { \
			NETP_ASSERT(m_noutbound_bytes > 0); \
			NETP_ASSERT(m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE))); \
			chp->set(netp::E_CHANNEL_WRITE_BLOCK); \
			return; \
		} \

	void socket::ch_write_impl(NRP<packet> const& outlet, NRP<promise<int>> const& chp)
	{
		NETP_ASSERT(L->in_event_loop());
		__CH_WRITEABLE_CHECK__(outlet, chp)
		m_outbound_entry_q.push_back({
			netp::make_ref<netp::packet>(outlet->head(), outlet_len),
			chp
		});
		m_noutbound_bytes += outlet_len;

		if (m_chflag&(int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)|int(channel_flag::F_BDLIMIT))) {
			return;
		}

#ifdef NETP_ENABLE_FAST_WRITE
		//fast write
		m_chflag |= int(channel_flag::F_WRITE_BARRIER);
		__cb_aio_write_impl(netp::OK);
		m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
#else
		ch_aio_write();
#endif
	}

	void socket::ch_write_to_impl(NRP<packet> const& outlet, netp::address const& to, NRP<promise<int>> const& chp) {
		NETP_ASSERT(L->in_event_loop());

		__CH_WRITEABLE_CHECK__(outlet, chp)
		m_outbound_entry_q.push_back({
			netp::make_ref<netp::packet>(outlet->head(), outlet_len),
			chp,
			to,
		});
		m_noutbound_bytes += outlet_len;

		if (m_chflag & (int(channel_flag::F_WRITE_BARRIER)|int(channel_flag::F_WATCH_WRITE)) ) {
			return;
		}

#ifdef NETP_ENABLE_FAST_WRITE
		//fast write
		m_chflag |= int(channel_flag::F_WRITE_BARRIER);
		__cb_aio_write_impl(netp::OK);
		m_chflag &= ~int(channel_flag::F_WRITE_BARRIER);
#else
		ch_aio_write();
#endif
	}

} //end of ns
