
#include <netp/ip.hpp>

namespace netp {
	ipv4_t dotiptonip(const char* ipaddr) {
		NETP_ASSERT(strlen(ipaddr) > 0);
		struct in_addr in4;
		int ret = ::inet_pton(AF_INET, ipaddr, &in4);
		if (ret == 1) {
			return { in4.s_addr };
		}
		return { 0 };
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
		in_addr in4;
		in4.s_addr = nip.u32;
		char addr[16] = { 0 };
		const char* addr_cstr = ::inet_ntop(AF_INET, &in4, addr, 16);
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
			std::memcpy(&(ip6.byte[0]), &(in6.u.Byte[0]), 16);

#ifdef _NETP_DEBUG
			netp::string_t v6s = nipv6tov6string(ip6.v6);
			NETP_ASSERT(netp::strncmp(v6string, v6s.c_str(), v6s.length()) == 0);
#endif
		}
		else {
			ip6.v6.u64.A = 0;
			ip6.v6.u64.B = 0;
		}
		return ip6.v6;
	}
	string_t nipv6tov6string(ipv6_t const& v6) {
		in6_addr in6;
		std::memcpy(&(in6.u.Byte[0]), (const char*)&v6, 16);
		char v6string[64] = { 0 };
		const char* addr_cstr = ::inet_ntop(AF_INET6, &in6, v6string, 64);
		if (NETP_LIKELY(addr_cstr != nullptr)) {
			return string_t(v6string);
		}
		return string_t();
	}

	//https://en.cppreference.com/w/c/language/operator_arithmetic

	netp::ipv4_t v4_mask_by_cidr(const netp::ipv4_t* const v4, netp::u8_t cidr) {
		if (cidr == 0) {//x<<32 on ul is UB
			return netp::ipv4_t{ 0 };
		} else if (cidr <= 32) {
			return { (v4->u32 & (netp::u32_t)(((0xfffffffful) << (32 - cidr)))) };
		} else {
			return (*v4);
		}
	}

	netp::ipv6_t v6_mask_by_cidr(const netp::ipv6_t* const v6_, netp::u8_t cidr) {
		netp::ipv6_t v6;
		if (cidr == 0) {//x<<64 is UB
			v6.u64.A = 0;
			v6.u64.B = 0;
		} else if (cidr <= 64) {
			v6.u64.A = (v6_->u64.A & (0xffffffffffffffffull << (64 - cidr)));
			v6.u64.B = 0;
		} else if (cidr <= 128) {
			v6.u64.A = v6_->u64.A;
			v6.u64.B = (v6_->u64.B & (0xffffffffffffffffull << (128 - cidr)));
		} else {
			v6 = (*v6_);
		}
		return v6;
	}
}