

#include <netp.hpp>

struct k_cost {
	int r;
	long long cost;
};

int main(int argc, char**argv) {
	netp::app::instance()->init(argc,argv);
	//netp::app::instance()->start_loop();
	netp::u32_t k_begin = netp::random(0,100000);
	netp::u32_t k_count = std::atoi(argv[1]);
	netp::u32_t k_capacity = std::atoi(argv[2]);
	NETP_INFO("[div_cost]k_begin: %u, k_count: %u, k_capacity: %u", k_begin, k_count, k_capacity);

	k_cost c ;//= netp::allocator<k_cost>::make_array(k_count);
	netp::u32_t k_end = (k_begin + k_count);
	netp::u32_t k_div_begin = k_begin;
	netp::benchmark divmk("div");
	while (k_div_begin < k_end) {
		volatile int rr = (k_div_begin++/k_capacity);
	}
	long long divcost = divmk.mark("div done").count();

	netp::u32_t k_and_begin = k_begin;
	netp::benchmark andmk("and");
	while (k_and_begin < k_end) {
		volatile int rr = (k_and_begin++ & k_capacity);
	}
	long long andcost = andmk.mark("and done").count();
	NETP_INFO("[main]divavg: %f, andavg: %f", divcost/(k_count*1.0f), andcost/(k_count * 1.0f) );
	return 0;
}