#include <netp/string.hpp>
#include <netp/address.hpp>

namespace netp {

	const char* DEF_protocol_str[NETP_PROTOCOL_MAX] = {
		"TCP",
		"UDP",
		"ICMP",
		"IGMP",
		"SCTP",
		"RAW",
		"USER"
	};

	const u16_t OS_DEF_protocol[NETP_PROTOCOL_MAX] = {
		IPPROTO_TCP,
		IPPROTO_UDP,
		IPPROTO_ICMP,
		IPPROTO_IGMP,
		IPPROTO_SCTP,
		IPPROTO_RAW,
		u16_t(~0)
	};

	const u16_t DEF_protocol_str_to_proto(const char* protostr) {
		if (netp::iequals<string_t>(protostr, string_t("tcp"))) {
			return NETP_PROTOCOL_TCP;
		} else if (netp::iequals<string_t>(protostr, string_t("udp"))) {
			return NETP_PROTOCOL_UDP;
		} else {
			return NETP_PROTOCOL_USER;
		}
	}

	/**
	 * @param hostname, domain name of the host, example: www.google.com
	 * @param servicename, ftp, http, etc..
	 *		for a list of service names , u could refer to %WINDOW%/system32/drivers/etc/services on windows
	 *
	 */
	int get_iplist_by_host( char const* const hostname, char const* const servicename, std::vector<string_t>& ips, int const& filter ) {

		NETP_ASSERT( (hostname != nullptr && netp::strlen(hostname)) ||
					 (servicename != nullptr && netp::strlen(servicename))
					);

		struct addrinfo hint;
		::memset( &hint, 0, sizeof(hint));
		struct addrinfo* result = nullptr;
		struct addrinfo* ptr = nullptr;

		if( filter != 0 ) {
			int aif_family = (filter&(int(addrinfo_filter::AIF_F_INET)|int(addrinfo_filter::AIF_F_INET6)));
			if( aif_family == int(addrinfo_filter::AIF_F_INET) ) {
				hint.ai_family = AF_INET; //
			} else if( aif_family == int(addrinfo_filter::AIF_F_INET6)) {
				hint.ai_family = AF_INET6;
			} else {
				hint.ai_family = AF_UNSPEC;
			}

			int aif_sockt = (filter&( int(addrinfo_filter::AIF_ST_DGRAM)|int(addrinfo_filter::AIF_ST_STREAM)));
			if( aif_sockt == int(addrinfo_filter::AIF_ST_STREAM)) {
				hint.ai_socktype = SOCK_STREAM;
			} else if( aif_sockt == int(addrinfo_filter::AIF_ST_DGRAM)) {
				hint.ai_socktype = SOCK_DGRAM;
			} else {
				hint.ai_socktype = 0;
			}

			int aif_protocol = (filter&(int(addrinfo_filter::AIF_P_TCP)|int(addrinfo_filter::AIF_P_UDP)));
			if( aif_protocol == int(addrinfo_filter::AIF_P_TCP)) {
				hint.ai_protocol = IPPROTO_TCP;
			} else if( aif_protocol == int(addrinfo_filter::AIF_P_UDP)) {
				hint.ai_protocol = IPPROTO_UDP;
			} else {
				hint.ai_protocol = 0;
			}
		}

#ifdef _NETP_WIN
		DWORD retval;
#else
		int retval;
#endif

		retval = getaddrinfo( hostname, servicename, &hint, &result );

		if( retval != 0 ) {
			return retval;
		}

		for( ptr=result;ptr!=nullptr; ptr = ptr->ai_next ) {

			NRP<address> addr;
			int f;
			switch( ptr->ai_family ) {
			case AF_UNSPEC:
				{
					f = NETP_AF_UNSPEC;
					//NETP_ASSERT( !"to impl" );
				}
				break;
			case AF_INET:
				{
					f = NETP_AF_INET;

					char addrv4_cstr[16] = {0};
					struct sockaddr_in* addrv4 = (struct sockaddr_in*) ptr->ai_addr;
					const char* addr_in_cstr = inet_ntop(AF_INET, (void*)&(addrv4->sin_addr), addrv4_cstr, 16 );
					//char* addrv4_cstr = inet_ntoa( addrv4->sin_addr );

					if(addr_in_cstr != nullptr ) {
						addr = netp::make_ref<address>(addrv4_cstr,u16_t(0), f);
						ips.push_back( addr->dotip() );
					}
				}
				break;
			case AF_INET6:
				{
					f = NETP_AF_INET6;
					//NETP_ASSERT( !"to impl" );
				}
				break;
			default:
				{
					f = NETP_AF_UNSPEC;
					//NETP_ASSERT( !"to impl" );
				}
				break;
			}
		}
		freeaddrinfo(result);
		return netp::OK;
	}

