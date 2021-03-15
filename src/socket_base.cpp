#include <netp/socket_base.hpp>

namespace netp {

	socket_base::socket_base(SOCKET fd, int family, int sockt, int proto, address const& laddr, address const& raddr, const socket_api* sockapi) :
		m_fd(fd),

		m_family(u8_t(family)),
		m_type(u8_t(sockt)),
		m_protocol(u16_t(proto)),
		m_option(0),

		m_api(sockapi==nullptr?((netp::socket_api*)&netp::NETP_DEFAULT_SOCKAPI):sockapi),

		m_laddr(laddr),
		m_raddr(raddr),

		m_kvals({0}),
		m_sock_buf({0,0})
	{
		NETP_ASSERT(proto < NETP_PROTOCOL_MAX);
	}

	socket_base::~socket_base() {}

	int socket_base::_cfg_reuseaddr(bool onoff) {
		NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

		bool setornot = ((m_option& netp::u16_t(socket_option::OPTION_REUSEADDR)) && (!onoff)) ||
			(((m_option& netp::u16_t(socket_option::OPTION_REUSEADDR)) == 0) && (onoff));

		if (!setornot) {
			return netp::OK;
		}

		int optval = onoff ? 1 : 0;
		int rt= m_api->setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		if (onoff) {
			m_option |= u16_t(socket_option::OPTION_REUSEADDR);
		} else {
			m_option &= ~u16_t(socket_option::OPTION_REUSEADDR);
		}
		return netp::OK;
	}

	int socket_base::_cfg_reuseport(bool onoff) {
		(void)onoff;

#if defined(_NETP_GNU_LINUX)|| defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

		bool setornot = ((m_option& u16_t(socket_option::OPTION_REUSEPORT)) && (!onoff)) ||
			(((m_option& u16_t(socket_option::OPTION_REUSEPORT)) == 0) && (onoff));

		if (!setornot) {
			return netp::OK;
		}

		int optval = onoff ? 1 : 0;
		int rt = m_api->setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		if (onoff) {
			m_option |= u16_t(socket_option::OPTION_REUSEPORT);
		} else {
			m_option &= ~u16_t(socket_option::OPTION_REUSEPORT);
		}
		return netp::OK;
#else
		return netp::E_INVALID_OPERATION;
#endif
	}
	int socket_base::_cfg_nonblocking(bool onoff) {
		NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

		bool setornot = ((m_option&int(socket_option::OPTION_NON_BLOCKING)) && (!onoff)) ||
			(((m_option&int(socket_option::OPTION_NON_BLOCKING)) == 0) && (onoff));

		if (!setornot) {
			return netp::OK;
		}

		int rt = m_api->set_nonblocking(m_fd, onoff);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		if (onoff) {
			m_option |= int(socket_option::OPTION_NON_BLOCKING);
		} else {
			m_option &= ~int(socket_option::OPTION_NON_BLOCKING);
		}
		return netp::OK;
	}

	int socket_base::_cfg_buffer(channel_buf_cfg const& cfg) {
		int rt = set_snd_buffer_size (cfg.sndbuf_size);
		NETP_RETURN_V_IF_MATCH(rt, rt < 0);

		return set_rcv_buffer_size(cfg.rcvbuf_size);
	}

	int socket_base::_cfg_nodelay(bool onoff) {

		NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
		bool setornot = ((m_option& u16_t(socket_option::OPTION_NODELAY)) && (!onoff)) ||
			(((m_option& u16_t(socket_option::OPTION_NODELAY)) == 0) && (onoff));

		if (!setornot) {
			return netp::OK;
		}

		int optval = onoff ? 1 : 0;
		int rt = m_api->setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		if (onoff) {
			m_option |= u16_t(socket_option::OPTION_NODELAY);
		} else {
			m_option &= ~u16_t(socket_option::OPTION_NODELAY);
		}
		return netp::OK;
	}

