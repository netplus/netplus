#include <netp.hpp>
#include <iostream>

struct non_pod {
	int i;
	non_pod(int i_) :
		i(i_)
	{}
	virtual ~non_pod() 
	{
		i = 3;
	}
};

NSP<std::vector<NSP<non_pod>>> g__xx ;


void th_run() {
	std::atomic_thread_fence(std::memory_order_acquire);
	NSP<std::vector<NSP<non_pod>>> __g_copy = g__xx;
	g__xx=(nullptr);
	std::atomic_thread_fence(std::memory_order_release);
	int i = 0;
	NSP<non_pod> nnpod = netp::make_shared<non_pod>(0);
	__g_copy->push_back(nnpod);

	nnpod = nullptr;
	while (1) {
		std::cout << i++ << std::endl;	
		netp::this_thread::sleep_for(std::chrono::seconds(1));
	}

	__g_copy = nullptr;
}

int main(int argc, char** argv) {
	netp::app::instance()->init(argc,argv);
	netp::app::instance()->start_loop();
	g__xx = netp::make_shared<std::vector<NSP<non_pod>>>();
	std::atomic_thread_fence(std::memory_order_release);
	NRP<netp::thread> th1 = netp::make_ref<netp::thread>();
	th1->start(&th_run);

	NRP<netp::timer> tm = netp::make_ref<netp::timer>(std::chrono::seconds(5), [th1](NRP<netp::timer> const& tm) {
		th1->interrupt();
	});
	netp::app::instance()->def_loop_group()->launch(tm);
//	th1->join();
	return 0;
}