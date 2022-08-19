#ifndef _NETP_ADDRESS_HPP_
#define _NETP_ADDRESS_HPP_

#include <string>
#include <vector>
#include <netp/core.hpp>
#include <netp/string.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/ip.hpp>

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
#define NETP_SOCK_UNKNOWN		(NETP_SOCK_USERPACKET+1)

#define NETP_PROTOCOL_TCP				0
#define NETP_PROTOCOL_UDP				1
#define NETP_PROTOCOL_ICMP			2
#define NETP_PROTOCOL_IGMP			3
#define NETP_PROTOCOL_SCTP				4
#define NETP_PROTOCOL_RAW				5
#define NETP_PROTOCOL_BFR_TCP		6
#define NETP_PROTOCOL_USER				7
#define NETP_PROTOCOL_MAX				8
#define NETP_PROTOCOL_UNKNOWN	9

namespace netp {

	extern const char* NETP_PROTO_MAP_PROTO_STR[NETP_PROTOCOL_MAX+1];
	extern const u16_t NETP_PROTO_MAP_OS_PROTO[NETP_PROTOCOL_MAX+1];

	extern const u16_t proto_str_to_netp_proto(const char* proto_str);
	extern const char* netp_proto_to_proto_str(u16_t netp_proto);
	extern const u16_t proto_str_to_os_proto(const char* proto_str);
	extern const char* os_proto_to_proto_str(u16_t os_proto);

	enum class addrinfo_filter {
		AIF_ALL			= 0x0, //return all
		AIF_F_INET		= 0x01, //return only ipv4
		AIF_F_INET6		= 0x02, //return only ipv6
		AIF_ST_STREAM	= 0x04,
		AIF_ST_DGRAM	= 0x08,
		AIF_P_TCP		= 0x10,
		AIF_P_UDP		= 0x20
	};

	enum class address_type {
		t_empty,
		t_ipv4,
		t_ipv6
	};

	extern int get_iplist_by_host(char const* const hostname, char const* const servicename, std::vector<string_t>& ips, int const& filter = int(addrinfo_filter::AIF_F_INET));
	extern int get_ip_by_host(const char* hostname, string_t& ip, int const& filter = int(addrinfo_filter::AIF_F_INET));
	extern ipv4_t hosttoip(const char* hostname);

	struct address final :
		public netp::ref_base
	{
		sockaddr_in m_in;
		sockaddr_in6 m_in6;

		address();
		address(const char* ip, unsigned short port, int f );
		address(const struct sockaddr_in* sockaddr_in_, size_t slen);
		address(const struct sockaddr_in6* sockaddr_in6_, size_t slen);
		address(ipv4_t ip, port_t port, int f);
		~address();

		inline bool is_loopback() {
			if (m_in.sin_family != NETP_AF_UNSPEC) {
				return netp::is_loopback({ m_in.sin_addr.s_addr });
			}
			NETP_TODO("IPV6");
		}

		inline bool is_rfc1918() {
			if (m_in.sin_family != NETP_AF_UNSPEC) {
				return netp::is_rfc1918({ m_in.sin_addr.s_addr });
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
			//@TODO ipv6 todo
			NETP_ASSERT( m_in.sin_family != AF_INET6);
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
			return { NETP_NTOHL(m_in.sin_addr.s_addr) };
		}
		inline ipv4_t hipv4() const {
			return ipv4();
		}
		inline ipv4_t nipv4() const {
			return {(m_in.sin_addr.s_addr)};
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
			m_in.sin_addr.s_addr = NETP_HTONL(ip.u32);
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