	int socket_base::__cfg_keepalive_vals(keep_alive_vals const& vals) {
		/*WCP WILL SETUP A DEFAULT KAV AT START FOR THE CURRENT IMPL*/
		if (!(m_protocol == u16_t(NETP_PROTOCOL_TCP))) { return netp::E_INVALID_OPERATION; }
		NETP_ASSERT(m_option & int(socket_option::OPTION_KEEP_ALIVE));
		int rt;
#ifdef _NETP_WIN
		DWORD dwBytesRet;
		struct tcp_keepalive alive;
		::memset(&alive, 0, sizeof(alive));

		alive.onoff = 1;
		if (vals.idle == 0) {
			alive.keepalivetime = (NETP_DEFAULT_TCP_KEEPALIVE_IDLETIME*1000);
		} else {
			alive.keepalivetime = (vals.idle*1000);
		}

		if (vals.interval == 0) {
			alive.keepaliveinterval = (NETP_DEFAULT_TCP_KEEPALIVE_INTERVAL*1000);
		} else {
			alive.keepaliveinterval = vals.interval*1000;
		}

		rt = ::WSAIoctl(m_fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), nullptr, 0, &dwBytesRet, nullptr, nullptr);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
#elif defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		if (vals.idle != 0) {
			int idle = (vals.idle);
			rt = m_api->setsockopt(m_fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		}
		if (vals.interval != 0) {
			int interval = (vals.interval);
			rt = m_api->setsockopt(m_fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		}
		if (vals.probes != 0) {
			int probes = vals.probes;
			rt = m_api->setsockopt(m_fd, SOL_TCP, TCP_KEEPCNT, &probes, sizeof(probes));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		}
#else
		#error
#endif
		m_kvals = vals;
		return netp::OK;
	}

	int socket_base::_cfg_keepalive(bool onoff, keep_alive_vals const& vals) {
		/*WCP WILL SETUP A DEFAULT KAV AT START FOR THE CURRENT IMPL*/
		if (!(m_protocol == u16_t(NETP_PROTOCOL_TCP))) { return netp::E_INVALID_OPERATION; }
		//force to false
		int rt = netp::set_keepalive(*m_api, m_fd, onoff);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		if (onoff) {
			m_option |= int(socket_option::OPTION_KEEP_ALIVE);
			return __cfg_keepalive_vals(vals);
		} else {
			m_option &= ~int(socket_option::OPTION_KEEP_ALIVE);
			return netp::OK;
		}
	}

	int socket_base::_cfg_broadcast(bool onoff) {
		NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
		NETP_RETURN_V_IF_NOT_MATCH(netp::E_INVALID_OPERATION, m_protocol == u8_t(NETP_PROTOCOL_UDP));

		bool setornot = ((m_option& u16_t(socket_option::OPTION_BROADCAST)) && (!onoff)) ||
			(((m_option& u16_t(socket_option::OPTION_BROADCAST)) == 0) && (onoff));

		if (!setornot) {
			return netp::OK;
		}
		int rt = netp::set_broadcast(*m_api,m_fd, onoff);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		if (onoff) {
			m_option |= u16_t(socket_option::OPTION_BROADCAST);
		} else {
			m_option &= ~u16_t(socket_option::OPTION_BROADCAST);
		}
		return netp::OK;
	}

	int socket_base::_cfg_option(u16_t opt, keep_alive_vals const& kvals) {
		//force nonblocking
		int rt = _cfg_nonblocking((opt& u16_t(socket_option::OPTION_NON_BLOCKING)) != 0);
		NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

		rt = _cfg_reuseaddr((opt& u16_t(socket_option::OPTION_REUSEADDR)) !=0);
		NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

#if defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		rt = _cfg_reuseport((m_option&u8_t(socket_option::OPTION_REUSEPORT))!=0);
		NETP_RETURN_V_IF_MATCH(NETP_SOCKET_ERROR, rt == NETP_SOCKET_ERROR);
#endif
		
		if (m_protocol == NETP_PROTOCOL_UDP) {
			rt = _cfg_broadcast((opt & u16_t(socket_option::OPTION_BROADCAST)) != 0);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
		}

		if (m_protocol == NETP_PROTOCOL_TCP) {
			rt = _cfg_nodelay((opt & u16_t(socket_option::OPTION_NODELAY)) != 0);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

			rt = _cfg_keepalive((opt & u16_t(socket_option::OPTION_KEEP_ALIVE)) != 0, kvals);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
		}
		return netp::OK;
	}

