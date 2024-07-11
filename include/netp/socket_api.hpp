 #ifndef NETP_NET_SOCKET_API_HPP
#define NETP_NET_SOCKET_API_HPP

#include <netp/core.hpp>
#include <netp/address.hpp>

#ifdef _NETP_WIN
	#include <netp/os/winsock_helper.hpp>
	#define WINAPI __stdcall 
#endif




namespace netp {

#ifdef _NETP_WIN
	inline netp::u32_t __recvonemsg(SOCKET fd, byte_t* const buff_o, netp::u32_t const bsize, NRP<address>& raddr, ipv4_t& lipv4, int& ec_o, int flag) {
		const static LPFN_WSARECVMSG __fn_wsa_recvmsg = (LPFN_WSARECVMSG)netp::os::load_api_ex_address(netp::os::winsock_api_ex::API_RECVMSG);
		NETP_ASSERT(__fn_wsa_recvmsg != nullptr);

	_recvmsg:
		char ctl_buffer[1024] = { 0 };
		DWORD _nbytes = 0;
		raddr = netp::make_ref<address>();
		WSABUF wsa_buf_data = { bsize, (char*)buff_o };
		WSAMSG _msg = {
			raddr->sockaddr_v4(),
			sizeof(struct sockaddr_in),
			&wsa_buf_data,
			1,
			{sizeof(ctl_buffer),ctl_buffer},
			0
		};
		int rt = __fn_wsa_recvmsg(fd, &_msg, &_nbytes, nullptr, nullptr);
		if (NETP_LIKELY(rt == 0)) {
			NETP_ASSERT(_nbytes >= 0);
			WSACMSGHDR* pCMsgHdr = WSA_CMSG_FIRSTHDR(&_msg);
			NETP_ASSERT(pCMsgHdr != nullptr);
			if (pCMsgHdr->cmsg_type == IP_PKTINFO) {
				IN_PKTINFO* pPktInfo = (IN_PKTINFO*)WSA_CMSG_DATA(pCMsgHdr);
				if (pPktInfo != nullptr) {
					lipv4 = netp::nipv4toipv4({pPktInfo->ipi_addr.s_addr});
				}
			}
			return _nbytes;
		}

		NETP_ASSERT(rt == NETP_SOCKET_ERROR);
		const int ec = netp_socket_get_last_errno();
		//NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvmsg, ERROR: %d", fd, ec);
		if (ec == netp::E_EINTR) {
			goto _recvmsg;
		} else {
			ec_o = ec;
		}
		(void)flag;
		return 0;
	}

#elif defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
#if defined IP_RECVDSTADDR
# define DSTADDR_SOCKOPT IP_RECVDSTADDR
# define DSTADDR_DATASIZE (CMSG_SPACE(sizeof(struct in_addr)))

# define dstaddr(x) (CMSG_DATA(x))
#elif defined IP_PKTINFO
# define DSTADDR_SOCKOPT IP_PKTINFO
# define DSTADDR_DATASIZE (CMSG_SPACE(sizeof(struct in_pktinfo)))

# define dstaddr(x) (&(((struct in_pktinfo *)(CMSG_DATA(x)))->ipi_addr))
#else
# error "can't determine socket option"
#endif
	union control_data {
		struct cmsghdr	cmsg;
		u_char	data[DSTADDR_DATASIZE];
	};

