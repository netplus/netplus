#include <netp.hpp>

NRP<netp::promise<int>> do_async() {
	NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
	NRP<netp::timer> tm = netp::make_ref<netp::timer>(std::chrono::seconds(2), [p]() {
		p->set(8);
	});
	netp::io_event_loop_group::instance()->launch(tm);
	return p;
}

int main(int argc, char** argv) {
	netp::app _app;
	NRP<netp::promise<int>> intp = do_async();
	intp->if_done([](int i) {
		NETP_INFO("do_async return : %d", i);
	});
	_app.run();
	return 0;
}