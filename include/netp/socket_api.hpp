 #ifndef NETP_NET_SOCKET_API_HPP
#define NETP_NET_SOCKET_API_HPP

#include <netp/core.hpp>
#include <netp/address.hpp>
#include <netp/logger_broker.hpp>

#ifdef _NETP_WIN
	#include <netp/os/winsock_helper.hpp>
	#define IS_ERRNO_EQUAL_WOULDBLOCK(_errno) ((_errno==netp::E_EWOULDBLOCK)||(_errno==netp::E_WSAEWOULDBLOCK))
	#define WINAPI __stdcall 
#else
	#define IS_ERRNO_EQUAL_WOULDBLOCK(_errno) ((_errno==netp::E_EWOULDBLOCK)||(_errno==netp::E_EAGAIN))
#endif

#define IS_ERRNO_EQUAL_CONNECTING(_errno) ((_errno==netp::E_EINPROGRESS)||(_errno==netp::E_WSAEWOULDBLOCK))

namespace netp {

	typedef SOCKET (*fn_socket)(int family, int type, int proto);
	typedef int(*fn_connect)(SOCKET fd, const struct sockaddr* sockaddr, socklen_t len);
		
	typedef int(*fn_bind)(SOCKET fd, const struct sockaddr* addr, socklen_t len);
	typedef int(*fn_shutdown)(SOCKET fd, int flag);
	typedef int(*fn_close)(SOCKET fd);
	typedef int(*fn_listen)(SOCKET fd, int backlog);
	typedef SOCKET(*fn_accept)(SOCKET fd, struct sockaddr* addr, socklen_t* addrlen);
	typedef int(*fn_getsockname)(SOCKET fd, struct sockaddr* addr, socklen_t* addrlen);
	typedef int(*fn_getpeername)(SOCKET fd, struct sockaddr* addr, socklen_t* addrlen);

	typedef int(*fn_getsockopt)(SOCKET fd, int level, int option_name, void* value, socklen_t* option_len);
	typedef int(*fn_setsockopt)(SOCKET fd, int level, int option_name, void const* value, socklen_t option_len);

	typedef int (*fn_send)(SOCKET fd, char const* const buf, u32_t len, int flags);
	typedef int (*fn_recv)(SOCKET fd, char* const buf, u32_t size, int flags);
	typedef int (*fn_sendto)(SOCKET fd, char const* buf, u32_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);
	typedef int (*fn_recvfrom)(SOCKET fd, char* buff_o, u32_t size, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
	typedef u32_t (*fn_recvonemsg)(SOCKET fd, byte_t* const buf_o, const u32_t bsize, address& raddr, ipv4_t& lipv4, int& ec_o, int flag);

	typedef int (*fn_set_nonblocking)(SOCKET fd, bool onoff);

	struct socket_api {
		fn_socket socket;	
		fn_bind bind;
		fn_connect connect;
		fn_listen listen;
		fn_accept accept;
		fn_shutdown shutdown;
		fn_close close;
		fn_getsockname getsockname;
		fn_getpeername getpeername;
		fn_getsockopt getsockopt;
		fn_setsockopt setsockopt;
		fn_send send;
		fn_sendto sendto;
		fn_recv recv;
		fn_recvfrom recvfrom;
		fn_recvonemsg recvonemsg;
		fn_set_nonblocking set_nonblocking;
	};

	inline int netp_close(SOCKET fd) { return NETP_CLOSE_SOCKET(fd); }

#ifdef NETP_IO_MODE_IOCP
	namespace iocp {
		inline SOCKET socket(int const& family, int const& type, int const& proto) {
			return ::WSASocketW(OS_DEF_family[family], OS_DEF_sock_type[type], OS_DEF_protocol[proto], nullptr, 0, WSA_FLAG_OVERLAPPED);
		}
	}
#endif

	inline int set_nonblocking(SOCKET fd, bool onoff) {
		int rt;
#if defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID)
		int mode = ::fcntl(fd, F_GETFL, 0);
		if (onoff) {
			mode |= O_NONBLOCK;
		}
		else {
			mode &= ~O_NONBLOCK;
		}
		rt = ::fcntl(fd, F_SETFL, mode);
#else
		ULONG nonBlocking = onoff ? 1 : 0;
		rt = ::ioctlsocket(fd, FIONBIO, &nonBlocking);
#endif
		return rt;
	}

#ifdef _NETP_WIN
	inline netp::u32_t recvonemsg(SOCKET fd, byte_t* const buff_o, netp::u32_t const bsize, address& raddr, ipv4_t& lipv4, int& ec_o, int flag) {
		const static LPFN_WSARECVMSG __fn_wsa_recvmsg = (LPFN_WSARECVMSG)netp::os::load_api_ex_address(netp::os::winsock_api_ex::API_RECVMSG);
		NETP_ASSERT(__fn_wsa_recvmsg != nullptr);

	_recvmsg:
		char ctl_buffer[1024] = { 0 };
		DWORD _nbytes = 0;
		sockaddr_in sockaddr_remote;
		WSABUF wsa_buf_data = { bsize, (char*)buff_o };
		WSAMSG _msg = {
			(sockaddr*)& sockaddr_remote,
			sizeof(sockaddr_remote),
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
					lipv4 = netp::nipv4toipv4(pPktInfo->ipi_addr.s_addr);
				}
			}
			raddr = netp::address(sockaddr_remote);
			return _nbytes;
		}

