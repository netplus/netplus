#ifndef _NETP_SOCKET_CH_HPP_
#define _NETP_SOCKET_CH_HPP_

#include <queue>

#include <netp/smart_ptr.hpp>
#include <netp/string.hpp>
#include <netp/packet.hpp>
#include <netp/address.hpp>

#include <netp/socket_api.hpp>
#include <netp/channel.hpp>
#include <netp/app.hpp>

//@NOTE: turn on this option would result in about 20% performance boost for EPOLL
#define NETP_ENABLE_FAST_WRITE

//in milliseconds, small clock would result in a more accurate control
#define NETP_DEFAULT_LISTEN_BACKLOG 256

namespace netp {

	enum socket_option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 1, //only for UDP
		OPTION_REUSEADDR = 1 << 1,
		OPTION_REUSEPORT = 1 << 2,
		OPTION_NON_BLOCKING = 1 << 3,
		OPTION_NODELAY = 1 << 4, //only for TCP
		OPTION_KEEP_ALIVE = 1 << 5,
		OPTION_NOCHECK = 1<<6
	};

	const static int default_socket_option = (int(socket_option::OPTION_NON_BLOCKING) | int(socket_option::OPTION_KEEP_ALIVE));

	struct keep_alive_vals {
		netp::u8_t	 probes;
		netp::u8_t	 idle; //in seconds
		netp::u8_t	 interval; //in seconds
	};

	const keep_alive_vals default_tcp_keep_alive_vals
	{
		(netp::u8_t)(NETP_DEFAULT_TCP_KEEPALIVE_IDLETIME),
		(netp::u8_t)(NETP_DEFAULT_TCP_KEEPALIVE_INTERVAL),
		(netp::u8_t)(NETP_DEFAULT_TCP_KEEPALIVE_PROBES)
	};

	struct socketinfo {
		SOCKET fd;
		u16_t f;
		u16_t t;
		u16_t p;

		NRP<address> laddr;
		NRP<address> raddr;

		netp::string_t to_string() const {
			char _buf[1024] = { 0 };
#ifdef _NETP_MSVC
			int nbytes = snprintf(_buf, 1024, "#%zu:%s:L:%s-R:%s", fd, NETP_PROTO_MAP_PROTO_STR[int(p)], laddr ? laddr->to_string().c_str():"", raddr?raddr->to_string().c_str():"");
#elif defined(_NETP_GCC)
			int nbytes = snprintf(_buf, 1024, "#%d:%s:L:%s-R:%s", fd, NETP_PROTO_MAP_PROTO_STR[int(p)], laddr ? laddr->to_string().c_str() : "", raddr ? raddr->to_string().c_str() : "");
#else
#error "unknown compiler"
#endif

			return netp::string_t(_buf, nbytes);
		}
	};

	class socket_cfg;
	typedef std::function<NRP<socket_channel>(NRP<socket_cfg> const& cfg)> fn_socket_channel_maker_t;
	class socket_cfg final :
		public ref_base
	{
	public:
		NRP<event_loop> L;
		SOCKET fd;
		u16_t family;
		u16_t type;
		u16_t proto;
		u16_t option;

		NRP<address> laddr;
		NRP<address> raddr;

		keep_alive_vals kvals;
		channel_buf_cfg sock_buf;
		u32_t tx_limit; //in Byte (1kb == 1024Byte), 0 means no limit
		u32_t wsabuf_size;

		fn_socket_channel_maker_t ch_maker;
		socket_cfg(NRP<event_loop> const& L = nullptr) :
			L(L),
			fd(SOCKET(NETP_INVALID_SOCKET)),
			family((NETP_AF_INET)),
			type(NETP_SOCK_STREAM),
			proto(NETP_PROTOCOL_TCP),
			option(default_socket_option),
			laddr(),
			raddr(),
			kvals(default_tcp_keep_alive_vals),
			sock_buf({ 0 }),
			tx_limit(0),
			wsabuf_size(64*1024),
			ch_maker(nullptr)
		{}

		NRP<socket_cfg> clone() const {
			NRP<socket_cfg> _cfg = netp::make_ref<socket_cfg>();
			_cfg->L = L;
			_cfg->fd = fd;
			_cfg->family = family; 
			_cfg->type = type;
			_cfg->proto = proto;
			_cfg->option = option; 
			_cfg->laddr = laddr;
			_cfg->raddr = raddr;
			_cfg->kvals = kvals;
			_cfg->sock_buf = sock_buf;
			_cfg->tx_limit = tx_limit;
			_cfg->wsabuf_size = wsabuf_size;
			_cfg->ch_maker = ch_maker;

			return _cfg;
		}
	};

	struct socket_outbound_entry final {
		u32_t written;
		NRP<netp::packet> data;
		NRP<promise<int>> write_promise;
	};
	struct socket_outbound_entry_to final {
		NRP<netp::packet> data;
		NRP<address> to;
		NRP<promise<int>> write_promise;
	};

	//@note: 1kb for delta checker
	#define _NETP_SOCKET_CHANNEL_LIMIT_MIN (1024)

	class socket_channel:
		public channel
	{
		friend void do_dial(NRP<channel_dial_promise> const& ch_dialf, NRP<address> const& addr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg);
		friend void do_listen_on(NRP<channel_listen_promise> const& listenp, NRP<address> const& laddr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog);
		typedef std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>> socket_outbound_entry_t;
		typedef std::deque<socket_outbound_entry_to, netp::allocator<socket_outbound_entry_to>> socket_outbound_entry_to_t;

		template <class _Ref_ty, typename... _Args>
		friend ref_ptr<_Ref_ty> make_ref(_Args&&... args);
	protected:
		SOCKET m_fd;
		u16_t m_family;
		u16_t m_type;
		u16_t m_protocol; //netp_proto 
		u16_t m_option;

		NRP<address> m_laddr;
		NRP<address> m_raddr;
		fn_io_event_t* m_fn_read;
		fn_io_event_t* m_fn_write;
		io_ctx* m_io_ctx;

		u32_t m_rcv_buf_size;
		u32_t m_snd_buf_size;
		u32_t m_tx_limit; //in byte
		u32_t m_tx_budget;
		u32_t m_tx_bytes;
		long long m_tx_limit_last_tp;

		//@note: for long term session, we should better release the q if necessary
		socket_outbound_entry_t m_tx_entry_q;
		socket_outbound_entry_to_t m_tx_entry_to_q;

		void _tmcb_tx_limit(NRP<timer> const& t);

		socket_channel(NRP<socket_cfg> const& cfg) :
			channel(cfg->L),
			m_fd(cfg->fd),
			m_family(cfg->family),
			m_type(cfg->type),
			m_protocol(cfg->proto),
			m_option(0),
			m_laddr(cfg->fd != NETP_INVALID_SOCKET ? cfg->laddr : nullptr),
			m_raddr(cfg->fd != NETP_INVALID_SOCKET ? cfg->raddr : nullptr),
			m_fn_read(nullptr),
			m_fn_write(nullptr),
			m_io_ctx(0),
			m_rcv_buf_size(0),
			m_snd_buf_size(0),
			m_tx_limit((cfg->tx_limit != 0 && cfg->tx_limit < _NETP_SOCKET_CHANNEL_LIMIT_MIN) ? _NETP_SOCKET_CHANNEL_LIMIT_MIN : cfg->tx_limit),
			m_tx_budget( (cfg->tx_limit != 0 && cfg->tx_limit < _NETP_SOCKET_CHANNEL_LIMIT_MIN) ? _NETP_SOCKET_CHANNEL_LIMIT_MIN : cfg->tx_limit ),
			m_tx_bytes(0),
			m_tx_limit_last_tp(0)
		{
			NETP_ASSERT(cfg->L != nullptr);
			if (cfg->fd != NETP_INVALID_SOCKET) {
				//@note: for unix_sock|pipe, there is no laddr&raddr
				NETP_ASSERT((m_laddr != nullptr) || (m_raddr !=nullptr) );
				m_chflag &= ~int(channel_flag::F_CLOSED);
			}
#ifdef _NETP_DEBUG
			else {
				NETP_ASSERT(m_laddr == nullptr);
				NETP_ASSERT(m_raddr == nullptr);
			}
#endif

		}

		~socket_channel()
		{
		}

		//default off
		int _cfg_nocheck(bool nocheckon) {
			NETP_ASSERT(is_udp());
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
			//bool setornot = ((m_option&netp::u16_t(socket_option::OPTION_NOCHECK)) && (!nocheckon)) ||
			//	(((m_option&netp::u16_t(socket_option::OPTION_NOCHECK)) == 0) && (nocheckon));

			//if (!setornot) {
			//	return netp::OK;
			//}
			int optval = nocheckon ? 1 : 0;
#if defined(_NETP_WIN)
			int rt = socket_setsockopt_impl(IPPROTO_UDP, UDP_NOCHECKSUM, &optval, sizeof(optval));
#elif defined(_NETP_GNU_LINUX)|| defined(_NETP_ANDROID) 
			int rt = socket_setsockopt_impl(SOL_SOCKET, SO_NO_CHECK, &optval, sizeof(optval));
#elif defined(_NETP_APPLE)
			int rt = socket_setsockopt_impl(IPPROTO_UDP, UDP_NOCKSUM, &optval, sizeof(optval));
#endif

			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			if (nocheckon) {
				m_option |= u16_t(socket_option::OPTION_NOCHECK);
			} else {
				m_option &= ~u16_t(socket_option::OPTION_NOCHECK);
			}
			return netp::OK;
		}

	public:
		//return 1 if nocheck on
		//return 0 if nocheck off
		//return <0 if any getsockopt error
		int is_nocheck_on() {
			NETP_ASSERT(is_udp());

			int rt;
			int optval = 0;
			socklen_t optlen = sizeof(int);
#if defined(_NETP_WIN)
			rt = socket_getsockopt_impl(IPPROTO_UDP, UDP_NOCHECKSUM, &optval, &optlen);
#elif defined(_NETP_GNU_LINUX)|| defined(_NETP_ANDROID)
			rt = socket_getsockopt_impl(SOL_SOCKET, SO_NO_CHECK, &optval, &optlen);
#elif defined(_NETP_APPLE)
			rt = socket_getsockopt_impl(IPPROTO_UDP, UDP_NOCKSUM, &optval, &optlen);
#endif
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			return (optval == 1) ? 1: 0;
		}

		int cfg_force_nocheck_off() {
			NETP_ASSERT(is_udp());

//			_cfg_nocheck(true);
//			NETP_ASSERT(is_nocheck_on());

			int onoff = is_nocheck_on();
			if (onoff == 0) {
				return netp::OK;
			}
			return _cfg_nocheck(false);
		}

protected:
		int _cfg_reuseaddr(bool onoff) {
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

			bool setornot = ((m_option & netp::u16_t(socket_option::OPTION_REUSEADDR)) && (!onoff)) ||
				(((m_option & netp::u16_t(socket_option::OPTION_REUSEADDR)) == 0) && (onoff));

			if (!setornot) {
				return netp::OK;
			}

			int optval = onoff ? 1 : 0;
			int rt = socket_setsockopt_impl(SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			if (onoff) {
				m_option |= u16_t(socket_option::OPTION_REUSEADDR);
			} else {
				m_option &= ~u16_t(socket_option::OPTION_REUSEADDR);
			}
			return netp::OK;
		}

		int _cfg_reuseport(bool onoff) {
			(void)onoff;

#if defined(_NETP_GNU_LINUX)|| defined(_NETP_ANDROID) || defined(_NETP_APPLE)
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

			bool setornot = ((m_option & u16_t(socket_option::OPTION_REUSEPORT)) && (!onoff)) ||
				(((m_option & u16_t(socket_option::OPTION_REUSEPORT)) == 0) && (onoff));

			if (!setornot) {
				return netp::OK;
			}

			int optval = onoff ? 1 : 0;
			int rt = socket_setsockopt_impl(SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
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

		int _cfg_nonblocking(bool onoff) {
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

			bool setornot = ((m_option & int(socket_option::OPTION_NON_BLOCKING)) && (!onoff)) ||
				(((m_option & int(socket_option::OPTION_NON_BLOCKING)) == 0) && (onoff));

			if (!setornot) {
				return netp::OK;
			}

			int rt = socket_set_nonblocking_impl(onoff);
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			if (onoff) {
				m_option |= int(socket_option::OPTION_NON_BLOCKING);
			}
			else {
				m_option &= ~int(socket_option::OPTION_NON_BLOCKING);
			}
			return netp::OK;
		}

		int _cfg_buffer(channel_buf_cfg const& cfg) {
			int rt = set_snd_buffer_size(cfg.sndbuf_size);
			NETP_RETURN_V_IF_MATCH(rt, rt < 0);

			return set_rcv_buffer_size(cfg.rcvbuf_size);
		}

		int _cfg_nodelay(bool onoff) {

			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_protocol == u16_t(NETP_PROTOCOL_UDP));
			bool setornot = ((m_option & u16_t(socket_option::OPTION_NODELAY)) && (!onoff)) ||
				(((m_option & u16_t(socket_option::OPTION_NODELAY)) == 0) && (onoff));

			if (!setornot) {
				return netp::OK;
			}

			int optval = onoff ? 1 : 0;
			int rt = socket_setsockopt_impl(IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			if (onoff) {
				m_option |= u16_t(socket_option::OPTION_NODELAY);
			} else {
				m_option &= ~u16_t(socket_option::OPTION_NODELAY);
			}
			return netp::OK;
		}

		//@no way to read it back on windows
		int __cfg_keepalive_vals(keep_alive_vals const& vals) {
			/*BFR WILL SETUP A DEFAULT KAV AT START FOR THE CURRENT IMPL*/
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_family == u16_t(NETP_AF_USER));
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_protocol == u16_t(NETP_PROTOCOL_UDP));
			NETP_ASSERT(m_option & int(socket_option::OPTION_KEEP_ALIVE));
			int rt;
#ifdef _NETP_WIN
			DWORD dwBytesRet;
			struct tcp_keepalive alive;
			::memset(&alive, 0, sizeof(alive));

			alive.onoff = 1;
			if (vals.idle == 0) {
				alive.keepalivetime = (NETP_DEFAULT_TCP_KEEPALIVE_IDLETIME * 1000);
			} else {
				alive.keepalivetime = (vals.idle * 1000);
			}

			if (vals.interval == 0) {
				alive.keepaliveinterval = (NETP_DEFAULT_TCP_KEEPALIVE_INTERVAL * 1000);
			} else {
				alive.keepaliveinterval = vals.interval * 1000;
			}

			rt = ::WSAIoctl(m_fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), nullptr, 0, &dwBytesRet, nullptr, nullptr);
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
#elif defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
			if (vals.idle != 0) {
				int idle = (vals.idle);
				rt = socket_setsockopt_impl( SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
				NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			}
			if (vals.interval != 0) {
				int interval = (vals.interval);
				rt = socket_setsockopt_impl( SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
				NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			}
			if (vals.probes != 0) {
				int probes = vals.probes;
				rt = socket_setsockopt_impl( SOL_TCP, TCP_KEEPCNT, &probes, sizeof(probes));
				NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			}
#else
#error
#endif
			return netp::OK;
		}

		int _cfg_keepalive(bool onoff, keep_alive_vals const& vals) {
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

			/*BFR WILL SETUP A DEFAULT KAV AT START FOR THE CURRENT IMPL*/
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_family == u16_t(NETP_AF_USER));
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_protocol == u16_t(NETP_PROTOCOL_UDP));

			//force to false
			int optval = onoff ? 1 : 0;
			int rt = socket_setsockopt_impl(SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			if (onoff) {
				m_option |= int(socket_option::OPTION_KEEP_ALIVE);
				return __cfg_keepalive_vals(vals);
			} else {
				m_option &= ~int(socket_option::OPTION_KEEP_ALIVE);
				return netp::OK;
			}
		}

		int _cfg_broadcast(bool onoff) {
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
			NETP_RETURN_V_IF_NOT_MATCH(netp::E_INVALID_OPERATION, m_protocol == u8_t(NETP_PROTOCOL_UDP));

			bool setornot = ((m_option & u16_t(socket_option::OPTION_BROADCAST)) && (!onoff)) ||
				(((m_option&u16_t(socket_option::OPTION_BROADCAST)) == 0) && (onoff));

			if (!setornot) {
				return netp::OK;
			}
			int optval = onoff ? 1 : 0;
			int rt = socket_setsockopt_impl(SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);
			if (onoff) {
				m_option |= u16_t(socket_option::OPTION_BROADCAST);
			} else {
				m_option &= ~u16_t(socket_option::OPTION_BROADCAST);
			}
			return netp::OK;
		}

		int _cfg_option(u16_t opt, keep_alive_vals const& kvals) {

			//force nonblocking
			int rt = _cfg_nonblocking((opt & u16_t(socket_option::OPTION_NON_BLOCKING)) != 0);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

			rt = _cfg_reuseaddr((opt & u16_t(socket_option::OPTION_REUSEADDR)) != 0);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

#if defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
			rt = _cfg_reuseport((m_option & u8_t(socket_option::OPTION_REUSEPORT)) != 0);
			NETP_RETURN_V_IF_MATCH(NETP_SOCKET_ERROR, rt == NETP_SOCKET_ERROR);
#endif

			if (is_udp()) {
				rt = _cfg_nocheck((opt & u16_t(socket_option::OPTION_NOCHECK)) != 0);
				NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

				rt = _cfg_broadcast((opt & u16_t(socket_option::OPTION_BROADCAST)) != 0);
				NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
			}

			if (is_tcp()) {
				rt = _cfg_nodelay((opt & u16_t(socket_option::OPTION_NODELAY)) != 0);
				NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);

				rt = _cfg_keepalive((opt & u16_t(socket_option::OPTION_KEEP_ALIVE)) != 0, kvals);
				NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
			}
			return netp::OK;
		}

		int _cfg_load_rcv_buf_size() {
			int rt = get_rcv_buffer_size();
			NETP_RETURN_V_IF_MATCH(rt, rt<0);
			m_rcv_buf_size = rt;
			return netp::OK;
		}

		int _cfg_load_snd_buf_size() {
			int rt = get_snd_buffer_size();
			NETP_RETURN_V_IF_MATCH(rt, rt < 0);
			m_snd_buf_size = rt;
			return netp::OK;
		}

		//all user custom socket(such as bfr etc), must impl these functions by user to utilize netplus features
		//the name of these kinds of functions looks like socket_x_impl
		//the defualt impl is posix socket api
		virtual int socket_open_impl() {
			NETP_ASSERT( m_fd == NETP_INVALID_SOCKET );
			m_fd = netp::open(m_family, m_type, m_protocol);
			NETP_RETURN_V_IF_MATCH((NETP_SOCKET_ERROR), m_fd == NETP_INVALID_SOCKET);
			return netp::OK;
		}
		virtual int socket_close_impl () {
			NETP_TRACE_SOCKET_OC("[socket][%s][socket_close_impl][netp::close]", ch_info().c_str());
			return netp::close(m_fd);
		}
		virtual int socket_shutdown_impl (int flag) {
			return netp::shutdown(m_fd, flag);
		}
		virtual int socket_bind_impl(NRP<address> const& addr) {
			return netp::bind(m_fd, addr);
		}
		virtual int socket_listen_impl(int backlog = NETP_DEFAULT_LISTEN_BACKLOG) {
			return netp::listen(m_fd, backlog);
		}
		virtual SOCKET socket_accept_impl( NRP<address>& raddr, NRP<address>& laddr) {
			SOCKET nfd = netp::accept(m_fd, raddr);
			NETP_RETURN_V_IF_MATCH(NETP_INVALID_SOCKET, nfd == NETP_INVALID_SOCKET);

			//patch for local addr
			int rt = netp::getsockname(nfd, laddr);
			if (rt != netp::OK) {
#ifdef _NETP_WIN
				NETP_ERR("[socket][%s][accept][netp::close]load local addr failed: %d, nfd: %zu", ch_info().c_str(), netp_socket_get_last_errno(), nfd );
#else
				NETP_ERR("[socket][%s][accept][netp::close]load local addr failed: %d, nfd: %u", ch_info().c_str(), netp_socket_get_last_errno(), nfd);
#endif
				netp::close(nfd);
				//quick return for retry
				netp_socket_set_last_errno(netp::E_EINTR);
				return (NETP_INVALID_SOCKET);
			}

			NETP_ASSERT(laddr->family() == (m_family));
			if ( *m_laddr == *raddr) {
#ifdef _NETP_WIN
				NETP_ERR("[socket][%s][accept][netp::close]laddr == raddr, force close, nfd: %zu", ch_info().c_str(), nfd);
#else
				NETP_ERR("[socket][%s][accept][netp::close]laddr == raddr, force close, nfd: %u", ch_info().c_str(), nfd);
#endif
				netp::close(nfd);
				//quick return for retry
				netp_socket_set_last_errno(netp::E_EINTR);
				return (NETP_INVALID_SOCKET);
			}
			return nfd ;
		}
		virtual int socket_connect_impl(NRP<address> const& addr) {
			return netp::connect(m_fd, addr);
		}

		virtual int socket_set_nonblocking_impl(bool onoff) {
			return netp::set_nonblocking(m_fd, onoff);
		}

		virtual int socket_getpeername_impl(NRP<address>& raddr) {
			return netp::getpeername(m_fd, raddr);
		}

		virtual int socket_getsockname_impl(NRP<address>& laddr) {
			return netp::getsockname(m_fd, laddr);
		}

		virtual int socket_getsockopt_impl(int level, int option_name, void* value, socklen_t* option_len) const {
			return netp::getsockopt(m_fd, level, option_name, value, option_len);
		}

		virtual int socket_setsockopt_impl(int level, int option_name, void const* value, socklen_t const& option_len) {
			return netp::setsockopt(m_fd, level, option_name, value, option_len);
		}

		virtual int socket_send_impl(const byte_t* data, u32_t len, int flag = 0) {
			return netp::send(m_fd, data, len, flag);
		}
		virtual int socket_sendto_impl(const byte_t* data, u32_t len, NRP<address> const& to, int flag = 0) {
			return netp::sendto(m_fd, data, len, to, flag);
		}

		virtual int socket_recv_impl(byte_t* const buf, u32_t size, int flag = 0) {
			return netp::recv(m_fd, buf, size, flag);
		}
		virtual int socket_recvfrom_impl(byte_t* const buf, u32_t size, NRP<address>& from, int flag = 0) {
			return netp::recvfrom(m_fd, buf, size, from, flag);
		}

	public:
		__NETP_FORCE_INLINE u16_t sock_family() const { return ((m_family)); };
		__NETP_FORCE_INLINE u16_t sock_type() const { return (m_type); };
		__NETP_FORCE_INLINE u16_t sock_protocol() { return (m_protocol); };

		__NETP_FORCE_INLINE bool is_stream() const { return (m_type== NETP_SOCK_STREAM); };

		__NETP_FORCE_INLINE bool is_tcp() const { return m_protocol == u8_t(NETP_PROTOCOL_TCP); }
		__NETP_FORCE_INLINE bool is_udp() const { return m_protocol == u8_t(NETP_PROTOCOL_UDP); }
		__NETP_FORCE_INLINE bool is_icmp() const { return m_protocol == u8_t(NETP_PROTOCOL_ICMP); }

		__NETP_FORCE_INLINE SOCKET fd() const { return m_fd; }
		__NETP_FORCE_INLINE NRP<address> const& remote_addr() const { return m_raddr; }
		__NETP_FORCE_INLINE NRP<address> const& local_addr() const { return m_laddr; }
		__NETP_FORCE_INLINE bool is_nonblocking() const { return ((m_option & u8_t(socket_option::OPTION_NON_BLOCKING)) != 0); }

		__NETP_FORCE_INLINE int cfg_nodelay(bool onoff) { return _cfg_nodelay(onoff); }
		__NETP_FORCE_INLINE int cfg_nonblocking(bool onoff) { return _cfg_nonblocking(onoff); }
		__NETP_FORCE_INLINE int cfg_reuseaddr() { return _cfg_reuseaddr(true); }
		__NETP_FORCE_INLINE int cfg_reuseport() { return _cfg_reuseport(true); }

		//https://www.man7.org/linux/man-pages/man7/socket.7.html
		int cfg_incoming_cpu(int cpu_affinity) {
#ifdef __NETP_ENABLE_SO_INCOMING_CPU
			int rt;
			//int __cpu_affinity_old= -2;
			//socklen_t __affinity_len = sizeof(__cpu_affinity_old);
			//@note , if it is not specifed, __cpu_affinity_old would be set to -1 (undefined)
			//rt = socket_getsockopt_impl(SOL_SOCKET, SO_INCOMING_CPU, &__cpu_affinity_old, &__affinity_len);

			//NETP_VERBOSE("[socket][%s]cfg_incoming_cpu, previous: %d, read rt: %d", ch_info().c_str(), __cpu_affinity_old, rt);
			rt = socket_setsockopt_impl(SOL_SOCKET, SO_INCOMING_CPU, &cpu_affinity, sizeof(cpu_affinity));
			NETP_RETURN_V_IF_MATCH(netp_socket_get_last_errno(), rt == NETP_SOCKET_ERROR);

			//rt = socket_getsockopt_impl(SOL_SOCKET, SO_INCOMING_CPU, &__cpu_affinity_old, &__affinity_len);
			//NETP_VERBOSE("[socket][%s]cfg_incoming_cpu, now: %d, target: %d, read rt: %d", ch_info().c_str(), __cpu_affinity_old, cpu_affinity, rt);
			return netp::OK;
#else
			(void)cpu_affinity;
			return netp::OK;
#endif
		}

		int load_sockname() {
			int rt = socket_getsockname_impl(m_laddr);
			if (rt == netp::OK) {
				NETP_ASSERT(m_laddr&&!m_laddr->is_af_unspec());
				NETP_ASSERT(m_laddr->family() == m_family);
				return netp::OK;
			}
			return netp_socket_get_last_errno();
		}

		//load_peername always succeed on win10
		int load_peername() {
			int rt = socket_getpeername_impl(m_raddr);
			if (rt == netp::OK) {
				NETP_ASSERT(m_raddr->family() == NETP_AF_INET);
				NETP_ASSERT(!m_laddr->is_af_unspec());
				return netp::OK;
			}
			return netp_socket_get_last_errno();
		}
		int bind_any();

		int open();
		int close();
		int shutdown(int flag);
		int bind(NRP<address> const& addr);
		int listen(int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
		SOCKET accept( NRP<address>& raddr, NRP<address>& laddr);
		int connect(NRP<address> const& addr);

		//int sendto();
		//int recvfrom();
		//int send();
		//int recv();

		//@NOTE
		// FOR PASSIVE FD , THE BEST WAY IS TO INHERITE THE SETTINGS FROM LISTEN FD
		// FOR ACTIVE FD, THE BEST WAY IS TO CALL THESE TWO APIS BEFORE ANY DATA WRITE

		int set_snd_buffer_size(u32_t size);
		int get_snd_buffer_size() const;

		//@deprecated
//		int get_left_snd_queue() const;

		int set_rcv_buffer_size(u32_t size);
		int get_rcv_buffer_size() const;

		//@deprecated
		//int get_left_rcv_queue() const;

		int get_linger(bool& on_off, int& linger_t) const;
		int set_linger(bool on_off, int linger_t = 30 /* in seconds */);

		__NETP_FORCE_INLINE int cfg_keep_alive_on() { return _cfg_keepalive(true, default_tcp_keep_alive_vals); }
		__NETP_FORCE_INLINE int cfg_keep_alive_off() { return _cfg_keepalive(false, default_tcp_keep_alive_vals); }

		//@hint windows do not provide ways to retrieve idle time and interval ,and probes has been disabled by programming since vista
		int set_keep_alive_vals(bool onoff, keep_alive_vals const& vals) { return _cfg_keepalive(onoff, vals); }

		int get_tos(u8_t& tos) const;
		int cfg_tos(u8_t tos);

		int ch_init(u16_t opt, keep_alive_vals const& kvals, channel_buf_cfg const& cbc) {
			NETP_ASSERT(L->in_event_loop());
			//@note: F_CLOSED SHOULD ALWAYS BE CLEARED ONCE fd is set
			//cuz we use this flag to check rdwr check ,rdwr -> ch_io_end -> ch_deinit()

			//set F_CLOSED if fd is NETP_INVALID_SOCKET when socket_channel is constructed
			//set F_CLOSED if both read|write is shutdowned, then schedule a ch_io_end to do netp::close(fd) & ch_deinit();

			int rt = netp::OK;
			if (m_fd == NETP_INVALID_SOCKET) {
				rt = open();
				if (rt != netp::OK) {
					NETP_WARN("[socket][%s][ch_init]open, errno: %d", ch_info().c_str(), rt );
					m_chflag |= int(channel_flag::F_READ_ERROR)|int(channel_flag::F_WRITE_ERROR);
					ch_errno() = rt;
					return rt;
				}
				NETP_ASSERT(m_fd != NETP_INVALID_SOCKET);
			}
			NETP_TRACE_SOCKET_OC("[socket][%s][ch_init][netp::open][socket]", ch_info().c_str());
			channel::ch_init();

			NETP_ASSERT( (m_chflag & int(channel_flag::F_CLOSED))==0);
			rt = _cfg_option(opt, kvals);
			if (rt != netp::OK) {
				NETP_WARN("[socket][%s][ch_init]_cfg_option, errno: %d", ch_info().c_str(), rt);
				m_chflag |= int(channel_flag::F_READ_ERROR);
				ch_errno() = rt;
				ch_close_impl(nullptr);
				return rt;
			}

			rt = _cfg_buffer(cbc);
			if (rt != netp::OK) {
				NETP_WARN("[socket][%s][ch_init]_cfg_buffer, errno: %d", ch_info().c_str(), rt);
				m_chflag |= int(channel_flag::F_READ_ERROR);
				ch_errno() = rt;
				ch_close_impl(nullptr);
				return rt;
			}
			return netp::OK;
		}

		//url example: tcp://0.0.0.0:80, udp://127.0.0.1:80
		//@todo
		//tcp6://ipv6address
		void do_listen_on(NRP<promise<int>> const& intp, NRP<address> const& addr, fn_channel_initializer_t const& fn_accepted, NRP<socket_cfg> const& ccfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
		void do_dial(NRP<promise<int>> const& dialp, NRP<address> const& addr, fn_channel_initializer_t const& fn_initializer);

		void _ch_do_close_read() {
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING)|int(channel_flag::F_READ_SHUTDOWN)) ) { return; }

			m_chflag |= int(channel_flag::F_READ_SHUTDOWNING);
			ch_io_end_read();
			//end_read and log might result in F_READ_SHUTDOWN state. (FOR net_logger)
			socket_shutdown_impl(SHUT_RD);
			ch_fire_read_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read end, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			m_chflag |= int(channel_flag::F_READ_SHUTDOWN);
			m_chflag &= ~int(channel_flag::F_READ_SHUTDOWNING);

			ch_rdwr_shutdown_check();
		}

		void _ch_do_close_write() {
			if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_SHUTDOWNING)) ) { return; }

			//boundary checking&set
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWNING);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
			ch_io_end_write();

