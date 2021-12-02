#ifndef _NETP_ADDRESS_HPP_
#define _NETP_ADDRESS_HPP_

#include <string>
#include <vector>
#include <netp/core.hpp>
#include <netp/ipv4.hpp>
#include <netp/ipv6.hpp>
#include <netp/string.hpp>

#include <netp/smart_ptr.hpp>

#define NETP_PF_INET 		PF_INET
#define NETP_AF_INET		AF_INET
#define NETP_AF_INET6		AF_INET6
#define NETP_AF_UNIX		AF_UNIX
#define NETP_PF_UNIX		PF_UNIX
#define NETP_AF_UNSPEC	AF_UNSPEC
#define NETP_AF_USER		(AF_MAX+2)


#define NETP_SOCK_STREAM				SOCK_STREAM
#define NETP_SOCK_DGRAM				SOCK_DGRAM
#define NETP_SOCK_RAW					SOCK_RAW
#define NETP_SOCK_RDM					SOCK_RDM
#define NETP_SOCK_SEQPACKET		SOCK_SEQPACKET
#define NETP_SOCK_USERPACKET		(SOCK_SEQPACKET+100)

#define NETP_PROTOCOL_TCP		0
#define NETP_PROTOCOL_UDP		1
#define NETP_PROTOCOL_ICMP	2
#define NETP_PROTOCOL_IGMP	3
#define NETP_PROTOCOL_SCTP		4
#define NETP_PROTOCOL_RAW		5
#define NETP_PROTOCOL_USER		6
#define NETP_PROTOCOL_MAX		7

namespace netp {

	extern const char* DEF_protocol_str[NETP_PROTOCOL_MAX];
	extern const u16_t OS_DEF_protocol[NETP_PROTOCOL_MAX];
	extern const u16_t DEF_protocol_str_to_proto(const char* protostr);

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
	enum class address_type {
		t_empty,
		t_ipv4,
		t_ipv6
	};
	extern int get_iplist_by_host(char const* const hostname, char const* const servicename, std::vector<string_t>& ips, int const& filter = int(addrinfo_filter::AIF_F_INET));
	extern int get_ip_by_host(const char* hostname, string_t& ip, int const& filter = int(addrinfo_filter::AIF_F_INET));

	extern ipv4_t dotiptonip(const char* dotip);
	extern ipv4_t dotiptoip(const char* dotip);
	extern ipv4_t hosttoip(const char* hostname);

	extern bool is_dotipv4_decimal_notation(const char* string);

	inline ipv4_t ipv4tonipv4(ipv4_t const& ip) { return htonl(ip); }
	inline ipv4_t nipv4toipv4(ipv4_t const& ip) { return ntohl(ip); }

	extern string_t nipv4todotip(ipv4_t const& ip);
	inline string_t ipv4todotip(ipv4_t const& ip) { return nipv4todotip(htonl(ip)); }

	__NETP_FORCE_INLINE netp::u32_t ipv4_t4h(ipv4_t lip, port_t lport, ipv4_t rip, port_t rport) {
		return ((netp::u32_t)(lip) * 59) ^
			((netp::u32_t)(rip)) ^
			((netp::u32_t)(lport) << 16) ^
			((netp::u32_t)(rport))
			;
	}


	//::1 - IPv6  loopback
	//127.0.0.0 - 127.255.255.255  (127 / 8 prefix)
	inline bool is_loopback(ipv4_t v4_/*nip*/) {
		ipv4_u4 _ipv4_u4 = { v4_ };
		return _ipv4_u4.u4.u1 == 127;
	}

	/* An IP should be considered as internal when
	10.0.0.0     -   10.255.255.255  (10/8 prefix)
	172.16.0.0   -   172.31.255.255  (172.16/12 prefix)
	192.168.0.0  -   192.168.255.255 (192.168/16 prefix)
	*/
	inline bool is_rfc1918(ipv4_t v4_/*nip*/) {
		ipv4_u4 _ipv4_u4 = { v4_ };
		switch (_ipv4_u4.u4.u1) {
		case 10:
			return true;
		case 172:
			return _ipv4_u4.u4.u2 >= 16 && _ipv4_u4.u4.u2 < 32;
		case 192:
			return _ipv4_u4.u4.u2 == 168;
		default:
			return false;
		}
	}

