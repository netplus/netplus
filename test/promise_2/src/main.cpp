#include <netp.hpp>

NRP<netp::promise<std::tuple<int, NRP<netp::packet>>>> set_p_on_L(NRP<netp::event_loop> L2) {
	NRP<netp::promise<std::tuple<int, NRP<netp::packet>>>> p = netp::make_ref<netp::promise<std::tuple<int, NRP<netp::packet>>>>();
		
	NETP_ASSERT(!L2->in_event_loop());
	L2->execute([p]() {
		int i = netp::random(10000);
		if (i < 5000) {
			p->set( std::make_tuple(-1, nullptr) );
		} else {
			NRP<netp::packet> pp = netp::make_ref<netp::packet>();
			NETP_ASSERT(pp != nullptr);
			p->set(std::make_tuple(0, std::move(pp)));
		}
	});
	return p;
}

//#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

int main(int argc, char** argv) {

	netp::app::instance()->init(argc,argv);
	netp::app::instance()->start_loop();

	NRP<netp::event_loop> L1 = netp::app::instance()->def_loop_group()->next();
	NRP<netp::event_loop> L2 = netp::app::instance()->def_loop_group()->next();
	std::atomic<bool> round_go = true;
	while (true) {
		while (!round_go.load(std::memory_order_acquire)) {
			netp::this_thread::yield();
		}
		round_go.store(false, std::memory_order_release);

		NRP<netp::promise<std::tuple<int, NRP<netp::packet> >>> p = set_p_on_L(L2);
		p->if_done([L1, L2, &round_go]( const std::tuple<int, NRP<netp::packet>> const& tup ) {
			//main thread check
			int rt = std::get<0>(tup);
			if (rt == 0) {
				NETP_ASSERT( std::get<1>(tup) != nullptr );
			}

			L1->execute([tup, &round_go]() {
				//L1 thread check
				if (std::get<0>(tup) == 0) {
					NETP_ASSERT(std::get<1>(tup) != nullptr);
				}
				round_go.store( true,std::memory_order_release );
			});
		});

	}

	return 0;
}