		NETP_ASSERT(rt == NETP_SOCKET_ERROR);
		const int ec = netp_socket_get_last_errno();
		NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvmsg, ERROR: %d", fd, ec);
		if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
			ec_o = netp::E_SOCKET_READ_BLOCK;
		}
		else if (ec == netp::E_EINTR) {
			goto _recvmsg;
		}
		else {
			ec_o = ec;
		}
		(void)flag;
		return 0;
	}

	namespace winapi {
		__NETP_FORCE_INLINE SOCKET socket(
			int af,
			int type,
			int protocol
		) {
			return ::socket(af, type, protocol);
		}

		__NETP_FORCE_INLINE int bind(
			SOCKET         s,
			const sockaddr* name,
			int            namelen
		) {
			return ::bind(s, name, namelen);
		}

		__NETP_FORCE_INLINE int connect(
			SOCKET         s,
			const sockaddr* name,
			int            namelen
		) {
			return ::connect(s, name, namelen);
		}

		__NETP_FORCE_INLINE int listen(
			SOCKET s,
			int    backlog
		) {
			return ::listen(s, backlog);
		}

		__NETP_FORCE_INLINE SOCKET accept(
			SOCKET   s,
			sockaddr* addr,
			int* addrlen
		) {
			return ::accept(s, addr, addrlen);
		}

		__NETP_FORCE_INLINE int shutdown(
			SOCKET s,
			int    how
		) {
			return ::shutdown(s, how);
		}

		__NETP_FORCE_INLINE int getsockname(
			SOCKET   s,
			sockaddr* name,
			int* namelen
		) {
			return ::getsockname(s, name, namelen);
		}

		__NETP_FORCE_INLINE int getpeername(
			SOCKET   s,
			sockaddr* name,
			int* namelen
		) {
			return ::getpeername(s, name, namelen);
		}

		__NETP_FORCE_INLINE int getsockopt(
			SOCKET s,
			int    level,
			int    optname,
			char* optval,
			int* optlen
		) {
			return ::getsockopt(s, level, optname, optval, optlen);
		}

		__NETP_FORCE_INLINE int setsockopt(
			SOCKET     s,
			int        level,
			int        optname,
			const char* optval,
			int        optlen
		) {
			return ::setsockopt(s, level, optname, optval, optlen);
		}

		__NETP_FORCE_INLINE int sendto(
			SOCKET         s,
			const char* buf,
			int            len,
			int            flags,
			const sockaddr* to,
			int            tolen
		) {
			return ::sendto(s, buf, len, flags, to, tolen);
		}

		__NETP_FORCE_INLINE int send(
			SOCKET     s,
			const char* buf,
			int        len,
			int        flags
		) {
			return ::send(s, buf, len, flags);
		}

		__NETP_FORCE_INLINE int recv(
			SOCKET s,
			char* buf,
			int    len,
			int    flags
		) {
			return ::recv(s, buf, len, flags);
		}

		__NETP_FORCE_INLINE int recvfrom(
			SOCKET   s,
			char* buf,
			int      len,
			int      flags,
			sockaddr* from,
			int* fromlen
		) {
			return ::recvfrom(s, buf, len, flags, from, fromlen);
		}
	}

