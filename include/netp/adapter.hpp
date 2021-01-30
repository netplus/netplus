#ifndef _NETP_ADAPTER_HPP
#define _NETP_ADAPTER_HPP

#include <vector>
#include <netp/core.hpp>
#include <netp/memory.hpp>
#include <netp/address.hpp>
#include <netp/l2/m6.hpp>
#include <netp/logger_broker.hpp>
#include <netp/string.hpp>

namespace netp {

	struct interface_unicast {
		netp::ipv4_t ip;
		netp::ipv4_t mask;
	};
	typedef std::vector<interface_unicast, netp::allocator<interface_unicast>> vector_interface_unicast_t;

	struct adapter {
		int idx;
		netp::l2::m6 mac;
		
		vector_interface_unicast_t unicast;
		vector_ipv4_t anycast;
		vector_ipv4_t multicast;
		vector_ipv4_t gateway;
		vector_ipv4_t dns;

		string_t name;
		string_t description;
		string_t friendlyname;
		string_t id;

		bool status_up;
		long flag;
		long mtu;
		long iftype;
	};

	extern string_t adapter_to_string(adapter const& adapter_);

	typedef std::vector<adapter, netp::allocator<adapter>> vector_adapter_t;
}
#endif