	inline netp::u32_t __recvonemsg(SOCKET fd, byte_t* const buff_o, const netp::u32_t bsize, NRP<address>& raddr, ipv4_t& lipv4, int& ec_o, int flag) {
		NETP_ASSERT(fd != NETP_INVALID_SOCKET);
	_recvmsg:
		struct iovec iov[1] = { {buff_o,bsize} };
		union control_data cmsg;
		raddr = netp::make_ref<netp::address>();
		struct msghdr msg = {
			raddr->sockaddr_v4(),
			sizeof(sockaddr_in),
			iov,
			msg.msg_iovlen = 1,
			&cmsg,
			sizeof(cmsg),
			0
		};

		::ssize_t nbytes = recvmsg(fd, &msg, 0);
		if (NETP_LIKELY(nbytes >= 0)) {
			for (struct cmsghdr* cmsgptr = CMSG_FIRSTHDR(&msg); cmsgptr != nullptr; cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
				if (cmsgptr->cmsg_level == IPPROTO_IP && cmsgptr->cmsg_type == DSTADDR_SOCKOPT) {
					struct in_addr* _dstaddr = (struct in_addr*)dstaddr((cmsgptr));
					lipv4 = { netp::nipv4toipv4({_dstaddr->s_addr}) };
				}
			}

			if ( raddr && !raddr->is_af_unspec() && lipv4.u32 != 0) {
				ec_o = netp::OK;
				return nbytes;
			}
			else {
				ec_o = netp::E_UNKNOWN;
				return 0;
			}
		}

		NETP_ASSERT(nbytes == NETP_SOCKET_ERROR);
		int ec = netp_socket_get_last_errno();
		NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvmsg, ERROR: %d", fd, ec);
		_NETP_REFIX_EWOULDBLOCK(ec);
		if (ec == netp::E_EINTR) {
			goto _recvmsg;
		} else {
			ec_o = ec;
		}
		(void)flag;
		return 0;
	}
#endif


	inline SOCKET open(int family, int type, int protocol) {
#ifdef _NETP_WIN
		return WSASocket(family, type, NETP_PROTO_MAP_OS_PROTO[protocol], NULL,0,WSA_FLAG_OVERLAPPED);
#else
		return ::socket(family, type, NETP_PROTO_MAP_OS_PROTO[protocol]);
#endif
	}

	inline int close(SOCKET fd) {
		return NETP_CLOSE_SOCKET(fd);
	}

	inline int connect(SOCKET fd, NRP<address> const& addr) {
		return ::connect(fd, (const struct sockaddr*)(addr->sockaddr_v4()), sizeof(struct sockaddr_in));
	}

	inline int bind(SOCKET fd, NRP<address> const& addr) {
		return ::bind(fd, (const struct sockaddr*)(addr->sockaddr_v4()), sizeof(struct sockaddr_in));
	}

	inline int shutdown(SOCKET fd, int flag) {
		return  ::shutdown(fd, flag);
	}

	inline int listen( SOCKET fd, int backlog) {
		return ::listen(fd, backlog);
	}

	inline SOCKET accept(SOCKET fd, NRP<address>& from) {
		socklen_t len = sizeof(struct sockaddr_in);
		from = netp::make_ref<address>();
		::memset((void*)from->sockaddr_v4(), 0, sizeof(struct sockaddr_in));
		SOCKET accepted_fd = ::accept(fd, from->sockaddr_v4(), &len);
		NETP_RETURN_V_IF_MATCH((SOCKET)NETP_SOCKET_ERROR, (accepted_fd == (SOCKET)NETP_INVALID_SOCKET));
		return accepted_fd;
	}

