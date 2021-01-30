#ifndef _NETP_ADDRESS_HPP_
#define _NETP_ADDRESS_HPP_

#include <string>
#include <vector>
#include <netp/core.hpp>
#include <netp/ipv4.hpp>
#include <netp/ipv6.hpp>
#include <netp/string.hpp>

namespace netp {

	#define NETP_PF_INET 		PF_INET
	#define NETP_AF_INET		AF_INET
	#define NETP_AF_INET6		AF_INET6
	#define NETP_AF_UNIX		AF_UNIX
	#define NETP_PF_UNIX		PF_UNIX
	#define NETP_AF_UNSPEC	AF_UNSPEC
	#define NETP_AF_USER		(~0)


	#define NETP_SOCK_STREAM				SOCK_STREAM
	#define NETP_SOCK_DGRAM				SOCK_DGRAM
	#define NETP_SOCK_RAW					SOCK_RAW
	#define NETP_SOCK_RDM					SOCK_RDM
	#define NETP_SOCK_SEQPACKET		SOCK_SEQPACKET
	#define NETP_SOCK_USERPACKET					(SOCK_SEQPACKET+100)
/*
	enum s_protocol {
		P_TCP,
		P_UDP,
		P_ICMP,
		P_IGMP,
		P_SCTP,
		P_RAW,
		P_USER,
		P_MAX
	};
	*/

	#define NETP_PROTOCOL_TCP		0
	#define NETP_PROTOCOL_UDP		1
	#define NETP_PROTOCOL_ICMP	2
	#define NETP_PROTOCOL_IGMP	3
	#define NETP_PROTOCOL_SCTP		4
	#define NETP_PROTOCOL_RAW		5
	#define NETP_PROTOCOL_USER		6
	#define NETP_PROTOCOL_MAX		7


	extern const char* DEF_protocol_str[NETP_PROTOCOL_MAX];
	extern const int OS_DEF_protocol[NETP_PROTOCOL_MAX];
	extern const int DEF_protocol_str_to_proto(const char* protostr);

	enum class addrinfo_filter {
		AIF_ALL			= 0x0, //return all
		AIF_F_INET		= 0x01, //return only ipv4
		AIF_F_INET6		= 0x02, //return only ipv6
		AIF_ST_STREAM	= 0x04,
		AIF_ST_DGRAM	= 0x08,
		AIF_P_TCP		= 0x10,
		AIF_P_UDP		= 0x20
	};

	const ipv4_t IP_LOOPBACK = 2130706433U;

	struct address final {
		ipv4_t m_ipv4;
		port_t m_port;
		u8_t m_family;

		address();
		address(const char* ip, unsigned short port, int f = NETP_AF_UNSPEC);
		address( sockaddr_in const& sockaddr_in_ ) ;

		~address();

		inline bool is_null() const {return 0 == m_ipv4 && 0 == m_port ;}
		inline u64_t hash() const {
			return (u64_t(m_ipv4) << 24 | u64_t(m_port) << 8 | u64_t(m_family));
		}
		inline bool operator == ( address const& addr ) const {
			return hash() == addr.hash();
		}

		inline bool operator != ( address const& addr ) const {
			return !((*this) == addr);
		}

		inline bool operator < (address const& addr) const {
			return hash() < addr.hash();
		}
		inline bool operator > (address const& addr) const {
			return !((*this) < addr);
		}

		inline int family() const {
			return int(m_family);
		}

		const string_t dotip() const ;

		inline ipv4_t ipv4() const {
			return m_ipv4;
		}
		inline ipv4_t hipv4() const {
			return ipv4();
		}
		inline ipv4_t nipv4() const {
			return ::htonl(m_ipv4);
		}
		inline port_t port() const {
			return m_port;
		}
		inline port_t hport() const {
			return port();
		}
		inline port_t nport() const {
			return htons(m_port);
		}
		inline void setipv4(ipv4_t ip) {
			m_ipv4 = ip;
		}
		inline void setport(port_t port) {
			m_port = port;
		}
		inline void setfamily(int f) {
			m_family = u8_t(f);
		}
		string_t to_string() const;
	};

	struct address_hash {
		inline u64_t operator()(address const& addr) const
		{
			return addr.hash();
		}
	};

	struct address_equal {
		inline bool operator()(address const& lhs, address const& rhs) const
		{
			return lhs == rhs;
		}
	};

	extern int get_iplist_by_host(char const* const hostname, char const* const servicename, std::vector<string_t>& ips, int const& filter = int(addrinfo_filter::AIF_F_INET));
	extern int get_ip_by_host(const char* hostname, string_t& ip, int const& filter = int(addrinfo_filter::AIF_F_INET));

	extern ipv4_t dotiptonip(const char* dotip);
	extern ipv4_t dotiptoip(const char* dotip);
	extern ipv4_t hosttoip(const char* hostname);

	extern bool is_dotipv4_decimal_notation(const char* string);

	inline ipv4_t ipv4tonipv4(ipv4_t const& ip) { return ::htonl(ip); }
	inline ipv4_t nipv4toipv4(ipv4_t const& ip) { return ::ntohl(ip); }

	extern string_t ipv4todotip(ipv4_t const& ip);
	inline string_t nipv4todotip(ipv4_t const& ip) { return ipv4todotip(nipv4toipv4(ip)); }
	__NETP_FORCE_INLINE netp::u32_t ipv4_t4h(ipv4_t lip, port_t lport, ipv4_t rip, port_t rport) {
		return ((netp::u32_t)(lip) * 59) ^
			((netp::u32_t)(rip)) ^
			((netp::u32_t)(lport) << 16) ^
			((netp::u32_t)(rport))
			;
	}
}
#endif //_ADDRESS_H