#ifdef _NETP_DEBUG
			NETP_ASSERT( ch_is_connected() ? m_tx_entry_to_q.empty() : m_tx_entry_q.empty(), "flag: %u", m_chflag );
#endif

			while (m_tx_entry_q.size()) {
				NETP_ASSERT((ch_errno() != 0) && (m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_READ_ERROR) | int(channel_flag::F_FIRE_ACT_EXCEPTION))));
				socket_outbound_entry& entry = m_tx_entry_q.front();
				NETP_WARN("[socket][%s]cancel outbound, nbytes:%u, errno: %d", ch_info().c_str(), entry.data->len(), ch_errno());
				//hold a copy before we do pop it from queue
				NRP<promise<int>> wp = entry.write_promise;
				m_tx_bytes -= u32_t(entry.data->len());
				m_tx_entry_q.pop_front();
				NETP_ASSERT(wp->is_idle());
				wp->set(ch_errno());
			}

			while (m_tx_entry_to_q.size()) {
				NETP_ASSERT((ch_errno() != 0) && (m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_READ_ERROR) | int(channel_flag::F_FIRE_ACT_EXCEPTION))));
				socket_outbound_entry_to& entry = m_tx_entry_to_q.front();
				NETP_WARN("[socket][%s]cancel outbound, nbytes:%u, errno: %d, to: %s", ch_info().c_str(), entry.data->len(), ch_errno(), entry.to && !entry.to->is_af_unspec() ? entry.to->to_string().c_str() : "");
				//hold a copy before we do pop it from queue
				NRP<promise<int>> wp = entry.write_promise;
				m_tx_bytes -= u32_t(entry.data->len());
				m_tx_entry_to_q.pop_front();
				NETP_ASSERT(wp->is_idle());
				wp->set(ch_errno());
			}
			socket_shutdown_impl(SHUT_WR);

			//unset boundary
			ch_fire_write_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_write end, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWNING);
			ch_rdwr_shutdown_check();
		}

		void __do_io_dial_done(fn_channel_initializer_t const& fn_initializer, NRP<promise<int>> const& dialf, int status, io_ctx* ctx);

		void __do_accept_fire(fn_channel_initializer_t const& ch_initializer) {
			ch_io_begin([ch=NRP<socket_channel>(this),ch_initializer](int status, io_ctx*) {
				if (status != netp::OK) {
					//begin failed
					NETP_ASSERT(ch->ch_flag() & int(channel_flag::F_CLOSED));
					return;
				}

				try {
					if (NETP_LIKELY(ch_initializer != nullptr)) {
						ch_initializer(ch);
					}
				} catch (netp::exception const& e) {
					NETP_ASSERT(e.code() != netp::OK);
					status = e.code();
					NETP_ERR("[socket][%s]accept netp::exception: %d, what: %s", ch->ch_info().c_str(), status, e.what());
				} catch (std::exception const& e) {
					status = netp_socket_get_last_errno();
					if (status == netp::OK) {
						status = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket][%s]accept std::exception: %d, what: %s", ch->ch_info().c_str(), status, e.what() );
				} catch (...) {
					status = netp_socket_get_last_errno();
					if (status == netp::OK) {
						status = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept unknown exception: %d", status);
				}

				if (status != netp::OK) {
					ch->ch_flag() |= int(channel_flag::F_READ_ERROR);
					ch->ch_errno() = status;
					ch->ch_close_impl(nullptr);
					return;
				}

				ch->ch_set_connected();
				_CH_FIRE_ACTION_CLOSE_AND_RETURN_IF_EXCEPTION(ch->ch_fire_connected(), ch, "ch_fire_connected");

				//it's safe to close read in connected() callback
				ch->ch_io_read();
			});
		}

		//posix api impl
		virtual void __do_io_accept_impl(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& cfg, int status, io_ctx* ctx);

		__NETP_FORCE_INLINE void ___do_io_read_done(int status) {
			switch (status) {
			case netp::OK:
			case netp::E_EWOULDBLOCK:
			{}
			break;
			case netp::E_SOCKET_GRACE_CLOSE:
			{
#ifdef _NETP_DEBUG
				NETP_ASSERT(m_protocol != u8_t(NETP_PROTOCOL_UDP));
#endif
				m_chflag |= int(channel_flag::F_FIN_RECEIVED);
				ch_close_read_impl(nullptr);
			}
			break;
			default:
			{
				NETP_VERBOSE("[socket][%s]___do_io_read_done, _ch_do_close_read_write, read error: %d, close, flag: %u", ch_info().c_str(), status, m_chflag);
				NETP_ASSERT(status < 0);
				m_chflag |= int(channel_flag::F_READ_ERROR);
				ch_errno() = (status);
				ch_close_impl(nullptr);
			}
			}
		}

		void __do_io_read_from(int status, io_ctx* ctx);
		void __do_io_read(int status, io_ctx* ctx);

		inline void __do_io_write_done(const int status) {
			switch (status) {
			case netp::OK:
			{
#ifdef _NETP_DEBUG
				NETP_ASSERT(ch_is_connected() ? m_tx_entry_q.empty() : m_tx_entry_to_q.empty(), "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno);
#endif
				NETP_ASSERT((m_chflag & int(channel_flag::F_TX_LIMIT)) == 0);
				if (m_chflag & int(channel_flag::F_CLOSE_PENDING)) {
					_ch_do_close_read_write();
					NETP_TRACE_SOCKET("[socket][%s]IO_WRITE, end F_CLOSE_PENDING, _ch_do_close_read_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
				} else if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
					_ch_do_close_write();
					NETP_TRACE_SOCKET("[socket][%s]IO_WRITE, end F_WRITE_SHUTDOWN_PENDING, ch_close_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
				} else {
					ch_io_end_write();
				}

				NETP_ASSERT( (m_chflag & (int(channel_flag::F_WATCH_WRITE))) == 0);
			}
			break;
			case netp::E_EWOULDBLOCK:
			{
#ifdef _NETP_DEBUG
				NETP_ASSERT(ch_is_connected() ? m_tx_entry_q.size() : m_tx_entry_to_q.size(), "[#%s]flag: %d, errno: %d", ch_info().c_str(), m_chflag, m_cherrno);
#endif

#ifdef NETP_ENABLE_FAST_WRITE
				NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER)) );
				ch_io_write();
#else
				NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER) | int(channel_flag::F_WATCH_WRITE)) == (int(channel_flag::F_WRITE_BARRIER) | int(channel_flag::F_WATCH_WRITE))  );
