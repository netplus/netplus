#include <netp.hpp>
#include <iostream>
void th_run() {
	int i = 0;
	while (1) {
		std::cout << i++ << std::endl;	
		netp::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

int main(int argc, char** argv) {
	netp::app _app;
	NRP<netp::thread> th1 = netp::make_ref<netp::thread>();
	th1->start(&th_run);

	NRP<netp::timer> tm = netp::make_ref<netp::timer>(std::chrono::seconds(3), [th1](NRP<netp::timer> const& tm) {
		th1->interrupt();
	});
	netp::io_event_loop_group::instance()->launch(tm);
	th1->join();
	return 0;
}