	int socket_base::open() {
		NETP_ASSERT(m_fd == NETP_INVALID_SOCKET);
		m_fd = netp::open(*m_api,m_family, m_type, m_protocol);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), m_fd == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_base::close() {
		int rt = netp::close(*m_api,m_fd);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_base::shutdown(int flag) {
		int rt = netp::shutdown(*m_api,m_fd, flag);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_base::bind( address const& addr) {
		NETP_ASSERT(m_laddr.is_null());

		NETP_ASSERT((m_family) == addr.family());
		int rt= netp::bind(*m_api,m_fd , addr);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		m_laddr = addr;
		return netp::OK;
	}

	int socket_base::listen(int backlog) {
		NETP_ASSERT(m_fd>0);
		int rt;

		if (m_protocol == u16_t(NETP_PROTOCOL_UDP)) {
			rt = netp::OK;
		} else {
			rt = netp::listen(*m_api,m_fd, backlog);
		}

		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	SOCKET socket_base::accept(address& addr) {
		return netp::accept(*m_api,m_fd, addr);
	}

	int socket_base::connect(address const& addr ) {
		NETP_ASSERT(m_raddr.is_null());

		NETP_ASSERT((m_family) == addr.family());
		m_raddr = addr;

#ifdef NETP_HAS_POLLER_IOCP
		//connectex requires the socket to be initially bound
		struct sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));
		addr_in.sin_family = m_family;
		addr_in.sin_addr.s_addr = INADDR_ANY;
		addr_in.sin_port = 0;
		int bindrt = ::bind( m_fd, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));
		if (bindrt != netp::OK ) {
			bindrt = netp_socket_get_last_errno();
			NETP_DEBUG("bind failed: %d\n", bindrt );
			return bindrt;
		}
		return netp::E_EINPROGRESS;
#else
		int rt= netp::connect(*m_api,m_fd, addr );
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
#endif
	}

	//for UDP socket, snd buffer is just a DASH BOX, the maxmium size set a limit on the largest packet that could get through
	int socket_base::set_snd_buffer_size(u32_t size) {
		NETP_ASSERT(m_fd > 0);

		if(size ==0) {
			int nsize = get_snd_buffer_size();
			NETP_RETURN_V_IF_MATCH(nsize, nsize < 0);
			m_sock_buf.sndbuf_size = nsize;
			return netp::OK;
		} else if (size < u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE) ) {
			size = u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE);
		} else if (size > u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE);
		}

		int rt;
#ifdef _DEBUG
		rt = get_snd_buffer_size();
		NETP_TRACE_SOCKET("[socket_base][%s]snd buffer size: %u, try to set: %u", info().c_str(), rt, size );
