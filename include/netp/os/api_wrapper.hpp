#ifndef _NETP_OS_API_WRAPPER_HPP
#define _NETP_OS_API_WRAPPER_HPP

#include <vector>
#include <netp/core.hpp>
#include <netp/string.hpp>
#include <netp/address.hpp>
#include <netp/adapter.hpp>

namespace netp { namespace os {
	enum adapter_filter_flag {
		adp_f_skip_noup =1,
		adp_f_skip_loopback = 1<<1,
		adp_f_skip_af_inet = 1<<2,
		adp_f_skip_af_inet6 = 1<<3,
		adp_f_skip_af_packet = 1<<4
	};

	extern u32_t get_process_id();
	extern int get_local_computer_name(std::string& name);
	extern int get_local_dns_server_list(vector_ipv4_t& ips);
	extern int get_adapters(vector_adapter_t& adapters, int filter);
}}
#endif