#elif defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID)
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

	inline netp::u32_t recvonemsg(SOCKET fd, byte_t* const buff_o, const netp::u32_t bsize, address& raddr, ipv4_t& lipv4, int& ec_o, int flag) {
		NETP_ASSERT(fd != NETP_INVALID_SOCKET);
	_recvmsg:
		struct iovec iov[1] = { {buff_o,bsize} };
		sockaddr_in sockaddr_remote;
		union control_data cmsg;

		struct msghdr msg = {
			&sockaddr_remote,
			sizeof(sockaddr_remote),
			iov,
			msg.msg_iovlen = 1,
			&cmsg,
			sizeof(cmsg),
			0
		};

		::ssize_t nbytes = recvmsg(fd, &msg, 0);
		if (NETP_LIKELY(nbytes >= 0)) {
			raddr = netp::address(sockaddr_remote);
			for (struct cmsghdr* cmsgptr = CMSG_FIRSTHDR(&msg); cmsgptr != nullptr; cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
				if (cmsgptr->cmsg_level == IPPROTO_IP && cmsgptr->cmsg_type == DSTADDR_SOCKOPT) {
					struct in_addr* _dstaddr = (struct in_addr*)dstaddr((cmsgptr));
					lipv4 = netp::nipv4toipv4(_dstaddr->s_addr);
				}
			}

			if (!raddr.is_null() && lipv4 != 0) {
				ec_o = netp::OK;
				return nbytes;
			}
			else {
				ec_o = netp::E_UNKNOWN;
				return 0;
			}
		}

		NETP_ASSERT(nbytes == NETP_SOCKET_ERROR);
		const int ec = netp_socket_get_last_errno();
		NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvmsg, ERROR: %d", fd, ec);
		if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
			ec_o = netp::E_SOCKET_READ_BLOCK;
		}
		else if (ec == netp::E_EINTR) {
			goto _recvmsg;
		}
		else {
			ec_o = ec;
		}
		(void)flag;
		return 0;
	}
#endif

#ifdef _NETP_WIN
#define __SOCKET_API_NS winapi
#else 
	#define __SOCKET_API_NS
