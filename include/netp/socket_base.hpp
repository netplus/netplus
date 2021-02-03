#ifndef _NETP_SOCKET_BASE_HPP
#define _NETP_SOCKET_BASE_HPP

#include <netp/core.hpp>
#include <netp/address.hpp>
#include <netp/socket_api.hpp>
#include <netp/channel.hpp>

namespace netp {

	enum socket_option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 1, //only for UDP
		OPTION_REUSEADDR = 1<<1,
		OPTION_REUSEPORT = 1<<2,
		OPTION_NON_BLOCKING = 1<<3,
		OPTION_NODELAY = 1<<4, //only for TCP
		OPTION_KEEP_ALIVE = 1<<5
	};

	const static int default_socket_option = int(socket_option::OPTION_NON_BLOCKING)|int(socket_option::OPTION_KEEP_ALIVE);

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

	class socket_base
	{
	protected:
		SOCKET m_fd;

		u8_t m_family;
		u8_t m_type;
		u16_t m_protocol;
		u16_t m_option;

		const socket_api* m_api;

		address m_laddr;
		address m_raddr;

		keep_alive_vals m_kvals;
		channel_buf_cfg m_sock_buf;

	protected:
		int _cfg_reuseaddr(bool onoff);
		int _cfg_reuseport(bool onoff);
		int _cfg_nonblocking(bool onoff);

		int _cfg_buffer(channel_buf_cfg const& bcfg );
		int _cfg_nodelay(bool onoff);
		int __cfg_keepalive_vals(keep_alive_vals const& vals);
		int _cfg_keepalive(bool onoff, keep_alive_vals const& vals);

		int _cfg_broadcast(bool onoff);
		int _cfg_option(u16_t opt, keep_alive_vals const& vlas);

		int init(u16_t opt, keep_alive_vals const& kvals, channel_buf_cfg const& cbc) {
			int rt = _cfg_option(opt,kvals);
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
			return _cfg_buffer(cbc);
		}

	public:
		int open();
		int shutdown(int flag);
		int close();
		int bind(address const& addr);
		int listen(int backlog);
		SOCKET accept(address& addr);
		int connect(address const& addr);

	public:
		socket_base(SOCKET fd, int family, int sockt, int proto, address const& laddr, address const& raddr, const socket_api* sockapi); //by pass a connected socket fd
		~socket_base();

		__NETP_FORCE_INLINE int sock_family() const { return ((m_family)); };
		__NETP_FORCE_INLINE int sock_type() const { return (m_type); };
		__NETP_FORCE_INLINE int sock_protocol() { return (m_protocol); };

		__NETP_FORCE_INLINE bool is_tcp() const { return m_protocol == u8_t(NETP_PROTOCOL_TCP); }
		__NETP_FORCE_INLINE bool is_udp() const { return m_protocol == u8_t(NETP_PROTOCOL_UDP); }
		__NETP_FORCE_INLINE bool is_icmp() const { return m_protocol == u8_t(NETP_PROTOCOL_ICMP); }

		__NETP_FORCE_INLINE SOCKET fd() const { return m_fd; }
		__NETP_FORCE_INLINE address const& remote_addr() const { return m_raddr; }
		__NETP_FORCE_INLINE address const& local_addr() const { return m_laddr; }

		int load_sockname() {
			int rt = netp::getsockname(*m_api,m_fd, m_laddr);
			if (rt == netp::OK) {
				NETP_ASSERT(m_laddr.family() == NETP_AF_INET);
				NETP_ASSERT(!m_laddr.is_null());
				return netp::OK;
			}
			return netp_socket_get_last_errno();
		}

		//load_peername always succeed on win10
		int load_peername() {
			int rt = netp::getpeername(*m_api,m_fd, m_raddr);
			if (rt == netp::OK) {
				NETP_ASSERT(m_raddr.family() == NETP_AF_INET);
				NETP_ASSERT(!m_laddr.is_null());
				return netp::OK;
			}
			return netp_socket_get_last_errno();
		}

		std::string info() const {
			return socketinfo{ m_fd, (m_family),(m_type),(m_protocol),local_addr(), remote_addr() }.to_string();
		}

		__NETP_FORCE_INLINE int getsockopt(int level, int option_name, void* value, socklen_t* option_len) const {
			return m_api->getsockopt(m_fd, level, option_name, value, option_len);
		}

		__NETP_FORCE_INLINE int setsockopt(int level, int option_name, void const* value, socklen_t const& option_len) {
			return m_api->setsockopt(m_fd, level, option_name, value, option_len);
		}

		__NETP_FORCE_INLINE int turnon_nodelay() { return _cfg_nodelay(true); }
		__NETP_FORCE_INLINE int turnoff_nodelay() { return _cfg_nodelay(false); }

		__NETP_FORCE_INLINE int turnon_nonblocking() { return _cfg_nonblocking(true); }
		__NETP_FORCE_INLINE int turnoff_nonblocking() { return _cfg_nonblocking(false); }
		__NETP_FORCE_INLINE bool is_nonblocking() const { return ((m_option&u8_t(socket_option::OPTION_NON_BLOCKING)) != 0); }

		__NETP_FORCE_INLINE int reuse_addr() { return _cfg_reuseaddr(true); }
		__NETP_FORCE_INLINE int reuse_port() { return _cfg_reuseport(true); }

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
		int set_keep_alive_vals(bool onoff, keep_alive_vals const& vals) { return _cfg_keepalive(onoff,vals); }

		int get_tos(u8_t& tos) const;
		int set_tos(u8_t tos);

		__NETP_FORCE_INLINE netp::u32_t send(byte_t const* const buffer, netp::u32_t size, int& ec_o, int flag = 0) {
			return netp::send(*m_api,m_fd, buffer, size, ec_o, flag);
		}
		__NETP_FORCE_INLINE netp::u32_t recv(byte_t* const buffer_o, netp::u32_t size, int& ec_o, int flag = 0) {
			return netp::recv(*m_api,m_fd, buffer_o, size, ec_o, flag);
		}

		__NETP_FORCE_INLINE netp::u32_t sendto(netp::byte_t const* const buffer, netp::u32_t len, const netp::address& addr, int& ec_o, int flag = 0) {
			return netp::sendto(*m_api,m_fd, buffer, len, addr, ec_o, flag);
		}

		__NETP_FORCE_INLINE netp::u32_t recvfrom(byte_t* const buffer_o, netp::u32_t size, address& addr_o, int& ec_o) {
			return netp::recvfrom(*m_api,m_fd, buffer_o, size, addr_o, ec_o, 0);
		}

		__NETP_FORCE_INLINE netp::u32_t recvonemsg(byte_t* const buff_o, netp::u32_t size, address& addr_o, ipv4_t& lipv4, int& ec_o, int flag) {
			return m_api->recvonemsg(m_fd, buff_o, size, addr_o, lipv4, ec_o,flag );
		}
	};
}
#endif
