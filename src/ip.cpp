
#include <netp/ip.hpp>

namespace netp {

	string_t niptostring(ip_version vx, ip_t const& ip)
	{
		if(ip_version::v4 == vx)
		{
			return nipv4todotip(ip.v4);
		}
		else if(ip_version::v6 == vx)
		{
			return nipv6tov6string(ip.v6);
		}
		return "";
	}

	string_t iptostring(ip_version vx, ip_t const& ip) 
	{
		if(ip_version::v4 == vx)
		{
			return ipv4todotip(ip.v4);
		}
		else if(ip_version::v6 == vx)
		{
			return ipv6tov6string(ip.v6);
		}
		
		return "";
	}

	ip_t stringtonip(ip_version vx, const char* string)
	{
		ip_t _ip = {0};
		if( ip_version::v4 == vx )
		{
			_ip.v4 = dotiptonip(string);
		}
		else if(ip_version::v6 == vx)
		{
			_ip.v6 = v6stringtonip(string);
		}
		return _ip;
	}
	ip_t stringtoip(ip_version vx, const char* string)
	{
		ip_t _ip = {0};
		if( ip_version::v4 == vx )
		{
			_ip.v4 = dotiptoip(string);
		}
		else if(ip_version::v6 == vx)
		{
			_ip.v6 = v6stringtoip(string);
		}
		return _ip;
	}

	ip_t iptonip(ip_version vx, ip_t const& ip)
	{
		ip_t _ip = {0};
		if(ip_version::v4 == vx)
		{
			_ip.v4 = ipv4tonipv4(ip.v4);
		}
		else if(ip_version::v6 == vx)
		{
			_ip.v6 = ipv6tonipv6(ip.v6);
		}
		return _ip;
	}

	ip_t niptoip(ip_version vx, ip_t const& ip)
	{
		ip_t _ip = {0};
		if(ip_version::v4 == vx)
		{
			_ip.v4 = nipv4toipv4(ip.v4);
		}
		else if(ip_version::v6 == vx)
		{
			_ip.v6 = nipv6toipv6(ip.v6);
		}
		return _ip;
	}

	ipv4_t dotiptonip(const char* ipaddr) {
		NETP_ASSERT(ipaddr && netp::strlen(ipaddr) > 0);
		ipv4_t v4 = {0};
		::inet_pton(AF_INET, ipaddr, &v4.inaddr);
		return v4;
	}

	ipv4_t dotiptoip(const char* ipaddr) {
		ipv4_t v4 = dotiptonip(ipaddr);
		v4.u32 = NETP_NTOHL(v4.u32);
		return v4;
	}

	bool is_dotipv4_decimal_notation(const char* cstr) {
		struct in_addr inaddr;
		int rval = ::inet_pton(AF_INET, cstr, &inaddr);
		if (rval == 0) return false;

		std::vector<netp::string_t, netp::allocator<netp::string_t>> vectors;
		netp::split<netp::string_t>(netp::string_t(cstr), netp::string_t("."), vectors);

		if (vectors.size() != 4) { return false; }

		char _tmp[32] = { 0 };
		snprintf(_tmp, 32, "%u.%u.%u.%u", netp::to_u32(vectors[0].c_str()), netp::to_u32(vectors[1].c_str()), netp::to_u32(vectors[2].c_str()), netp::to_u32(vectors[3].c_str()));

		return netp::strcmp(_tmp, cstr) == 0;
	}

	string_t nipv4todotip(ipv4_t const& nip) {
		char addr[16] = { 0 };
		const char* addr_cstr = ::inet_ntop(AF_INET, &nip.inaddr, addr, 16);
		if (NETP_LIKELY(addr_cstr != nullptr)) {
			return string_t(addr);
		}
		return string_t();
	}

	ipv6_t v6stringtonip(const char* v6string) {
		in6_addr in6;
		ip_t ip6;
		int ret = ::inet_pton(AF_INET6, v6string, &in6);
		if (ret == 1) {
			std::memcpy(&(ip6.byte[0]), &(in6.s6_addr), 16);
		} else {
			ip6.v6.u64.A = 0;
			ip6.v6.u64.B = 0;
		}
		return ip6.v6;
	}
	string_t nipv6tov6string(ipv6_t const& v6) {
		in6_addr in6;
		std::memcpy(&(in6.s6_addr), (const char*)(&(v6.byte[0])), 16);
		char v6string[48] = { 0 };
		const char* addr_cstr = ::inet_ntop(AF_INET6, &in6, v6string, 48);
		if (NETP_LIKELY(addr_cstr != nullptr)) {
			return string_t(v6string);
		}
		return string_t();
	}

	//https://en.cppreference.com/w/c/language/operator_arithmetic
	//192.168.0.x/24 -> 192.168.0.0
	void v4_mask_by_prefix( netp::ipv4_t* const v4 /*in host endian*/, netp::u8_t prefix) {
		if (prefix == 0) {//x<<32 on ul is UB
			v4->u32 = 0;
		} else if (prefix <= 32) {
			 v4->u32 = (v4->u32 & (netp::u32_t)(((0xfffffffful) << (32 - prefix))));
		} else {
			/*do nothing*/
			(void) (*v4);
		}
	}

