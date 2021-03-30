#ifndef _NETP_SOCKET_CH_HPP_
#define _NETP_SOCKET_CH_HPP_

#include <queue>

#include <netp/smart_ptr.hpp>
#include <netp/string.hpp>
#include <netp/packet.hpp>
#include <netp/address.hpp>

#include <netp/socket_api.hpp>
#include <netp/channel.hpp>
#include <netp/dns_resolver.hpp>

//@NOTE: turn on this option would result in about 20% performance boost for EPOLL
#define NETP_ENABLE_FAST_WRITE

//in milliseconds
#define NETP_SOCKET_BDLIMIT_TIMER_DELAY_DUR (250)
#define NETP_DEFAULT_LISTEN_BACKLOG 256

namespace netp {

	enum socket_option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 1, //only for UDP
		OPTION_REUSEADDR = 1 << 1,
		OPTION_REUSEPORT = 1 << 2,
		OPTION_NON_BLOCKING = 1 << 3,
		OPTION_NODELAY = 1 << 4, //only for TCP
		OPTION_KEEP_ALIVE = 1 << 5
	};

	const static int default_socket_option = int(socket_option::OPTION_NON_BLOCKING) | int(socket_option::OPTION_KEEP_ALIVE);

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
		u8_t f;
		u8_t t;
		u16_t p;

		address laddr;
		address raddr;

		std::string to_string() const {
			char _buf[1024] = { 0 };
#ifdef _NETP_MSVC
			int nbytes = snprintf(_buf, 1024, "#%zu:%s:L:%s-R:%s", fd, DEF_protocol_str[int(p)], laddr.to_string().c_str(), raddr.to_string().c_str());
#elif defined(_NETP_GCC)
			int nbytes = snprintf(_buf, 1024, "#%d:%s:L:%s-R:%s", fd, DEF_protocol_str[int(p)], laddr.to_string().c_str(), raddr.to_string().c_str());
#else
#error "unknown compiler"
#endif

			return std::string(_buf, nbytes);
		}
	};


	class socket_cfg;
	typedef std::function<NRP<channel>(NRP<socket_cfg> const& cfg)> fn_channel_creator_t;
	class socket_cfg final :
		public ref_base
	{
	public:
		NRP<io_event_loop> L;
		SOCKET fd;
		u8_t family;
		u8_t type;
		u16_t proto;
		u16_t option;

		socket_api* sockapi;

		address laddr;
		address raddr;

		keep_alive_vals kvals;
		channel_buf_cfg sock_buf;
		u32_t bdlimit; //in bit (1kb == 1024b), 0 means no limit
		u32_t wsabuf_size;
		socket_cfg(NRP<io_event_loop> const& L = nullptr) :
			L(L),
			fd(SOCKET(NETP_INVALID_SOCKET)),
			family((NETP_AF_INET)),
			type(NETP_SOCK_STREAM),
			proto(NETP_PROTOCOL_TCP),
			option(default_socket_option),
			sockapi((netp::socket_api*)&netp::default_socket_api),
			laddr(),
			raddr(),
			kvals(default_tcp_keep_alive_vals),
			sock_buf({ 0 }),
			bdlimit(0),
			wsabuf_size(64*1024)
		{}
	};

	template <class socket_channel_t>
	std::tuple<int, NRP<socket_channel_t>> create(NRP<socket_cfg> const& cfg) {
		NETP_ASSERT(cfg->L != nullptr);
		NETP_ASSERT(cfg->L->in_event_loop());
		NETP_ASSERT(cfg->proto == NETP_PROTOCOL_USER ? cfg->L->poller_type() != NETP_DEFAULT_POLLER_TYPE : true);
		NRP<socket_channel_t> so = netp::make_ref<socket_channel_t>(cfg);
		int rt;
		if (cfg->fd == NETP_INVALID_SOCKET) {
			rt = so->open();
			if (rt != netp::OK) {
				NETP_WARN("[socket][%s]open failed: %d", so->ch_info().c_str(), rt);
				return std::make_tuple(rt,nullptr);
			}
		}
		rt = so->ch_init(cfg->option, cfg->kvals, cfg->sock_buf);
		if (rt != netp::OK) {
			so->close();
			NETP_WARN("[socket][%s]init failed: %d", so->ch_info().c_str(), rt);
			return std::make_tuple(rt, nullptr);
		}
		return std::make_tuple(rt, so);
	}

	template <class socket_channel_t>
	std::tuple<int, NRP<socket_channel_t>> accepted_create(NRP<io_event_loop> const& L, SOCKET nfd, address const& laddr, address const& raddr, NRP<socket_cfg> const& cfg) {
		NETP_ASSERT(L->in_event_loop());
		NRP<socket_cfg> ccfg = netp::make_ref<socket_cfg>();
		ccfg->fd = nfd;
		ccfg->family = cfg->family;
		ccfg->type = cfg->type;
		ccfg->proto = cfg->proto;
		ccfg->laddr = laddr;
		ccfg->raddr = raddr;

		ccfg->L = L;
		ccfg->sockapi = cfg->sockapi;
		ccfg->option = cfg->option;
		ccfg->kvals = cfg->kvals;
		ccfg->sock_buf = cfg->sock_buf;
		ccfg->bdlimit = cfg->bdlimit;

		return create<socket_channel_t>(ccfg);
	}

	struct socket_outbound_entry final {
		NRP<packet> data;
		NRP<promise<int>> write_promise;
		address to;
	};

	class socket_channel:
		public channel
	{
		friend void do_dial(address const& addr, fn_channel_initializer_t const& initializer, NRP<channel_dial_promise> const& ch_dialf, NRP<socket_cfg> const& cfg);
		friend void do_listen_on(NRP<channel_listen_promise> const& listenp, address const& laddr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog);
		typedef std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>> socket_outbound_entry_t;

		template <class socket_channel_t>
		friend std::tuple<int, NRP<socket_channel_t>> accepted_create(NRP<io_event_loop> const& L, SOCKET nfd, address const& laddr, address const& raddr, NRP<socket_cfg> const& cfg);

		template <class socket_channel_t>
		friend std::tuple<int, NRP<socket_channel_t>> create(NRP<socket_cfg> const& cfg);

		template <class _Ref_ty, typename... _Args>
		friend ref_ptr<_Ref_ty> make_ref(_Args&&... args);

	protected:
		SOCKET m_fd;
		u8_t m_family;
		u8_t m_type;
		u16_t m_protocol;
		u16_t m_option;

		address m_laddr;
		address m_raddr;

		io_ctx* m_io_ctx;
		byte_t* m_rcv_buf_ptr;
		u32_t m_rcv_buf_size;

		u32_t m_noutbound_bytes;
		socket_outbound_entry_t m_outbound_entry_q;

		u32_t m_outbound_budget;
		u32_t m_outbound_limit; //in byte

		fn_io_event_t* m_fn_read;
		fn_io_event_t* m_fn_write;

		void _tmcb_BDL(NRP<timer> const& t);

		socket_channel(NRP<socket_cfg> const& cfg) :
			channel(cfg->L),
			m_fd(cfg->fd),
			m_family(cfg->family),
			m_type(cfg->type),
			m_protocol(cfg->proto),
			m_option(0),
			m_laddr(cfg->laddr),
			m_raddr(cfg->raddr),
			m_io_ctx(0),
			m_rcv_buf_ptr(cfg->L->channel_rcv_buf()->head()),
			m_rcv_buf_size(u32_t(cfg->L->channel_rcv_buf()->left_right_capacity())),
			m_noutbound_bytes(0),
			m_outbound_budget(cfg->bdlimit),
			m_outbound_limit(cfg->bdlimit),
			m_fn_read(nullptr),
			m_fn_write(nullptr)
		{
			NETP_ASSERT(cfg->L != nullptr);
		}

		~socket_channel()
		{
		}

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
			int rt = socket_setsockopt_impl(m_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
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
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_protocol == u16_t(NETP_PROTOCOL_USER));

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
			}
			else {
				m_option &= ~u16_t(socket_option::OPTION_NODELAY);
			}
			return netp::OK;
		}

		int __cfg_keepalive_vals(keep_alive_vals const& vals) {
			/*BFR WILL SETUP A DEFAULT KAV AT START FOR THE CURRENT IMPL*/
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_protocol == u16_t(NETP_PROTOCOL_USER));
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
			//m_kvals = vals;
			return netp::OK;
		}

		int _cfg_keepalive(bool onoff, keep_alive_vals const& vals) {
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_fd == NETP_INVALID_SOCKET);

			/*BFR WILL SETUP A DEFAULT KAV AT START FOR THE CURRENT IMPL*/
			NETP_RETURN_V_IF_MATCH(netp::E_INVALID_OPERATION, m_protocol == u16_t(NETP_PROTOCOL_USER));

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
				(((m_option & u16_t(socket_option::OPTION_BROADCAST)) == 0) && (onoff));

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

		//all user custom socket(such as bfr/kcp etc), must impl these functions by user to utilize netplus features
		//the name of these kinds of functions looks like socket_x_impl
		//the defualt impl is posix socket api
		virtual int socket_open_impl() {
			NETP_ASSERT( m_fd == NETP_INVALID_SOCKET );
			m_fd = netp::open(m_family, m_type, m_protocol);
			NETP_RETURN_V_IF_MATCH((NETP_SOCKET_ERROR), m_fd == NETP_INVALID_SOCKET);
			return netp::OK;
		}
		virtual int socket_close_impl () {
			return netp::close(m_fd);
		}
		virtual int socket_shutdown_impl (int flag) {
			return netp::shutdown(m_fd, flag);
		}
		virtual int socket_bind_impl(address const& addr) {
			return netp::bind(m_fd, addr);
		}
		virtual int socket_listen_impl(int backlog = NETP_DEFAULT_LISTEN_BACKLOG) {
			return netp::listen(m_fd, backlog);
		}
		virtual SOCKET socket_accept_impl(address& raddr, address& laddr) {
			SOCKET nfd =  netp::accept(m_fd, raddr);

			//patch for local addr
			address laddr;
			int rt = netp::getsockname(nfd, laddr);
			if (rt != netp::OK) {
				NETP_ERR("[socket][%s][accept]load local addr failed: %d", ch_info().c_str(), netp_socket_get_last_errno());
				NETP_CLOSE_SOCKET(nfd);
				//quick return for retry
				netp_set_last_errno(netp::E_EINTR);
				nfd = NETP_SOCKET_ERROR;
				return nfd;
			}

			NETP_ASSERT(laddr.family() == (m_family));
			if (m_laddr == raddr) {
				NETP_ERR("[socket][%s][accept]laddr == raddr, force close", ch_info().c_str());
				NETP_CLOSE_SOCKET(nfd);
				//quick return for retry
				netp_set_last_errno(netp::E_EINTR);
				nfd = NETP_SOCKET_ERROR;
				return nfd;
			}
			laddr = m_laddr;
			return nfd ;
		}
		virtual int socket_connect_impl(address const& addr) {
			return netp::connect(m_fd, addr);
		}

		virtual int socket_set_nonblocking_impl(bool onoff) {
			return netp::set_nonblocking(m_fd, onoff);
		}

		virtual int socket_getpeername_impl(address& raddr) {
			return netp::getpeername(m_fd, raddr);
		}

		virtual int socket_getsockname_impl(address& laddr) {
			return netp::getsockname(m_fd, laddr);
		}

		virtual int socket_getsockopt_impl(int level, int option_name, void* value, socklen_t* option_len) const {
			return netp::getsockopt(m_fd, level, option_name, value, option_len);
		}

		virtual int socket_setsockopt_impl(int level, int option_name, void const* value, socklen_t const& option_len) {
			return netp::setsockopt(m_fd, level, option_name, value, option_len);
		}

		virtual int socket_send_impl(const byte_t* data, u32_t len, int& status, int flag = 0) {
			return netp::send(m_fd, data, len, status, flag);
		}
		virtual int socket_sendto_impl(const byte_t* data, u32_t len, address const& to, int& status, int flag = 0) {
			return netp::sendto(m_fd, data, len, to, status, flag);
		}

		virtual int socket_recv_impl(byte_t* const buf, u32_t size, int& status, int flag = 0) {
			return netp::recv(m_fd, buf, size, status, flag);
		}
		virtual int socket_recvfrom_impl(byte_t* const  buf, u32_t size, address& from, int& status, int flag = 0) {
			return netp::recvfrom(m_fd, buf, size, from, status, flag);
		}

	public:
		__NETP_FORCE_INLINE u8_t sock_family() const { return ((m_family)); };
		__NETP_FORCE_INLINE u8_t sock_type() const { return (m_type); };
		__NETP_FORCE_INLINE u16_t sock_protocol() { return (m_protocol); };

		__NETP_FORCE_INLINE bool is_tcp() const { return m_protocol == u8_t(NETP_PROTOCOL_TCP); }
		__NETP_FORCE_INLINE bool is_udp() const { return m_protocol == u8_t(NETP_PROTOCOL_UDP); }
		__NETP_FORCE_INLINE bool is_icmp() const { return m_protocol == u8_t(NETP_PROTOCOL_ICMP); }

		__NETP_FORCE_INLINE SOCKET fd() const { return m_fd; }
		__NETP_FORCE_INLINE address const& remote_addr() const { return m_raddr; }
		__NETP_FORCE_INLINE address const& local_addr() const { return m_laddr; }
		__NETP_FORCE_INLINE bool is_nonblocking() const { return ((m_option & u8_t(socket_option::OPTION_NON_BLOCKING)) != 0); }

		__NETP_FORCE_INLINE int cfg_nodelay(bool onoff) { return _cfg_nodelay(onoff); }
		__NETP_FORCE_INLINE int cfg_nonblocking(bool onoff) { return _cfg_nonblocking(onoff); }
		__NETP_FORCE_INLINE int cfg_reuseaddr() { return _cfg_reuseaddr(true); }
		__NETP_FORCE_INLINE int cfg_reuseport() { return _cfg_reuseport(true); }

		int load_sockname() {
			int rt = socket_getsockname_impl(m_laddr);
			if (rt == netp::OK) {
				NETP_ASSERT(m_laddr.family() == NETP_AF_INET);
				NETP_ASSERT(!m_laddr.is_null());
				return netp::OK;
			}
			return netp_socket_get_last_errno();
		}

		//load_peername always succeed on win10
		int load_peername() {
			int rt = socket_getpeername_impl(m_raddr);
			if (rt == netp::OK) {
				NETP_ASSERT(m_raddr.family() == NETP_AF_INET);
				NETP_ASSERT(!m_laddr.is_null());
				return netp::OK;
			}
			return netp_socket_get_last_errno();
		}
		int bind_any();

		int open();
		int close();
		int shutdown(int flag);
		int bind(address const& addr);
		int listen(int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
		SOCKET accept(address& raddr, address& laddr);
		int connect(address const& addr);

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

		__NETP_FORCE_INLINE int turnon_keep_alive() { return _cfg_keepalive(true, default_tcp_keep_alive_vals); }
		__NETP_FORCE_INLINE int turnoff_keep_alive() { return _cfg_keepalive(false, default_tcp_keep_alive_vals); }

		//@hint windows do not provide ways to retrieve idle time and interval ,and probes has been disabled by programming since vista
		int set_keep_alive_vals(bool onoff, keep_alive_vals const& vals) { return _cfg_keepalive(onoff, vals); }

		int get_tos(u8_t& tos) const;
		int set_tos(u8_t tos);

		int ch_init(u16_t opt, keep_alive_vals const& kvals, channel_buf_cfg const& cbc) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_chflag&int(channel_flag::F_CLOSED));
			channel::ch_init();
			int rt = _cfg_option(opt, kvals);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
			rt = _cfg_buffer(cbc);
			if (rt != netp::OK) {
				ch_errno() = rt;
				m_chflag |= int(channel_flag::F_READ_ERROR);
				return rt;
			}
			m_chflag &= ~int(channel_flag::F_CLOSED);
			return rt;
		}

		//url example: tcp://0.0.0.0:80, udp://127.0.0.1:80
		//@todo
		//tcp6://ipv6address
		void do_listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<promise<int>> const& chp, NRP<socket_cfg> const& ccfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
		//NRP<promise<int>> listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<socket_cfg> const& cfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);

		void do_dial(address const& addr, fn_channel_initializer_t const& fn_initializer, NRP<promise<int>> const& dialp);
		//NRP<promise<int>> dial(address const& addr, fn_channel_initializer_t const& initializer);

		void _ch_do_close_read() {
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING)|int(channel_flag::F_READ_SHUTDOWN)) ) { return; }

			m_chflag |= int(channel_flag::F_READ_SHUTDOWNING);
			ch_io_end_read();
			//end_read and log might result in F_READ_SHUTDOWN state. (FOR net_logger)
			socket_shutdown_impl(SHUT_RD);
			ch_fire_read_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read end, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			m_chflag &= ~int(channel_flag::F_READ_SHUTDOWNING);
			m_chflag |= int(channel_flag::F_READ_SHUTDOWN);

			ch_rdwr_shutdown_check();
		}

		void _ch_do_close_write() {
			if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_SHUTDOWNING)) ) { return; }

			//boundary checking&set
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWNING);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
			ch_io_end_write();

			while (m_outbound_entry_q.size()) {
				NETP_ASSERT((ch_errno() != 0) && (m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_READ_ERROR) | int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING))));
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				NETP_WARN("[socket][%s]cancel outbound, nbytes:%u, errno: %d", ch_info().c_str(), entry.data->len(), ch_errno());
				//hold a copy before we do pop it from queue
				NRP<promise<int>> wp = entry.write_promise;
				m_noutbound_bytes -= u32_t(entry.data->len());
				m_outbound_entry_q.pop_front();
				NETP_ASSERT(wp->is_idle());
				wp->set(ch_errno());
			}

			socket_shutdown_impl(SHUT_WR);
			//unset boundary
			ch_fire_write_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_write end, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWNING);
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN);
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
				} catch (std::exception const& e) {
					status = netp_socket_get_last_errno();
					if (status == netp::OK) {
						status = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept failed: %d:%s", status, e.what());
				} catch (...) {
					status = netp_socket_get_last_errno();
					if (status == netp::OK) {
						status = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept failed, %d: unknown", status);
				}

				if (status != netp::OK) {
					ch->ch_errno() = status;
					ch->ch_flag() |= int(channel_flag::F_READ_ERROR);
					ch->ch_close_impl(nullptr);
					NETP_ERR("[socket][%s]accept failed: %d", ch->ch_info().c_str(), status);
					return;
				}

				ch->ch_set_connected();
				ch->ch_fire_connected();
				ch->ch_io_read();
			});
		}

		void __do_io_accept(NRP<socket_cfg> const& cfg, fn_channel_initializer_t const& fn_initializer, int status, io_ctx* ctx);

		__NETP_FORCE_INLINE void ___do_io_read_done(int status) {
			switch (status) {
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
				NETP_ASSERT(status < 0);
				ch_io_end_read();
				m_chflag |= int(channel_flag::F_READ_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
				ch_errno() = (status);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]___io_read_impl_done, _ch_do_close_read_write, read error: %d, close, flag: %u", ch_info().c_str(), status, m_chflag);
			}
			}
		}

		void __do_io_read_from(int status, io_ctx* ctx);
		void __do_io_read(int status, io_ctx* ctx);

		inline void __do_io_write_done(const int status) {
			switch (status) {
			case netp::OK:
			{
				NETP_ASSERT((m_chflag & int(channel_flag::F_BDLIMIT)) == 0);
				NETP_ASSERT(m_outbound_entry_q.size() == 0);
				if (m_chflag & int(channel_flag::F_CLOSE_PENDING)) {
					_ch_do_close_read_write();
					NETP_TRACE_SOCKET("[socket][%s]IO_WRITE, end F_CLOSE_PENDING, _ch_do_close_read_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
				}
				else if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
					_ch_do_close_write();
					NETP_TRACE_SOCKET("[socket][%s]IO_WRITE, end F_WRITE_SHUTDOWN_PENDING, ch_close_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
				}
				else {
					std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>>().swap(m_outbound_entry_q);
					ch_io_end_write();
				}
			}
			break;
			case netp::E_SOCKET_WRITE_BLOCK:
			{
				NETP_ASSERT(m_outbound_entry_q.size() > 0);
#ifdef NETP_ENABLE_FAST_WRITE
				NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER) | int(channel_flag::F_WATCH_WRITE)));
				ch_io_write();
#else
				NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_WRITE));