#endif
	const socket_api NETP_DEFAULT_SOCKAPI = {
			__SOCKET_API_NS::socket,
			__SOCKET_API_NS::bind,
			__SOCKET_API_NS::connect,
			__SOCKET_API_NS::listen,
			__SOCKET_API_NS::accept,
			__SOCKET_API_NS::shutdown,
			(fn_close) netp_close,
			(fn_getsockname)__SOCKET_API_NS::getsockname,
			(fn_getpeername)__SOCKET_API_NS::getpeername,
			(fn_getsockopt)__SOCKET_API_NS::getsockopt,
			(fn_setsockopt)__SOCKET_API_NS::setsockopt,
			(fn_send)__SOCKET_API_NS::send,
			(fn_sendto)__SOCKET_API_NS::sendto,
			(fn_recv)__SOCKET_API_NS::recv,
			(fn_recvfrom)__SOCKET_API_NS::recvfrom,
			(fn_recvonemsg)recvonemsg,
			(fn_set_nonblocking)set_nonblocking
	};
	
	inline SOCKET open(socket_api const& fn, int family, int type, int protocol) {
		return fn.socket(family, type, OS_DEF_protocol[protocol]);
	}

	inline int connect(socket_api const& fn,SOCKET fd, address const& addr) {
		sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));
		addr_in.sin_family = u16_t(addr.family());
		addr_in.sin_port = addr.nport();
		addr_in.sin_addr.s_addr = addr.nipv4();
		return fn.connect(fd, (sockaddr*)(&addr_in), sizeof(addr_in));
	}

	inline int bind(socket_api const& fn,SOCKET fd, address const& addr) {
		sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));
		addr_in.sin_family = u16_t(addr.family());
		addr_in.sin_port = addr.nport();
		addr_in.sin_addr.s_addr = addr.nipv4();
		return fn.bind(fd, (sockaddr*)(&addr_in), sizeof(addr_in));
	}

	inline int shutdown(socket_api const& fn, SOCKET fd, int flag) {
		return  fn.shutdown(fd, flag);
	}

	inline int close(socket_api const& fn, SOCKET fd) {
		return fn.close(fd);
	}

	inline int listen(socket_api const& fn, SOCKET fd, int backlog) {
		return fn.listen(fd, backlog);
	}

	inline SOCKET accept(socket_api const& fn, SOCKET fd, address& addr) {
		sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));
		socklen_t len = sizeof(addr_in);

		SOCKET accepted_fd = fn.accept(fd, (sockaddr*)(&addr_in), &len);
		NETP_RETURN_V_IF_MATCH((SOCKET)NETP_SOCKET_ERROR, (accepted_fd == (SOCKET)NETP_INVALID_SOCKET));
		addr = address(addr_in);
		return accepted_fd;
	}

	inline int getsockname(socket_api const& fn,SOCKET fd, address& addr) {
		sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));

		socklen_t len = sizeof(addr_in);
		int rt = fn.getsockname(fd, (sockaddr*)(&addr_in), &len);
		NETP_RETURN_V_IF_MATCH(rt, rt == NETP_SOCKET_ERROR);
		addr = address(addr_in);
		return netp::OK;
	}

	inline int getpeername(socket_api const& fn, SOCKET fd, address& addr) {
		sockaddr_in addr_peer;
		::memset(&addr_peer, 0, sizeof(addr_peer));
		socklen_t len = sizeof(addr_peer);
		int rt = fn.getpeername(fd, (sockaddr*)(&addr_peer), &len);
		NETP_RETURN_V_IF_MATCH(rt, rt == NETP_SOCKET_ERROR);
		addr = address(addr_peer);
		return netp::OK;
	}

	inline netp::u32_t send(socket_api const& fn, SOCKET fd, byte_t const* const buf, netp::u32_t len, int& ec_o, int flag) {
		NETP_ASSERT(buf != nullptr);
		NETP_ASSERT(len > 0);

		netp::u32_t R = 0;

		//TRY SEND
		do {
			const int r = fn.send(fd, reinterpret_cast<const char*>(buf) + R, (int)(len - R), flag);
			if (NETP_LIKELY(r > 0)) {
				ec_o = netp::OK;
				R += r;
				if (R == len) {
					break;
				}
			}
			else {
				NETP_ASSERT(r == -1);
				int ec = netp_socket_get_last_errno();
				if (NETP_LIKELY(IS_ERRNO_EQUAL_WOULDBLOCK(ec))) {
					ec_o = netp::E_SOCKET_WRITE_BLOCK;
					break;
				}
				else if (NETP_UNLIKELY(ec == netp::E_EINTR)) {
					continue;
				}
				else {
					NETP_TRACE_SOCKET_API("[netp::send][#%d]send failed: %d", fd, ec);
					ec_o = ec;
					break;
				}
			}
		} while (true);

		NETP_TRACE_SOCKET_API("[netp::send][#%d]send, to send: %d, sent: %d, ec: %d", fd, len, R, ec_o);
		return R;
	}

	inline netp::u32_t recv(socket_api const& fn, SOCKET fd, byte_t* const buffer_o, netp::u32_t size, int& ec_o, int flag) {
		NETP_ASSERT(buffer_o != nullptr);
		NETP_ASSERT(size > 0);

		netp::u32_t R = 0;
		do {
			const int r = fn.recv(fd, reinterpret_cast<char*>(buffer_o) + R, (int)(size - R), flag);
			if (NETP_LIKELY(r > 0)) {
				R += r;
				ec_o = netp::OK;
				break;
			}
			else if (r == 0) {
				NETP_TRACE_SOCKET_API("[netp::recv][#%d]socket closed by remote side gracefully[detected by recv]", fd);
				ec_o = netp::E_SOCKET_GRACE_CLOSE;
				break;
			}
			else {
				NETP_ASSERT(r == -1);
				const int ec = netp_socket_get_last_errno();

				if (NETP_LIKELY(IS_ERRNO_EQUAL_WOULDBLOCK(ec))) {
					ec_o = netp::E_SOCKET_READ_BLOCK;
					break;
				}
				else if (NETP_UNLIKELY(ec == netp::E_EINTR)) {
					continue;
				}
				else {
					NETP_ASSERT(ec != netp::OK);
					ec_o = ec;
					NETP_TRACE_SOCKET_API("[netp::recv][#%d]recv: %d", fd, ec);
					break;
				}
			}
		} while (true);

		NETP_TRACE_SOCKET_API("[netp::recv][#%d]recv bytes, %u, ec: %d", fd, R, ec_o);
		NETP_ASSERT(R <= size);
		return R;
	}

	inline netp::u32_t sendto(socket_api const& fn, SOCKET fd, netp::byte_t const* const buff, netp::u32_t len, address const& addr, int& ec_o, int const& flag) {

		NETP_ASSERT(buff != nullptr);
		NETP_ASSERT(len > 0);
		NETP_ASSERT(!addr.is_null());

		sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));
		addr_in.sin_family = u16_t(addr.family());
		addr_in.sin_port = addr.nport();
		addr_in.sin_addr.s_addr = addr.nipv4();
