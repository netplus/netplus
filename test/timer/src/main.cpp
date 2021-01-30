
//#include <vld.h>

#include <netp.hpp>
#include <chrono>
#include <vector>

std::chrono::nanoseconds delay = std::chrono::nanoseconds(1000 * 10); //10 ns

#define ENABLE_TRACE_TICK
#ifdef ENABLE_TRACE_TICK
	#define TRACE_TICK NETP_INFO
#else
	#define TRACE_TICK(...)
#endif

struct cookie : netp::ref_base
{
	NRP<netp::timer> _timer;
	int i;
	int j;
	cookie(int i_) :i(i_),j(0) {}
};

/*
void spawn_repeat_timer(int);
void repeat_timer_tick(NRP<netp::timer> const& t, NRP<netp::ref_base> const& cookie_) {
	NRP<cookie> c = netp::static_pointer_cast<cookie>(cookie_);
	TRACE_TICK("i: %d, j: %d", c->i , c->j );

	if ( ++c->j>2) {
		netp::global_timer_manager::instance()->stop(t);
	}

	//int i = std::rand();
	//if (std::rand() < 500 ) {
	//	spawn_repeat_timer(i);
	//}
}

void spawn_repeat_timer(int i) {
	NRP<cookie> c = netp::make_ref<cookie>(i);
	NRP<netp::timer> t = netp::make_ref<netp::timer>(delay, true, c, std::bind(&repeat_timer_tick,std::placeholders::_1, std::placeholders::_2));
	netp::global_timer_manager::instance()->start(t);
}
*/
void spawn_user_circle_timer(int i);
void user_circle_tick(NRP<netp::timer> const& t) {
	NRP<cookie> c = t->get_ctx<cookie>();

	TRACE_TICK("i: %d, j: %d", c->i, c->j);
	//if (c->i == 1000) {
	//	netp::signal::signal_manager::instance()->raise_signal(SIGINT);
	//}

	//if (std::rand() & 0x01) {
		spawn_user_circle_timer(++c->i);
	//}
}

void spawn_user_circle_timer(int i) {
	NRP<cookie> c = netp::make_ref<cookie>(i);
	NRP<netp::timer> t = netp::make_ref<netp::timer>(std::chrono::seconds(1), std::bind(&user_circle_tick, std::placeholders::_1));
	t->set_ctx(c);
	netp::timer_manager::instance()->launch(t);
}


void spawn_timer(int);
void timer_tick(NRP<netp::timer> const& t) {
	NRP<cookie> c = t->get_ctx<cookie>();
	TRACE_TICK("i: %d, j: %d", c->i, c->j);
}
void spawn_timer(int i) {
	NRP<cookie> c = netp::make_ref<cookie>(i);
//	NRP<netp::timer> t = netp::make_ref<netp::timer>(delay, c, std::bind(&timer_tick,std::placeholders::_1, std::placeholders::_2));
	NRP<netp::timer> t = netp::make_ref<netp::timer>(std::chrono::seconds(1),&timer_tick);
	t->set_ctx(c);
	netp::timer_manager::instance()->launch(t);
}
struct foo
{
	int i;
	foo(int i_):i(i_)
	{
		printf("__construct foo\n");
	}

	foo(const foo& o) : i(o.i)
	{
		printf("initialize foo\n");
	}

	foo(foo&& o):i(o.i)
	{
		printf("right v ref foo\n");
	}

	~foo()
	{
		printf("~foo: %d\n", i);
	}
};

class foo_timer:
	public netp::ref_base
{
	int i;
public:
	foo_timer(int i_):i(i_)
	{}

	void bar(NRP<netp::timer> const& t) {
		TRACE_TICK("%d", i);
	}
};

void spawn_object_timer(int i ) {
	NRP<cookie> c = netp::make_ref<cookie>(i);
	NRP<foo_timer> f = netp::make_ref<foo_timer>(i);
	NRP<netp::timer> t = netp::make_ref<netp::timer>(delay, &foo_timer::bar, f);
	t->set_ctx(c);
	netp::timer_manager::instance()->launch(t);
}

void spawn_lambda_timer(int i) {
	NRP<cookie> c = netp::make_ref<cookie>(i);
	auto f = [c](NRP<netp::timer> const& t) -> void {
		NRP<cookie> c_ = t->get_ctx<cookie>();
		NETP_ASSERT(c->i == c_->i);
		TRACE_TICK("%d", c->i);
	};

	NRP<netp::timer> t = netp::make_ref<netp::timer>(delay, f);
	t->set_ctx(c);
	netp::timer_manager::instance()->launch(t);
}

void th_spawn_timer() {

	while (1) {
		int i = std::rand();
		spawn_user_circle_timer(i);
		return;
		/*
		if (i % 6 == 0) {
			spawn_timer(i);
		}
		else if (i % 5 == 0) {
			spawn_user_circle_timer(i);
		}
		//else if(i%4 == 0){
		//	spawn_repeat_timer(i);
		//}
		else if (i % 3 == 0) {
			spawn_lambda_timer(i);
		}
		else {
			spawn_object_timer(i);
		}
		*/
//		std::this_thread::yield();
		netp::this_thread::nsleep(1);
	}
}

int main(int argc, char** argv) {

	std::srand(0);
	netp::app _app;

	const int th_count = 4;
	NRP<netp::thread> th[th_count];
	for (int i = 0; i < th_count; ++i) {
		th[i] = netp::make_ref<netp::thread>();
		th[i]->start(&th_spawn_timer);
	}

	//spawn_circle_tick(1);
	_app.run();

	for (int i = 0; i < th_count; ++i) {
		th[i]->interrupt();
		th[i]->join();
	}

	return 0;
}