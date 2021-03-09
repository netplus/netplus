#include <netp/os/api_wrapper.hpp>

#if defined(_NETP_GNU_LINUX)  || defined(_NETP_APPLE)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>

namespace netp { namespace os {

	int get_local_dns_server_list(vector_ipv4_t& ips) {
		NETP_TODO("toimpl");
		return 0;
	}

	int get_adapters(vector_adapter_t& adapters, int filter) {

		struct ifaddrs* ifaddr;
		struct ifaddrs* ifa;

		int idx = 0;

		if (getifaddrs(&ifaddr) == -1) {
			int _errno = netp_last_errno();
			NETP_WARN("[api_wrapper_gnulinux]get_adapters, call getifaddrs() failed, error: %d", _errno);
			return _errno;
		}

		for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {

			++idx;
			if (ifa->ifa_addr == nullptr) {
				continue;
			}

			if (filter&adp_f_skip_noup && (ifa->ifa_flags&IFF_UP) == 0) {
				continue;
			}

			if (filter& adp_f_skip_loopback && (ifa->ifa_flags&IFF_LOOPBACK)) {
				continue;
			}

			netp::adapter adapter_;
			adapter_.idx = idx;
			adapter_.status_up = (ifa->ifa_flags&IFF_UP);
			adapter_.iftype = (ifa->ifa_flags&IFF_LOOPBACK);
			adapter_.flag = ifa->ifa_flags;
			adapter_.name = netp::string_t(ifa->ifa_name, netp::strlen(ifa->ifa_name) );

			if (ifa->ifa_addr->sa_family == AF_INET) {
				if (filter& adp_f_skip_af_inet) {
					continue;
				}
				netp::address addr( *((struct sockaddr_in*)(ifa->ifa_addr)) );
				netp::address mask( *((struct sockaddr_in*)(ifa->ifa_netmask)) );
				adapter_.unicast.push_back({addr.ipv4(), mask.ipv4()});
			}
			else if (ifa->ifa_addr->sa_family == AF_INET6) {
				if (filter& adp_f_skip_af_inet6) {
					continue;
				}
				NETP_ASSERT("todo");
			}
			#ifndef _NETP_APPLE
			else if (ifa->ifa_addr->sa_family == AF_PACKET) {
				if (filter& adp_f_skip_af_packet) {
					continue;
				}
				NETP_ASSERT("todo");
			}
			#endif
			else {
				NETP_WARN("[api_wrapper_gnulinux]unknown sa_family: %u", ifa->ifa_addr->sa_family);
				continue;
			}

			adapters.push_back(adapter_);
		}

		freeifaddrs(ifaddr);
		return netp::OK;
	}

	int get_local_computer_name(netp::string_t& name) {
		NETP_TODO("toimpl");
		return netp::OK;
	}
}}
#endif
