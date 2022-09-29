#include <netp.hpp>
#include <iostream>

std::condition_variable g_cv;
std::mutex g_mtx;

struct cost_sample {
	//in nanos
	std::atomic<long long> begin;
	std::atomic<long long> end;
};
cost_sample g_tmp;
netp::benchmark g_mk("cost_sample", netp::bf_no_mark_output|netp::bf_no_end_output);
void th_wait() {
	while (1) {
		std::unique_lock<std::mutex> ulk(g_mtx);
		g_cv.wait(ulk);
		g_tmp.end.store(g_mk.mark("waken").count(), std::memory_order_relaxed);
		NETP_INFO("[condition_cost]cost: %lld ns", (g_tmp.end - g_tmp.begin));
		netp::this_thread::yield();
	}
}
void th_notify() {
	while (1) {
		g_tmp.begin.store(g_mk.mark("notify").count(), std::memory_order_relaxed);
		std::unique_lock<std::mutex> ulk(g_mtx);
		g_cv.notify_one();
		netp::this_thread::yield();
		//netp::this_thread::usleep(netp::random(100,1000));
	}
}

int main(int argc, char** argv) {
	netp::app::instance()->init(argc,argv);
	netp::app::instance()->start_loop();

	NRP<netp::thread> thwait = netp::make_ref<netp::thread>();
	thwait->start(&th_wait);

	NRP<netp::thread> thnotify = netp::make_ref<netp::thread>();
	thnotify->start(&th_notify);

	netp::app::instance()->wait();

	thwait->interrupt();
	thnotify->interrupt();
	thwait->join();
	thnotify->join();

	return 0;
}