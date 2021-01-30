#include <algorithm>
#include <netp/address.hpp>
#include <netp/adapter.hpp>
#include <netp/string.hpp>

namespace netp {
	string_t adapter_to_string(adapter const& adapter_) {
		string_t info = "\nadapter info--\n";
		info += "name: " + adapter_.name + "\n";
		info += "description: " + adapter_.description + "\n";
		info += "friendly name: " + adapter_.friendlyname + "\n";
		info += "mac: " + m6tostring(adapter_.mac) + "\n";
		info += string_t("is up: ") + string_t((adapter_.status_up ? "yes" : "no")) + "\n";

		info += "unicast: \n";
		std::for_each(adapter_.unicast.begin(), adapter_.unicast.end(), [&info](netp::interface_unicast const& unicast_) {
			info += "\tipv4 address: " + netp::ipv4todotip(unicast_.ip) + "\n";
			info += "\tsubnet mask: " + netp::ipv4todotip(unicast_.mask) + "\n";
			});

		info += string_t("anycast: \n");
		std::for_each(adapter_.anycast.begin(), adapter_.anycast.end(), [&info](netp::ipv4_t const& ip_) {
			info += "\t" + netp::ipv4todotip(ip_) + "\n";
			});

		info += string_t("multicast: \n");
		std::for_each(adapter_.multicast.begin(), adapter_.multicast.end(), [&info](netp::ipv4_t const& ip_) {
			info += "\t" + netp::ipv4todotip(ip_) + "\n";
			});

		info += string_t("gateway: \n");
		std::for_each(adapter_.gateway.begin(), adapter_.gateway.end(), [&info](netp::ipv4_t const& ip_) {
			info += "\t" + netp::ipv4todotip(ip_) + "\n";
			});
		info += "--\n";

		info += string_t("dns server: \n");
		std::for_each(adapter_.dns.begin(), adapter_.dns.end(), [&info](netp::ipv4_t const& ip_) {
			info += "\t" + netp::ipv4todotip(ip_) + "\n";
			});
		info += "--\n";

		return info;
	}
}