#endif
				//NETP_TRACE_SOCKET("[socket][%s]__do_io_write, write block", info().c_str());
			}
			break;
			case netp::E_CHANNEL_TXLIMIT:
			{
				m_chflag |= int(channel_flag::F_TX_LIMIT);
				ch_io_end_write();
			}
			break;
			default:
			{
				m_chflag |= int(channel_flag::F_WRITE_ERROR);
				ch_errno() = (status);
				ch_close_impl(nullptr);
				NETP_VERBOSE("[socket][%s]__do_io_write, call_ch_do_close_read_write, write error: %d, m_chflag: %u", ch_info().c_str(), status, m_chflag);
			}
			break;
			}
		}

		void __do_io_write(int status, io_ctx* ctx);
		void __do_io_write_to(int status, io_ctx* ctx);

		//@note, we need simulate a async write, so for write operation, we'll flush outbound buffer in the next loop
		//flush until error
		//<0, is_error == (errno != E_CHANNEL_WRITING)
		//==0, flush done
		//this api would be called right after a check of writeable of the current socket
		int ___do_io_write();
		int ___do_io_write_to();

		//for connected socket type
		void _ch_do_close_listener();
		void _ch_do_close_read_write();

		virtual void __io_begin_done(io_ctx*) {
			m_chflag |= int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE);
		}

		void ch_write_impl(NRP<promise<int>> const& intp, NRP<packet> const& outlet) override;
		void ch_write_to_impl(NRP<promise<int>> const& intp, NRP<packet> const& outlet, NRP<netp::address> const& to) override;

		void ch_close_read_impl(NRP<promise<int>> const& closep) override;
		void ch_close_write_impl(NRP<promise<int>> const& chp) override;
		void ch_close_impl(NRP<promise<int>> const& chp) override;

		//for io monitor
		virtual void io_notify_terminating(int status, io_ctx*) override;
		virtual void io_notify_read(int status, io_ctx* ctx) override;
		virtual void io_notify_write(int status, io_ctx* ctx) override;
		
		virtual void __ch_clean();
		virtual void __ch_io_cancel_connect(int cancel_code, io_ctx* ctx_) {
			NETP_ASSERT(m_fn_write != nullptr);
			(*m_fn_write)(cancel_code, ctx_);
		}

	public:
		void ch_io_begin(fn_io_event_t const& fn_begin_done) override;
		void ch_io_end() override;

		void ch_io_accept(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& cfg, fn_io_event_t const& fn = nullptr) override;
		void ch_io_end_accept() override {
			ch_io_end_read();
		}

		void ch_io_read(fn_io_event_t const& fn_read = nullptr) override;
		void ch_io_end_read() override;
		void ch_io_write(fn_io_event_t const& fn_write = nullptr) override;
		void ch_io_end_write() override;

		void ch_io_connect(fn_io_event_t const& fn = nullptr) override {
			NETP_ASSERT(fn != nullptr);
			if (m_chflag&int(channel_flag::F_WATCH_WRITE)) {
				return;
			}
			ch_io_write(fn);
		}

		void ch_io_end_connect() override {
			NETP_ASSERT(!ch_is_passive());
			ch_io_end_write();
		}

		NRP<promise<int>> ch_set_read_buffer_size(u32_t size) override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), size, chp]() {
				chp->set(S->set_rcv_buffer_size(size));
			});
			return chp;
		}

		NRP<promise<int>> ch_get_read_buffer_size() override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), chp]() {
				chp->set(S->m_rcv_buf_size);
			});
			return chp;
		}

		NRP<promise<int>> ch_set_write_buffer_size(u32_t size) override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), size, chp]() {
				chp->set(S->set_snd_buffer_size(size));
			});
			return chp;
		}

		NRP<promise<int>> ch_get_write_buffer_size() override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), chp]() {
				chp->set(S->m_snd_buf_size);
			});
			return chp;
		}

		NRP<promise<int>> ch_set_nodelay() override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([s = NRP<socket_channel>(this), chp]() {
				chp->set(s->cfg_nodelay(true));
			});
			return chp;
		}

		__NETP_FORCE_INLINE channel_id_t ch_id() const override { return m_fd; }
		netp::string_t ch_info() const override {
			return socketinfo{ m_fd, (m_family),(m_type),(m_protocol),local_addr(), remote_addr() }.to_string();
		}
		void ch_set_tx_limit(netp::u32_t limit) override {
			L->execute([s = NRP<socket_channel>(this), limit]() {
				s->m_tx_limit = (limit != 0 && limit< _NETP_SOCKET_CHANNEL_LIMIT_MIN) ? _NETP_SOCKET_CHANNEL_LIMIT_MIN: limit;
				s->m_tx_budget = (limit != 0 && limit < _NETP_SOCKET_CHANNEL_LIMIT_MIN) ? _NETP_SOCKET_CHANNEL_LIMIT_MIN : limit;
			});
		}

		NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> dup(NRP<event_loop> const& LL);
	};

	extern NRP<socket_channel> default_socket_channel_maker(NRP<netp::socket_cfg> const& cfg);
	extern std::tuple<int, NRP<socket_channel>> create_socket_channel(NRP<netp::socket_cfg> const& cfg);
}
#endif