	extern int get_ip_by_host( const char* hostname, string_t& ip_o, int const& filter ) {
		std::vector<string_t> infos;
		int retval = get_iplist_by_host( hostname, "", infos, filter );

		if( retval != 0 ) {
			return retval;
		}

		NETP_ASSERT( infos.size() != 0 );
		ip_o = infos[0] ;
		return netp::OK;
	}

	ipv4_t dotiptonip(const char* ipaddr) {
		NETP_ASSERT(strlen(ipaddr) > 0);
		struct in_addr inaddr;
		int ret = ::inet_pton(AF_INET, ipaddr, &inaddr);
		switch (ret) {
			case 1:
			{
				return inaddr.s_addr;
			}
			break;
			case 0:
			{
				return 0;
			}
			break;
			case -1:
			default:
			{
			}
			break;
		}
		 return 0;
	}

	ipv4_t dotiptoip(const char* ipaddr) {
		return ntohl(dotiptonip(ipaddr));
	}

	ipv4_t hosttoip( const char* hostname ) {
		NETP_ASSERT( strlen(hostname) > 0 );
		string_t dotip;
		int ec = get_ip_by_host( hostname, dotip, 0 );
		NETP_RETURN_V_IF_NOT_MATCH( 0, ec ==netp::OK );
		return dotiptoip(dotip.c_str());
	}

	bool is_dotipv4_decimal_notation(const char* cstr) {
		struct in_addr inaddr;
		int rval = ::inet_pton(AF_INET, cstr, &inaddr);
		if (rval == 0) return false;

		std::vector<string_t> vectors;
		netp::split<string_t>(string_t(cstr), ".", vectors);

		if (vectors.size() != 4) return false;

		char _tmp[32] = { 0 };
		snprintf(_tmp, 32, "%u.%u.%u.%u", netp::to_u32(vectors[0].c_str()), netp::to_u32(vectors[1].c_str()), netp::to_u32(vectors[2].c_str()), netp::to_u32(vectors[3].c_str()));

		return netp::strcmp(_tmp, cstr) == 0;
	}

	string_t nipv4todotip(ipv4_t const& nip) { 
		in_addr ia;
		ia.s_addr = nip;
		char addr[16] = { 0 };
		const char* addr_cstr = ::inet_ntop(AF_INET, &ia, addr, 16);
		if (NETP_LIKELY(addr_cstr != nullptr)) {
			return string_t(addr);
		}
		return string_t();
	}


	address::address()
	{
		std::memset((void*)&m_in,0,sizeof(sockaddr_in));
		std::memset((void*)&m_in6,0,sizeof(sockaddr_in6));
		m_in.sin_family = u16_t(NETP_AF_UNSPEC);
		m_in6.sin6_family = u16_t(NETP_AF_UNSPEC);
	}

	address::address( char const* ip, unsigned short port , int f)
	{
		NETP_ASSERT(f < 255);
		NETP_ASSERT( ip != nullptr && netp::strlen(ip) );
		m_in.sin_port = htons(port);
		m_in.sin_family = u8_t(f);
		m_in.sin_addr.s_addr = dotiptonip(ip);
	}

	address::address( const struct sockaddr_in* sockaddr_in_, size_t slen )
	{
		NETP_ASSERT(slen == sizeof(sockaddr_in));
		std::memcpy(&m_in, sockaddr_in_, slen);
	}

	address::address(const struct sockaddr_in6* sockaddr_in6_, size_t slen)
	{
		NETP_ASSERT(slen == sizeof(sockaddr_in6));
		std::memcpy(&m_in6, sockaddr_in6_, slen);
	}

	address::~address() {}

	const string_t address::dotip() const {
		return nipv4todotip(m_in.sin_addr.s_addr);
	}

	string_t address::to_string() const {
		char info[32] = { 0 };
		int rtval = snprintf(const_cast<char*>(info), sizeof(info) / sizeof(info[0]), "%s:%d", dotip().c_str(), hport());
		NETP_ASSERT(rtval > 0);
		(void)rtval;
		return string_t(info, netp::strlen(info));
	}
}