sendto:
		const int nbytes = fn.sendto(fd, reinterpret_cast<const char*>(buff), (int)len, flag, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));

		if (NETP_LIKELY(nbytes > 0)) {
			NETP_ASSERT((u32_t)nbytes == len);
			ec_o = netp::OK;
			NETP_TRACE_SOCKET_API("[netp::sendto][#%d]sendto() == %d", fd, nbytes);
			return nbytes;
		}

		NETP_ASSERT(nbytes == -1);
		const int ec = netp_socket_get_last_errno();
		NETP_TRACE_SOCKET_API("[netp::sendto][#%d]send failed, error code: %d", fd, ec);
		if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
			ec_o = netp::E_SOCKET_WRITE_BLOCK;
		} else if (ec == netp::E_EINTR) {
			goto sendto;
		} else {
			ec_o = ec;
		}
		return 0;
	}

	inline netp::u32_t recvfrom(socket_api const& api, SOCKET fd, byte_t* const buff_o, netp::u32_t size, address& addr_o, int& ec_o, int const& flag) {
recvfrom:
		sockaddr_in addr_in;
		::memset(&addr_in, 0, sizeof(addr_in));
		socklen_t socklen = sizeof(addr_in);
		const int nbytes = api.recvfrom(fd, reinterpret_cast<char*>(buff_o), (int)size, flag, reinterpret_cast<sockaddr*>(&addr_in), &socklen);

		if (NETP_LIKELY(nbytes > 0)) {
			addr_o = address(addr_in);
			ec_o = netp::OK;
			NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvfrom() == %d", fd, nbytes);
			return nbytes;
		}

		NETP_ASSERT(nbytes == -1);
		const int ec = netp_socket_get_last_errno();
		NETP_TRACE_SOCKET_API("[netp::recvfrom][#%d]recvfrom, ERROR: %d", fd, ec);
		if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
			ec_o = netp::E_SOCKET_READ_BLOCK;
		} else if (ec == netp::E_EINTR) {
			goto recvfrom;
		} else {
			ec_o = ec;
		}
		return 0;
	}

	inline int set_keepalive(socket_api const& api, SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return api.setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
	}

	inline int set_reuseaddr(socket_api const& api, SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return api.setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	}

	inline int set_reuseport(socket_api const& api, SOCKET fd, bool onoff) {
#ifdef _NETP_GNU_LINUX
		int optval = onoff ? 1 : 0;
		return fn.setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
#else
		(void)api;
		(void)fd;
		(void)onoff;
		return netp::OK;
#endif
	}
	inline int set_broadcast(socket_api const& api, SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return api.setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
	}

	inline int turnon_nonblocking(socket_api const& api, SOCKET fd) {
		return api.set_nonblocking(fd, true);
	}
	inline int turnoff_nonblocking(socket_api const& api,SOCKET fd) {
		return api.set_nonblocking(fd, false);
	}

	inline int set_nodelay(socket_api const& api,SOCKET fd, bool onoff) {
		int optval = onoff ? 1 : 0;
		return api.setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
	}
	inline int turnon_nodelay(socket_api const& api, SOCKET fd) {
		return set_nodelay(api,fd, true);
	}
	inline int turnoff_nodelay(socket_api const& api, SOCKET fd) {
		return set_nodelay(api,fd, false);
	}

	inline int socketpair(int domain, int type, int protocol, SOCKET sv[2]) {
		if (domain != int(NETP_AF_INET)) {
			return NETP_SOCKET_ERROR;
		}

		if (type == int(NETP_SOCK_DGRAM)) {
			if (protocol != (NETP_PROTOCOL_UDP)) {
				netp_set_last_errno(netp::E_INVAL);
				return NETP_SOCKET_ERROR;
			}
		}

		NETP_ASSERT((protocol == NETP_PROTOCOL_TCP) || protocol == NETP_PROTOCOL_UDP);

		SOCKET listenfd = ::socket(domain, type, OS_DEF_protocol[protocol]);
		if (listenfd == NETP_INVALID_SOCKET) {
			NETP_INFO("[socketpair]make listener failed: %d", netp_socket_get_last_errno());
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
		addr_listen.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
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

		connectfd = ::socket(domain, type, OS_DEF_protocol[protocol]);
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
		NETP_CLOSE_SOCKET(listenfd);
		return rt;
	}
}
#endif