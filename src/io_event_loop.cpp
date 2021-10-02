#include <netp/core.hpp>
#include <netp/app.hpp>
#include <netp/timer.hpp>
#include <netp/thread.hpp>
#include <netp/io_event_loop.hpp>

#if defined(NETP_HAS_POLLER_EPOLL)
#include <netp/poller_epoll.hpp>
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_EPOLL
#elif defined(NETP_HAS_POLLER_SELECT)
#include <netp/poller_select.hpp>
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_SELECT
#elif defined(NETP_HAS_POLLER_KQUEUE)
#include <netp/poller_kqueue.hpp>
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_KQUEUE
#elif defined(NETP_HAS_POLLER_IOCP)
#include <netp/poller_iocp.hpp>
#include <netp/poller_select.hpp>
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_IOCP
#else
#error "unknown poller type"
#endif

namespace netp {

	inline static NRP<io_event_loop> default_event_loop_maker(io_poller_type t, event_loop_cfg const& cfg) {
		NRP<poller_abstract> poller;
		switch (t) {
#if defined(NETP_HAS_POLLER_EPOLL)
		case T_EPOLL:
		{
			poller = netp::make_ref<poller_epoll>();
			NETP_ALLOC_CHECK(poller, sizeof(poller_epoll));
		}
		break;
#elif defined(NETP_HAS_POLLER_IOCP)
		case T_IOCP:
		{
			poller = netp::make_ref<poller_iocp>();
			NETP_ALLOC_CHECK(poller, sizeof(poller_iocp));
		}
		break;
		case T_SELECT:
		{
			poller = netp::make_ref<poller_select>();
			NETP_ALLOC_CHECK(poller, sizeof(poller_select));
		}
		break;
#elif defined(NETP_HAS_POLLER_KQUEUE)
		case T_KQUEUE:
		{
			poller = netp::make_ref<poller_kqueue>();
			NETP_ALLOC_CHECK(poller, sizeof(poller_kqueue));
		}
		break;
#else
		case T_SELECT:
		{
			poller = netp::make_ref<poller_select>();
			NETP_ALLOC_CHECK(poller, sizeof(poller_select));
		}
		break;
#endif
		default:
		{
			NETP_THROW("invalid poll type");
		}
		}

		NETP_ASSERT(poller != nullptr);
		return netp::make_ref<io_event_loop>(t,poller, cfg);
	}

	void io_event_loop::init() {
		m_channel_rcv_buf = netp::make_ref<netp::packet>(m_cfg.ch_buf_size);
		m_tid = std::this_thread::get_id();
		m_tb = netp::make_ref<timer_broker>();

		m_poller->init();
	}