	inline bool is_internal(ipv4_t v4_ /*nip*/) {
		return is_loopback(v4_) || is_rfc1918(v4_);
	}

	struct address final :
		public netp::ref_base
	{
		sockaddr_in m_in;
		sockaddr_in6 m_in6;

		address();
		address(const char* ip, unsigned short port, int f );
		address(const struct sockaddr_in* sockaddr_in_, size_t slen);
		address(const struct sockaddr_in6* sockaddr_in6_, size_t slen);

		~address();

		inline bool is_loopback() {
			if (m_in.sin_family != NETP_AF_UNSPEC) {
				return netp::is_loopback( m_in.sin_addr.s_addr );
			}
			NETP_TODO("IPV6");
		}

		inline bool is_rfc1918() {
			if (m_in.sin_family != NETP_AF_UNSPEC) {
				return netp::is_internal(m_in.sin_addr.s_addr);
			}
			NETP_TODO("IPV6");
		}

		inline bool is_internal() {
			return is_loopback() || is_rfc1918();
		}

		struct sockaddr* sockaddr_v4() {
			return (struct sockaddr*)(&m_in);
		}

		struct sockaddr* sockaddr_v6() {
			return (struct sockaddr*)(&m_in6);
		}

		NRP<address> clone() const {
			NRP<address> a = netp::make_ref<address>();
			std::memcpy(&(a->m_in), &m_in, sizeof(sockaddr_in));
			std::memcpy(&(a->m_in6), &m_in6, sizeof(sockaddr_in6));
			return a;
		}

		//@removed on 2021-12-2
		//@deprecated
		//inline bool is_null() const { return is_af_unspec(); }

		inline bool is_af_unspec() const {
			return m_in.sin_family== NETP_AF_UNSPEC && m_in6.sin6_family == NETP_AF_UNSPEC;
		}

		inline u64_t hash() const {
			return (u64_t(m_in.sin_addr.s_addr) << 24 | u64_t(m_in.sin_port) << 8 | u64_t(m_in.sin_family));
		}

		inline bool operator == (address const& addr) const {
			return hash() == addr.hash();
		}

		inline bool operator != (address const& addr) const {
			return !((*this) == addr);
		}

		inline bool operator < (address const& addr) const {
			return hash() < addr.hash();
		}
		inline bool operator > (address const& addr) const {
			return !((*this) < addr);
		}

		inline u16_t family() const {
			return u16_t(m_in.sin_family);
		}

		const string_t dotip() const;

		inline ipv4_t ipv4() const {
			return ntohl(m_in.sin_addr.s_addr);
		}
		inline ipv4_t hipv4() const {
			return ipv4();
		}
		inline ipv4_t nipv4() const {
			return (m_in.sin_addr.s_addr);
		}
		inline port_t port() const {
			return ntohs(m_in.sin_port);
		}
		inline port_t hport() const {
			return port();
		}
		inline port_t nport() const {
			return (m_in.sin_port);
		}
		inline void setipv4(ipv4_t ip) {
			m_in.sin_addr.s_addr = htonl(ip);
		}
		inline void setport(port_t port) {
			m_in.sin_port = htons(port);
		}
		inline void setfamily(u16_t f) {
			NETP_ASSERT(f < 255);
			m_in.sin_family = u8_t(f);
		}
		string_t to_string() const;
	};

	struct address_hash {
		__NETP_FORCE_INLINE u64_t operator()(NRP<address> const& addr) const
		{
			return addr->hash();
		}
	};

	struct address_equal {
		__NETP_FORCE_INLINE bool operator()(NRP<address> const& lhs, NRP<address> const& rhs) const
		{
			return lhs->hash() == rhs->hash();
		}
	};
}
#endif //_ADDRESS_H