	inline int getsockname(SOCKET fd, NRP<address>& addr) {
		socklen_t len = sizeof(struct sockaddr_in);
		addr = netp::make_ref<address>();
		::memset((void*)addr->sockaddr_v4(), 0, sizeof(struct sockaddr_in));
		int rt = ::getsockname(fd, addr->sockaddr_v4(), &len);
		NETP_RETURN_V_IF_MATCH(rt, rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	inline int getpeername(SOCKET fd, NRP<address>& addr) {
		socklen_t len = sizeof(struct sockaddr_in);
		addr = netp::make_ref<address>();
		::memset((void*)addr->sockaddr_v4(), 0, sizeof(struct sockaddr_in));
		int rt = ::getpeername(fd, (struct sockaddr*)(addr->sockaddr_v4()), &len);
		NETP_RETURN_V_IF_MATCH(rt, rt == NETP_SOCKET_ERROR);
		return netp::OK;
	}

	inline int getsockopt(SOCKET fd, int level, int option_name, void* value, socklen_t* option_len) {
#ifdef _NETP_WIN
		return ::getsockopt(fd, level, option_name, (char*)value, option_len);
#else
		return ::getsockopt(fd, level, option_name, value, option_len);
#endif
	}
	inline int setsockopt(SOCKET fd, int level, int option_name, void const* value, socklen_t const& option_len) {
#ifdef _NETP_WIN
		return ::setsockopt(fd, level, option_name, (char*)value, option_len);
#else
		return ::setsockopt(fd, level, option_name, value, option_len);
#endif
	}

	//@note: fcntl return -1 if failed, ioctlsocket return SOCKET_ERROR(-1) if failed
	inline int set_nonblocking(SOCKET fd, bool onoff) {
		int rt;
#if defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		int mode = ::fcntl(fd, F_GETFL, 0);
		if (onoff) {
			mode |= O_NONBLOCK;
		} else {
			mode &= ~O_NONBLOCK;
		}
		rt = ::fcntl(fd, F_SETFL, mode);
#else
		ULONG nonBlocking = onoff ? 1 : 0;
		rt = ::ioctlsocket(fd, FIONBIO, &nonBlocking);
#endif
		return rt;
	}

	inline int set_keepalive(SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return netp::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
	}

	inline int set_reuseaddr(SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return netp::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	}

	inline int set_reuseport(SOCKET fd, bool onoff) {
#if defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		int optval = onoff ? 1 : 0;
		return netp::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
#else
		(void)fd;
		(void)onoff;
		return netp::OK;
#endif
	}
	inline int set_broadcast(SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return netp::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
	}

	inline int set_nodelay(SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return netp::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
	}

	//caller decide to retry or not
	inline int send( SOCKET fd, byte_t const* const buf, netp::u32_t len, int flag = 0) {
		//for a connected udp session, zero-len is allowed
__label_send:
		const int r = ::send(fd, reinterpret_cast<const char*>(buf), (int)(len), flag);
		if (NETP_UNLIKELY(r == -1)) {
			int ec = netp_socket_get_last_errno();
			if (NETP_UNLIKELY(ec == netp::E_EINTR)) {
				goto __label_send;
			}
			//NETP_TRACE_SOCKET_API("[netp::send][#%d]send failed: %d", fd, ec);
			_NETP_REFIX_EWOULDBLOCK(ec);
			return ec;
		}
		return r;
	}

	//@note: 
	//Datagram sockets in various domains(e.g., the UNIXand Internet
	//	domains) permit zero - length datagrams.When such a datagram is
	//@return nbytes received, otherwise the error code
	//@prototype of posix use ssize_t for the len parameter, but windows use int
	inline int recv(SOCKET fd, byte_t* const buffer_o, netp::u32_t size, int flag =0) {
__label_recv:
		const int r = ::recv(fd, reinterpret_cast<char*>(buffer_o), (int)(size), flag);
		if (NETP_UNLIKELY(r == -1)) {
			int ec = netp_socket_get_last_errno();
			if (NETP_UNLIKELY(ec == netp::E_EINTR)) {
				goto __label_recv;
			}
			_NETP_REFIX_EWOULDBLOCK(ec);
			return ec;
			//NETP_TRACE_SOCKET_API("[netp::recv][#%d]recv: %d", fd, ec);
		}

		//note
		//(1) one shot one recv to give a zero-len udp pkt a chance to the caller, or we'll risk to miss this zero-len udp pkt for the following case
		//		a, zero-len udp pkt arrive
		//		b, we fetch from kernel by recv
		//		c, next recv return E_AGAIN
		//		d, return the call with R=0&&ec_o==E_AGAIN

		//(2) a break is a must to distinguish udp-pkts, one shot(recv) one pkt
		//(3) optimization: the caller decide to retry or not
		return r;
	}

	//@NOTE: udp allow zero-len pkt
	//@return nbytes sent, otherwise the error code
	//@prototype of posix use ssize_t for the len parameter, but windows use int
	inline int sendto(SOCKET fd, netp::byte_t const* const buf, netp::u32_t len, NRP<address> const& addr_to, int flag = 0) {
	_label_sendto:
		int nbytes;
		if (addr_to != nullptr) {
			struct sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in));
			addr_in.sin_family = u16_t(addr_to->family());
			addr_in.sin_port = addr_to->nport();
			addr_in.sin_addr.s_addr = addr_to->nipv4().u32;
			nbytes = ::sendto(fd, reinterpret_cast<const char*>(buf), (int)len, flag, reinterpret_cast<struct sockaddr*>(&addr_in), sizeof(addr_in));
		} else {
			nbytes = ::sendto(fd, reinterpret_cast<const char*>(buf), (int)len, flag, NULL, 0);
		}

		if ( NETP_UNLIKELY(nbytes == -1) ) {
			int ec = netp_socket_get_last_errno();
			//NETP_TRACE_SOCKET_API("[netp::sendto][#%d]send failed, error code: %d", fd, ec);
			if (ec == netp::E_EINTR) {
				goto _label_sendto;
			}
			_NETP_REFIX_EWOULDBLOCK(ec);
			return ec;
		}
		//sometimes, we got nbytes != len (noticed on rpi )
		//NETP_TRACE_SOCKET_API("[netp::sendto][#%d]sendto() == %d", fd, nbytes);
		return nbytes;
	}