#endif

		rt = socket_base::setsockopt(SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size));
		if (rt == NETP_SOCKET_ERROR) {
			const int errno_ = netp_socket_get_last_errno();
			NETP_WARN("[socket_base][%s]setsockopt failed: %d", info().c_str(), errno_);
			return errno_;
		}

		int nsize = get_snd_buffer_size();
		NETP_RETURN_V_IF_MATCH(nsize, nsize <0);
		m_sock_buf.sndbuf_size = nsize;
		NETP_TRACE_SOCKET("[socket_base][%s]snd buffer size new: %u", info().c_str(), m_sock_buf.sndbuf_size);

		return netp::OK;
	}

	int socket_base::get_snd_buffer_size() const {
		NETP_ASSERT(m_fd > 0);
		int size;
		socklen_t opt_length = sizeof(u32_t);
		int rt = socket_base::getsockopt(SOL_SOCKET, SO_SNDBUF, (char*)&size, &opt_length);
		if (rt == NETP_SOCKET_ERROR) {
			const int errno_ = netp_socket_get_last_errno();
			NETP_WARN("[socket_base][%s]getsockopt failed: %d", info().c_str(), errno_);
			return errno_;
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

	int socket_base::set_rcv_buffer_size(u32_t size) {
		NETP_ASSERT(m_fd != NETP_INVALID_SOCKET );

		if (size == 0) {
			int nsize = get_rcv_buffer_size();
			NETP_RETURN_V_IF_MATCH(nsize, nsize < 0);
			m_sock_buf.rcvbuf_size = nsize;
			return netp::OK;
		} else if (size < u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE);
		} else if (size > u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE)) {
			size = u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE);
		}

		int rt;
#ifdef _DEBUG
		rt = get_rcv_buffer_size();
		NETP_TRACE_SOCKET("[socket_base][%s]rcv buffer size: %u, try to set: %u", info().c_str(), rt, size );
#endif

		rt = socket_base::setsockopt(SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size));
		if (rt == NETP_SOCKET_ERROR) {
			const int errno_ = netp_socket_get_last_errno();
			NETP_WARN("[socket_base][%s]setsockopt failed: %d", info().c_str(), errno_);
			return errno_;
		}
		int nsize = get_rcv_buffer_size();
		NETP_RETURN_V_IF_MATCH(nsize, nsize < 0);
		m_sock_buf.rcvbuf_size = nsize;
		NETP_TRACE_SOCKET("[socket_base][%s]rcv buffer size new: %u", info().c_str(), m_sock_buf.rcvbuf_size);
		return netp::OK;
	}

	int socket_base::get_rcv_buffer_size() const {
		NETP_ASSERT(m_fd > 0);
		int size;
		socklen_t opt_length = sizeof(u32_t);
		int rt = socket_base::getsockopt(SOL_SOCKET, SO_RCVBUF, (char*)&size, &opt_length);
		if (rt == NETP_SOCKET_ERROR) {
			const int errno_ = netp_socket_get_last_errno();
			NETP_WARN("[socket_base][%s]getsockopt failed: %d", info().c_str(), errno_);
			return errno_;
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

	int socket_base::get_linger(bool& on_off, int& linger_t) const {
		NETP_ASSERT(m_fd > 0);
		struct linger soLinger;
		socklen_t opt_length = sizeof(soLinger);
		int rt = socket_base::getsockopt(SOL_SOCKET, SO_LINGER, (char*)&soLinger, &opt_length);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);

		on_off = (soLinger.l_onoff != 0);
		linger_t = soLinger.l_linger;
		return netp::OK;
	}

	int socket_base::set_linger(bool on_off, int linger_t /* in seconds */) {
		struct linger soLinger;
		NETP_ASSERT(m_fd > 0);
		soLinger.l_onoff = on_off;
		soLinger.l_linger = (linger_t&0xFFFF);
		int rt = socket_base::setsockopt(SOL_SOCKET, SO_LINGER, (char*)&soLinger, sizeof(soLinger));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	int socket_base::get_tos(u8_t& tos) const {
		u8_t _tos;
		socklen_t length;

		int rt = socket_base::getsockopt(IPPROTO_IP, IP_TOS, (char*)&_tos, &length);
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		tos = IPTOS_TOS(_tos);
		return netp::OK;
	}

	int socket_base::set_tos(u8_t tos) {
		NETP_ASSERT(m_fd>0);
		u8_t _tos = IPTOS_TOS(tos) | 0xe0;
		int rt= socket_base::setsockopt(IPPROTO_IP, IP_TOS, (char*)&_tos, sizeof(_tos));
		NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}
}