	void io_event_loop::deinit() {
		NETP_VERBOSE("[io_event_loop]deinit begin");
		NETP_ASSERT(in_event_loop());
		NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_EXIT), "event loop deinit state check failed");

		{
			lock_guard<spin_mutex> lg(m_tq_mutex);
			NETP_ASSERT(m_tq_standby.empty());
		}

		NETP_ASSERT(m_tq.empty());
		NETP_ASSERT(m_tb->size() == 0);
		m_tb = nullptr;

		m_poller->deinit();
		NETP_VERBOSE("[io_event_loop]deinit done");
	}

	//@NOTE: promise to execute all task already in tq or tq_standby
	void io_event_loop::__run() {
		//NETP_ASSERT(!"CHECK EXCEPTION STACK");
		init();
		//record a snapshot, used by update state
		m_io_ctx_count_before_running = m_io_ctx_count;
		u8_t _SL = u8_t(loop_state::S_LAUNCHING);
		const bool rt = m_state.compare_exchange_strong(_SL, u8_t(loop_state::S_RUNNING), std::memory_order_acq_rel, std::memory_order_acquire);
		NETP_ASSERT(rt == true);
		try {
			//this load also act as a memory synchronization fence to sure all release operation happen before this line
			//if we make_ref a atomic_ref object, then we call L->schedule([o=atomic_ref_instance](){});, the assign of a atomic_ref_instance would trigger memory_order_acq_rel, this operation guard all object member initialization and member valud update before the assign
			//all member value of that object must be synchronized after this line, cuz we have netp::atomic_incre inside ref object
			while (NETP_UNLIKELY(u8_t(loop_state::S_EXIT) != m_state.load(std::memory_order_acquire))) {
				{
					//again the spin_mutex acts as a memory synchronization fence
					lock_guard<spin_mutex> lg(m_tq_mutex);
					if (!m_tq_standby.empty()) {
						std::swap(m_tq, m_tq_standby);
					}
				}
				std::size_t ss = m_tq.size();
				if (ss > 0) {
					std::size_t i = 0;
					while (i < ss) {
						m_tq[i++]();
					}
					if (ss > 4096) {
						io_task_q_t().swap(m_tq);
					}
					else {
						m_tq.clear();
					}
				}
				//@_calc_wait_dur_in_nano must happen before poll..
				m_poller->poll(_calc_wait_dur_in_nano(), m_waiting);
			}
		}
		catch (...) {
			//@NOTE: we should terminate our process in this case
			//m_state = S_EXIT;
			//deinit();
			NETP_WARN("[io_event_loop]__run reach exception------------------------------------------");
			throw;
		}
		{
			NETP_VERBOSE("[io_event_loop]exiting run");
			// EDGE check
			// scenario 1:
			// 1) do schedule, 2) set L -> null
			std::size_t i = 0;
			std::size_t vecs = m_tq_standby.size();
			while (i < vecs) {
				m_tq_standby[i++]();
			}
			m_tq_standby.clear();
			m_tb->expire_all();
		}

		deinit();
	}

	void io_event_loop::__do_notify_terminating() {
		NETP_ASSERT( in_event_loop() );
		io_do(io_action::NOTIFY_TERMINATING, 0);
		if (m_io_ctx_count == m_io_ctx_count_before_running) {
			__do_enter_terminated();
		}
	//	NETP_VERBOSE("[io_event_loop]__do_notify_terminating done");
	}

	void io_event_loop::__do_enter_terminated() {
		//no competitor here, store directly
		NETP_ASSERT(in_event_loop());
		m_state.store(u8_t(loop_state::S_TERMINATED), std::memory_order_release);
		NETP_ASSERT(m_tb != nullptr);
		m_tb->expire_all();
	}
	
	//terminating phase 1
	void io_event_loop::__notify_terminating() {
		u8_t running = u8_t(loop_state::S_RUNNING);
		if (m_state.compare_exchange_strong(running, u8_t(loop_state::S_TERMINATING), std::memory_order_acq_rel, std::memory_order_acquire)) {
			schedule([L = NRP<io_event_loop>(this)]() {
				L->__do_notify_terminating();
			});
		}
	}

		int io_event_loop::__launch() {
			u8_t _si = u8_t(loop_state::S_IDLE);
			bool upstate = m_state.compare_exchange_strong(_si, u8_t(loop_state::S_LAUNCHING), std::memory_order_acq_rel, std::memory_order_acquire);
			NETP_ASSERT(upstate == true);

			m_th = netp::make_ref<netp::thread>();
			int rt = m_th->start(&io_event_loop::__run, NRP<io_event_loop>(this));
			NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
			int k = 0;
			while (m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_LAUNCHING)) {
				netp::this_thread::yield(++k);
			}
			//NETP_VERBOSE("[io_event_loop][%u]__launch done", m_type);
			
			return netp::OK;
		}

		void io_event_loop::__terminate() {
			NETP_VERBOSE("[io_event_loop][%u]__terminate begin", m_type );
			while (m_state.load(std::memory_order_acquire) != u8_t(loop_state::S_TERMINATED)) {
				netp::this_thread::sleep(1);
			}
			u8_t terminated = u8_t(loop_state::S_TERMINATED);
			if (m_state.compare_exchange_strong(terminated, u8_t(loop_state::S_EXIT), std::memory_order_acq_rel, std::memory_order_acquire)) {
				NETP_VERBOSE("[io_event_loop][%u]enter S_EXIT, last interrupt if needed", m_type);
				m_poller->interrupt_wait();
				//@NOTE:
				//1, we don't interrupt these kinds of thread, cuz we want all the tasks be finished one by one
				//m_th->interrupt();
				m_th->join();
				m_th = nullptr;
			}
			NETP_INFO("[io_event_loop][%u]__terminate end", m_type);
		}

		io_event_loop::io_event_loop(io_poller_type t, NRP<poller_abstract> const& poller, event_loop_cfg const& cfg) :
			m_waiting(false),
			m_state(u8_t(loop_state::S_IDLE)),
			m_type(t),
			m_io_ctx_count(0),
			m_io_ctx_count_before_running(0),
			m_poller(poller),
			m_internal_ref_count(0),
			m_cfg(cfg)
		{}

		io_event_loop::~io_event_loop() {
			NETP_ASSERT(m_tb == nullptr);
			NETP_ASSERT(m_th == nullptr);
		}

		io_event_loop_group::io_event_loop_group():
			m_bye_ref_count(0),
			m_bye_state(bye_event_loop_state::S_IDLE)
		{
		}
		io_event_loop_group::~io_event_loop_group()
		{
		}

		void io_event_loop_group::notify_terminating_loop(io_poller_type t) {
			shared_lock_guard<shared_mutex> slg(m_loop_mtx[t]);
			for(::size_t i=0;i<m_loop[t].size();++i) {
				m_loop[t][i]->__notify_terminating();
			}
		}

		void io_event_loop_group::launch_loop(io_poller_type t, u32_t count, event_loop_cfg const& cfg, fn_event_loop_maker_t const& fn_maker ) {
			NETP_VERBOSE("[io_event_loop_group]alloc poller: %u, count: %u, ch_buf_size: %u", t, count, cfg.ch_buf_size );
			lock_guard<shared_mutex> lg(m_loop_mtx[t]);
			m_curr_loop_idx[t] = 0;
			while (count-- > 0) {
				NRP<io_event_loop> o = fn_maker == nullptr ?
					default_event_loop_maker(t,cfg) : 
					fn_maker(t,cfg);

				int rt = o->__launch();
				NETP_ASSERT(rt == netp::OK);
				o->store_internal_ref_count(o.ref_count());
				m_loop[t].push_back(std::move(o));
			}
		}

		void io_event_loop_group::wait_loop(io_poller_type t) {
		__dealloc_begin:
			std::vector<NRP<io_event_loop>> to_deattach;
			{
				lock_guard<shared_mutex> lg(m_loop_mtx[t]);
				if (m_loop[t].size() == 0) {
					io_event_loop_vector().swap(m_loop[t]);
					//NETP_INFO("[io_event_loop][%u]__dealloc_poller end, all event loop dattached", t);
					return;//exit
				}

				bye_event_loop_state idle = bye_event_loop_state::S_IDLE;
				if (m_bye_state.compare_exchange_strong(idle, bye_event_loop_state::S_PREPARING, std::memory_order_acq_rel, std::memory_order_acquire)) {
					NETP_ASSERT(m_bye_event_loop == nullptr, "m_bye_event_loop check failed");
					m_bye_event_loop = default_event_loop_maker(NETP_DEFAULT_POLLER_TYPE, { 0 });
					int rt = m_bye_event_loop->__launch();
					NETP_ASSERT(rt == netp::OK);
					m_bye_ref_count = m_bye_event_loop.ref_count();

					bye_event_loop_state preparing = bye_event_loop_state::S_PREPARING;
					bool set_to_running = m_bye_state.compare_exchange_strong(preparing, bye_event_loop_state::S_RUNNING, std::memory_order_acq_rel, std::memory_order_acquire);
					NETP_ASSERT(set_to_running == true);
				}

				io_event_loop_vector::iterator&& it = m_loop[t].begin();
				while( it != m_loop[t].end() ) {
					//ref_count == internal_ref_count means no other ref for this LOOP, we must deattach it from our pool
					if((*it).ref_count() == (*it)->internal_ref_count() ) {
						NETP_VERBOSE("[io_event_loop][%u]__dealloc_poller, dattached one event loop", t);

						to_deattach.push_back(*it);
						m_loop[t].erase(it);
						break;
					} else {
						++it;
					}
				}
			}
			while (to_deattach.size()) {
				to_deattach.back()->__terminate();
				to_deattach.pop_back();
			}
			netp::this_thread::no_interrupt_sleep(1);
			goto __dealloc_begin;
		}

		void io_event_loop_group::_init(u32_t count[io_poller_type::T_POLLER_MAX], event_loop_cfg cfgs[io_poller_type::T_POLLER_MAX]) {
			for (u32_t i = 0; i < T_POLLER_MAX; ++i) {
				if (count[i] > 0) {
					launch_loop(io_poller_type(i),count[i], cfgs[i]);
				}
			}
		}

		void io_event_loop_group::_notify_terminating_all() {
			//phase 1, terminating
			for (int t = T_POLLER_MAX - 1; t >= 0; --t) {
				if (m_loop[t].size()) {
					NETP_INFO("[io_event_loop]__notify_terminating, type: %d", io_poller_type(t));
					notify_terminating_loop(io_poller_type(t));
				}
			}
		}

		/*
			@dealloc logic
				I, phase 1, detach io_event_loop from io_event_loop_vector one by one by the following procedure
					1) call io_event_loop->stop() to set its state into S_EXIT;
						when io_event_loop get into S_EXIT, we do as followwing:
						2), phase 1, call all fd event, cancel all new watch evt, ignore all unwatch, timer
						3), execute all tasks in m_tq_standby && m_tq, if no more tasks exit, we delete m_tq_standby, and forbidding any new scheduled task
					4), terminate this event_loop
				II, when io_event_loop_vector.size() == 0, next() would always return m_bye_io_event_loop [this kind of io_event_loop can only do executor and schedule]
				III, when all kinds of io_event_loop_vector.size() ==0, we enter into phase 2

				IV, in phase 2, we stop m_bye_io_event_loop
		*/
		void io_event_loop_group::_wait_all() {

			//phase 2, deattach one by one
			for (int t = T_POLLER_MAX - 1; t >= 0; --t) {
				if (m_loop[t].size()) {
					NETP_INFO("[io_event_loop]__dealloc_poller, type: %d", io_poller_type(t));
					wait_loop(io_poller_type(t));
					NETP_INFO("[io_event_loop]__dealloc_poller, type: %d done", io_poller_type(t));
				}
			}

			bye_event_loop_state running = bye_event_loop_state::S_RUNNING;
			if (m_bye_state.compare_exchange_strong(running, bye_event_loop_state::S_EXIT, std::memory_order_acq_rel, std::memory_order_acquire)) {
				NETP_INFO("[io_event_loop]__dealloc_poller bye, begin");
				NETP_ASSERT(m_bye_event_loop != nullptr);
				m_bye_event_loop->__notify_terminating();
				while ( m_bye_event_loop.ref_count() != m_bye_ref_count) {
					//NETP_INFO("l.ref_count: %ld, ref_count: %ld", m_bye_event_loop.ref_count(), m_bye_ref_count.load(std::memory_order_acquire) );
					netp::this_thread::no_interrupt_sleep(1);
				}
				m_bye_event_loop->__terminate();
				m_bye_event_loop = nullptr;
				m_bye_ref_count = 0;
				m_bye_state.store(bye_event_loop_state::S_IDLE);
				NETP_INFO("[io_event_loop]__dealloc_poller bye done");
			}
		}

		io_poller_type io_event_loop_group::query_available_custom_poller_type() {
			for (int i = T_POLLER_CUSTOM_1; i < T_POLLER_MAX; ++i) {
				if (m_loop[i].size() == 0) {
					return io_poller_type(i);
				}
			}
			return T_NONE;
		}

		netp::size_t io_event_loop_group::size(io_poller_type t) {
			shared_lock_guard<shared_mutex> lg(m_loop_mtx[t]);
			return netp::size_t(m_loop[t].size());
		}

		NRP<io_event_loop> io_event_loop_group::next(io_poller_type t, std::set<NRP<io_event_loop>> const& exclude_this_list_if_have_more) {
			{
				shared_lock_guard<shared_mutex> lg(m_loop_mtx[t]);
				const io_event_loop_vector& pollers = m_loop[t];
				if (pollers.size() > 0) {
					const std::size_t psize = pollers.size();
					if (psize <= exclude_this_list_if_have_more.size()) {
						//skip one time
						//u32_t idx = netp::atomic_incre(&m_curr_poller_idx[t]) % psize;
						//if (exclude_this_list_if_have_more.find(pollers[idx]) == exclude_this_list_if_have_more.end()) {
						//	return pollers[idx];
						//}
						return pollers[m_curr_loop_idx[t].fetch_add(1, std::memory_order_relaxed) % psize];
					}
					else {
						u32_t idx = m_curr_loop_idx[t].fetch_add(1, std::memory_order_relaxed) % psize;
						while (exclude_this_list_if_have_more.find(pollers[idx]) != exclude_this_list_if_have_more.end()) {
							idx = m_curr_loop_idx[t].fetch_add(1, std::memory_order_relaxed) % psize;
						}
						return pollers[idx];
					}
				}
			}

			if (m_bye_state.load(std::memory_order_relaxed) == bye_event_loop_state::S_RUNNING) {
				return m_bye_event_loop;
			}
			NETP_THROW("io_event_loop_group deinit logic issue");
		}

		NRP<io_event_loop> io_event_loop_group::next(io_poller_type t) {
			{
				shared_lock_guard<shared_mutex> lg(m_loop_mtx[t]);
				const io_event_loop_vector& pollers = m_loop[t];
				if (pollers.size() != 0) {
					return pollers[m_curr_loop_idx[t].fetch_add(1, std::memory_order_relaxed) % pollers.size()];
				}
			}
			if(m_bye_state.load(std::memory_order_relaxed) == bye_event_loop_state::S_RUNNING) {
				return m_bye_event_loop;
			}
			NETP_THROW("io_event_loop_group deinit logic issue");
		}

		
		NRP<io_event_loop> io_event_loop_group::internal_next(io_poller_type t) {
			shared_lock_guard<shared_mutex> lg(m_loop_mtx[t]);
			const io_event_loop_vector& pollers = m_loop[t];
			NETP_ASSERT(m_loop[t].size() != 0);
			int idx = m_curr_loop_idx[t].fetch_add(1, std::memory_order_relaxed) % pollers.size();
			pollers[idx]->inc_internal_ref_count();
			return pollers[idx];
			//NETP_THROW("io_event_loop_group deinit logic issue");
		}
		

		void io_event_loop_group::execute(fn_task_t&& f, io_poller_type poller_t) {
			next(poller_t)->execute(std::forward<fn_task_t>(f));
		}
		void io_event_loop_group::schedule(fn_task_t&& f, io_poller_type poller_t) {
			next(poller_t)->schedule(std::forward<fn_task_t>(f));
		}
		void io_event_loop_group::launch(NRP<netp::timer> const& t, NRP<netp::promise<int>> const& lf, io_poller_type poller_t) {
			next(poller_t)->launch(t,lf);
		}
}
