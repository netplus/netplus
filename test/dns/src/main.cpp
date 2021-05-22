
#include <netp.hpp>

int main(int argc, char** argv) {

	netp::app _app;

	NRP<netp::dns_query_promise> dqp = netp::dns_resolver::instance()->resolve("www.163.com");

	dqp->if_done([](std::tuple<int, std::vector<netp::ipv4_t, netp::allocator<netp::ipv4_t>>> const& query_result) {
		int code = std::get<0>(query_result);
		if (code == netp::OK) {
			std::vector<netp::ipv4_t, netp::allocator<netp::ipv4_t>> const& ipvec = std::get<1>(query_result);
			for (auto ipv4_ : ipvec) {
				netp::string_t ipstr = netp::ipv4todotip(ipv4_);
				NETP_INFO("%s", ipstr.c_str());
			}
		}
		
	});

	_app.run();
	return 0;
}