	void v4_mask_by_prefix( netp::ipv4_t* const v4 /*in host endian*/, netp::u8_t prefix, netp::u32_t* host) {
		if( host != 0)
		{
			*host = (v4->u32 & (netp::u32_t)(((0xfffffffful) >> (prefix))));
		}
		v4_mask_by_prefix(v4,prefix);
	}

	void v6_mask_by_prefix( netp::ipv6_t* const v6_ /*in host endian*/, netp::u8_t prefix) {
		if (prefix == 0) {//x<<64 is UB
			v6_->u64.A = 0;
			v6_->u64.B = 0;
		} else if (prefix <= 64) {
			v6_->u64.A = (v6_->u64.A & (0xffffffffffffffffull << (64 - prefix)));
			v6_->u64.B = 0;
		} else if (prefix <= 128) {
			v6_->u64.B = (v6_->u64.B & (0xffffffffffffffffull << (128 - prefix)));
		} else {
			/*do nothing*/
			(void) (*v6_);
		}
	}

	
	void v6_mask_by_prefix( netp::ipv6_t* const v6_ /*in host endian*/, netp::u8_t prefix, netp::u32_t* host) {
		if(host != 0)
		{
			/*not supported for now*/
			*host = 0;
		}
		v6_mask_by_prefix(v6_, prefix);
	}

	int v4_from_cidr_string(const char* cidrstr, netp::ipv4_t* const v4/*stored in host endian*/, netp::u8_t* prefix)
	{
		std::vector<netp::string_t, netp::allocator<netp::string_t>> cidrstr_;
		netp::split<netp::string_t>(netp::string_t(cidrstr, netp::strlen(cidrstr)), netp::string_t("/"), cidrstr_);
		if (cidrstr_.size() != 2) {
			return netp::E_OP_INVALID_ARG;
		}

		*v4 = dotiptoip(cidrstr_[0].c_str());
		(*prefix) = netp::u8_t(netp::to_u32(cidrstr_[1].c_str()));
		return netp::OK;
	}

	int nv4_from_cidr_string(const char* cidrstr, netp::ipv4_t* const v4/*stored in network endian*/, netp::u8_t* prefix)
	{
		std::vector<netp::string_t, netp::allocator<netp::string_t>> cidrstr_;
		netp::split<netp::string_t>(netp::string_t(cidrstr, netp::strlen(cidrstr)), netp::string_t("/"), cidrstr_);
		if (cidrstr_.size() != 2) {
			return netp::E_OP_INVALID_ARG;
		}

		*v4 = dotiptonip(cidrstr_[0].c_str());
		(*prefix) = netp::u8_t(netp::to_u32(cidrstr_[1].c_str()));
		return netp::OK;
	}

	int v6_from_cidr_string(const char* cidrstr, netp::ipv6_t* const v6/*stored in host endian*/, netp::u8_t* prefix)
	{
		std::vector<netp::string_t, netp::allocator<netp::string_t>> cidrstr_;
		netp::split<netp::string_t>(netp::string_t(cidrstr, netp::strlen(cidrstr)), netp::string_t("/"), cidrstr_);
		if (cidrstr_.size() != 2) {
			return netp::E_OP_INVALID_ARG;
		}

		*v6 = v6stringtoip(cidrstr_[0].c_str());
		(*prefix) = netp::u8_t(netp::to_u32(cidrstr_[1].c_str()));
		return netp::OK;
	}
	int nv6_from_cidr_string(const char* cidrstr, netp::ipv6_t* const v6/*stored in network endian*/, netp::u8_t* prefix)
	{
		std::vector<netp::string_t, netp::allocator<netp::string_t>> cidrstr_;
		netp::split<netp::string_t>(netp::string_t(cidrstr, netp::strlen(cidrstr)), netp::string_t("/"), cidrstr_);
		if (cidrstr_.size() != 2) {
			return netp::E_OP_INVALID_ARG;
		}

		*v6 = v6stringtonip(cidrstr_[0].c_str());
		(*prefix) = netp::u8_t(netp::to_u32(cidrstr_[1].c_str()));
		return netp::OK;
	}

	//ip/cidr: 192.168.0.0/16, 1234:0000:2d00:0000:0000:123:73:26b1/64
	int ip_from_cidr_string(netp::ip_version vx,const char* cidr_string, netp::ip_t* const ipbits/*stored in host endian*/, netp::u8_t* prefix) {
		int rt;
		if(ip_version::v4 == vx)
		{
			rt = v4_from_cidr_string(cidr_string, &ipbits->v4, prefix);
		}
		else if(ip_version::v6 == vx)
		{
			rt = v6_from_cidr_string(cidr_string, &ipbits->v6, prefix);
		}
		return rt;
	}

	//ip/cidr: 192.168.0.0/16, 1234:0000:2d00:0000:0000:123:73:26b1/64
	int nip_from_cidr_string(netp::ip_version vx, const char* cidr_string, netp::ip_t* const ipbits/*stored in network endian*/, netp::u8_t* prefix) {
		int rt;
		if(ip_version::v4 == vx)
		{
			rt = nv4_from_cidr_string(cidr_string, &ipbits->v4, prefix);
		}
		else if(ip_version::v6 == vx)
		{
			rt = nv6_from_cidr_string(cidr_string, &ipbits->v6, prefix);
		}
		return rt;
	}
}