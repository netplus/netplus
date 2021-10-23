//包含库头文件
#include <netp.hpp>

//我们的非阻塞函数，调用后马上返回一个NRP<netp::promise<int>>，此promise可以取到一个int值
//这里，这了简单化问题，我们以int替代，实际上，它的类型可以为任意类型
NRP<netp::promise<int>> do_watch_a_future_state() {
	NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
	NRP<netp::timer> tm = netp::make_ref<netp::timer>(std::chrono::seconds(2), [p]() {
		p->set(8);
	});
	netp::app::instance()->def_loop_group()->launch(tm);
	return p;
}
int main(int argc, char** argv) {
	netp::app::instance()->init(argc, argv);
	netp::app::instance()->start_loop();
	//非阻塞调用开始
	NRP<netp::promise<int>> intp = do_watch_a_future_state();

	//这一行开始，我们为此promise设置一个lambda, 当定时器触发的时候，此lambda被执行。
	//注意：if_done也是非阻塞的，设置完成后，它马上返回，使得我们的线程可以去执行其它的任务。 
	//当然，通过，intp->get()也能像std::future那个取得一个值，但是，这就阻塞了。
	intp->if_done([](int i) {
		NETP_INFO("do_async return : %d", i);
	});

	//等待退出信号
	//ctrl+c, kill -15
	netp::app::instance()->start_loop();
	return 0;
}