#endif
				//NETP_TRACE_SOCKET("[socket][%s]__do_io_write, write block", info().c_str());
			}
			break;
			case netp::E_CHANNEL_BDLIMIT:
			{
				m_chflag |= int(channel_flag::F_BDLIMIT);
				ch_io_end_write();
			}
			break;
			default:
			{
				ch_io_end_write();
				m_chflag |= int(channel_flag::F_WRITE_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
				ch_errno() = (status);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]__do_io_write, call_ch_do_close_read_write, write error: %d, m_chflag: %u", ch_info().c_str(), status, m_chflag);
			}
			break;
			}
		}
		void __do_io_write(int status, io_ctx* ctx);

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

		void ch_write_impl(NRP<packet> const& outlet, NRP<promise<int>> const& chp) override;
		void ch_write_to_impl(NRP<packet> const& outlet, netp::address const& to, NRP<promise<int>> const& chp) override;

		void ch_close_read_impl(NRP<promise<int>> const& closep) override
		{
			NETP_ASSERT(L->in_event_loop());
			NETP_TRACE_SOCKET("[socket][%s]ch_close_read_impl, _ch_do_close_read, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			int prt = netp::OK;
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING))) {
				prt = (netp::E_OP_INPROCESS);
			} else if ((m_chflag & int(channel_flag::F_READ_SHUTDOWN)) != 0) {
				prt = (netp::E_CHANNEL_WRITE_CLOSED);
			} else {
				_ch_do_close_read();
			}
			if (closep) { closep->set(prt); }
		}

		void ch_close_write_impl(NRP<promise<int>> const& chp) override;
		void ch_close_impl(NRP<promise<int>> const& chp) override;

		//for io monitor
		virtual void io_notify_terminating(int status, io_ctx*) override;
		virtual void io_notify_read(int status, io_ctx* ctx) override;
		virtual void io_notify_write(int status, io_ctx* ctx) override;
		
		virtual void __ch_clean();
	public:
		void ch_io_begin(fn_io_event_t const& fn_begin_done) override;
		void ch_io_end() override;

		void ch_io_accept(fn_io_event_t const& fn = nullptr) override;
		void ch_io_end_accept() override {
			ch_io_end_read();
		}

		void ch_io_read(fn_io_event_t const& fn_read = nullptr) override;
		void ch_io_end_read() override;
		void ch_io_write(fn_io_event_t const& fn_write = nullptr) override;
		void ch_io_end_write() override;

		void ch_io_connect(fn_io_event_t const& fn = nullptr) override {
			NETP_ASSERT(fn != nullptr);
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
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
					chp->set(S->get_rcv_buffer_size());
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
					chp->set(S->get_snd_buffer_size());
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
			std::string ch_info() const override {
				return socketinfo{ m_fd, (m_family),(m_type),(m_protocol),local_addr(), remote_addr() }.to_string();
			}
			void ch_set_bdlimit(netp::u32_t limit) override {
				L->execute([s = NRP<socket_channel>(this), limit]() {
					s->m_outbound_limit = limit;
					s->m_outbound_budget = s->m_outbound_limit;
				});
			};
	};
}
#endif