	inline int recvfrom( SOCKET fd, byte_t* const buff_o, netp::u32_t size, NRP<address>& addr_o, int flag = 0) {
	_label_recvfrom:
		int nbytes;
		if (addr_o != nullptr) {
			::memset((void*)addr_o->sockaddr_v4(), 0, sizeof(struct sockaddr_in));
			socklen_t socklen = sizeof(struct sockaddr_in);
			nbytes = ::recvfrom(fd, reinterpret_cast<char*>(buff_o), (int)size, flag, addr_o->sockaddr_v4(), &socklen);
		} else {
			nbytes = ::recvfrom(fd, reinterpret_cast<char*>(buff_o), (int)size, flag, NULL, NULL);
		}

		if (NETP_UNLIKELY(nbytes == -1)) {
			int ec = netp_socket_get_last_errno();
			//NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvfrom, ERROR: %d", fd, ec);
			if (ec == netp::E_EINTR) {
				goto _label_recvfrom;
			}
			_NETP_REFIX_EWOULDBLOCK(ec);
			return ec;
		}
		return nbytes;
	}

	inline int socketpair(int domain, int type, int protocol, SOCKET sv[2]) {
		if (domain != int(NETP_AF_INET)) {
			return NETP_SOCKET_ERROR;
		}

		if (type == int(NETP_SOCK_DGRAM)) {
			if (protocol != (NETP_PROTOCOL_UDP)) {
				netp_socket_set_last_errno(netp::E_INVAL);
				return NETP_SOCKET_ERROR;
			}
		}

		NETP_ASSERT((protocol == NETP_PROTOCOL_TCP) || protocol == NETP_PROTOCOL_UDP);

		SOCKET listenfd = ::socket(domain, type, NETP_PROTO_MAP_OS_PROTO[protocol]);
		if (listenfd == NETP_INVALID_SOCKET) {
			//NETP_INFO("[socketpair]make listener failed: %d", netp_socket_get_last_errno());
			return NETP_SOCKET_ERROR;
		}

		struct sockaddr_in addr_listen;
		struct sockaddr_in addr_connect;
		struct sockaddr_in addr_accept;
		::memset(&addr_listen, 0, sizeof(addr_listen));

		SOCKET connectfd;
		SOCKET acceptfd;
		socklen_t socklen = sizeof(addr_connect);

		addr_listen.sin_family = AF_INET;
		addr_listen.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr_listen.sin_port = 0;
		int rt = ::bind(listenfd, reinterpret_cast<sockaddr*>(&addr_listen), sizeof(addr_listen));
		if (rt != netp::OK) {
			goto end;
		}
		rt = ::listen(listenfd, 1);
		if (rt != netp::OK) {
			goto end;
		}
		rt = ::getsockname(listenfd, reinterpret_cast<sockaddr*>(&addr_connect), &socklen);
		if (rt != netp::OK) {
			goto end;
		}

		connectfd = ::socket(domain, type, NETP_PROTO_MAP_OS_PROTO[protocol]);
		if (connectfd == NETP_INVALID_SOCKET) {
			goto end;
		}
		rt = ::connect(connectfd, reinterpret_cast<sockaddr*>(&addr_connect), sizeof(addr_connect));
		if (rt != netp::OK) {
			goto end;
		}

		acceptfd = ::accept(listenfd, reinterpret_cast<sockaddr*>(&addr_accept), &socklen);
		if (acceptfd == NETP_INVALID_SOCKET) {
			goto end;
		}

		sv[0] = acceptfd;
		sv[1] = connectfd;
	end:
		//NETP_INFO("[socketpair]make socketpair status: %d", netp_socket_get_last_errno() );
		netp::close(listenfd);
		return rt;
	}
}
#endif