#include <netp/core.hpp>
#include <netp/app.hpp>
#include <netp/timer.hpp>
#include <netp/thread.hpp>
#include <netp/event_loop.hpp>
#include <netp/dns_resolver.hpp>

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

	NRP<event_loop> default_event_loop_maker(event_loop_cfg const& cfg) {
		NRP<poller_abstract> poller;
		switch (cfg.type) {
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
		return netp::make_ref<event_loop>(cfg, poller);
	}

	void event_loop::init() {
		m_channel_rcv_buf = netp::make_ref<netp::packet>(m_cfg.channel_read_buf_size,0);
		m_tid = std::this_thread::get_id();
		m_tb = netp::make_ref<timer_broker>();
		m_poller->init();

		if (m_cfg.flag & f_enable_dns_resolver) {
			if (m_cfg.type == NETP_DEFAULT_POLLER_TYPE) {
				m_dns_resolver = netp::make_ref<dns_resolver>(NRP<event_loop>(this));
				inc_internal_ref_count();
			} else {
				m_dns_resolver = netp::make_ref<dns_resolver>(netp::app::instance()->def_loop_group()->next());
			}

			if (m_dns_hosts.size()) {
				m_dns_resolver->add_name_server(m_dns_hosts);
			}
			NRP<netp::promise<int>> dnsp = m_dns_resolver->start();
			if (dnsp->get() != netp::OK) {
				char _fail_message[256] = { 0 };
				snprintf(_fail_message, 255, "dns resolver start failed: %d", dnsp->get());
				NETP_ERR("[app]start dnsresolver failed: %d, exit", dnsp->get());
				NETP_THROW("dns_resolver start failed");
			}
		}
	}

	void event_loop::deinit() {
		NETP_VERBOSE("[event_loop][%p]deinit begin", this );
		NETP_ASSERT(in_event_loop());
		NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_EXIT), "event loop deinit state check failed");

		if (m_cfg.flag & f_enable_dns_resolver) {
			NETP_ASSERT(m_dns_resolver != nullptr);
			m_dns_resolver = nullptr;
		}

		{
			lock_guard<spin_mutex> lg(m_tq_mutex);
			NETP_ASSERT(m_tq_standby.empty());
		}

		NETP_ASSERT(m_tq.empty());
		NETP_ASSERT(m_tb->size() == 0);
		m_tb = nullptr;

		m_poller->deinit();
		NETP_VERBOSE("[event_loop][%p]deinit done", this );
	}

	//@NOTE: promise to execute all task already in tq or tq_standby
	void event_loop::__run() {

		if(m_cfg.flag&f_th_thread_affinity) {
			m_th->set_affinity((m_cfg.thread_affinity) % std::thread::hardware_concurrency());
		}

		if (m_cfg.flag & f_th_priority_time_critical) {
			m_th->set_priority_time_critical();
		} else if (m_cfg.flag & f_th_priority_above_normal) {
			m_th->set_priority_above_normal();
		}

		//NETP_ASSERT(!"CHECK EXCEPTION STACK");
		init();
		//record a snapshot, used by update state
		m_io_ctx_count_before_running = m_io_ctx_count;
		u8_t _SL = u8_t(loop_state::S_LAUNCHING);
		const bool rt = m_state.compare_exchange_strong(_SL, u8_t(loop_state::S_RUNNING), std::memory_order_acq_rel, std::memory_order_acquire);
		NETP_ASSERT(rt == true);
#ifdef NETP_DEBUG_LOOP_TIME
		m_loop_last_tp = netp::now<std::chrono::nanoseconds, netp::steady_clock_t>().time_since_epoch().count();
#endif
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
					if (ss > 1024) {
						io_task_q_t().swap(m_tq);
					}
					else {
						m_tq.clear();
					}
				}
				//@_calc_wait_dur_in_nano must happen before poll..

#ifdef NETP_DEBUG_LOOP_TIME
				long long _now = netp::now<std::chrono::nanoseconds, netp::steady_clock_t>().time_since_epoch().count();
				NETP_INFO("[event_loop]loop dt: %llu ns, last_wait: %llu", _now - m_loop_last_tp, m_last_wait);
				m_loop_last_tp = _now;
#endif
				m_poller->poll(_calc_wait_dur_in_nano(), m_waiting);
			}
		}
		catch (...) {
			//@NOTE: we should terminate our process in this case
			//m_state = S_EXIT;
			//deinit();
			NETP_WARN("[event_loop]__run reach exception------------------------------------------");
			throw;
		}
		{
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
		NETP_VERBOSE("[event_loop][%p]exiting run", this );
	}

	void event_loop::__do_notify_terminating() {
		NETP_ASSERT( in_event_loop() );

		//dns resolver stop would result in dns socket be removed from io_ctx
		//we keep m_dns_resolver instance until there is no event_loop reference outside
		//no new fd is accepted after state enter terminating, so it's safe to stop dns first
		if (m_cfg.flag & f_enable_dns_resolver) {
			NETP_ASSERT(m_dns_resolver != nullptr);
			m_dns_resolver->stop();
		}
		io_do(io_action::NOTIFY_TERMINATING, 0);
		if (m_io_ctx_count == m_io_ctx_count_before_running) {
			__do_enter_terminated();
		}
	//	NETP_VERBOSE("[event_loop]__do_notify_terminating done");
	}

	void event_loop::__do_enter_terminated() {
		//no competitor here, store directly
		NETP_ASSERT(in_event_loop());
		NETP_ASSERT(m_io_ctx_count == m_io_ctx_count_before_running);
		u8_t terminating = u8_t(loop_state::S_TERMINATING);
		if (m_state.compare_exchange_strong(terminating, u8_t(loop_state::S_TERMINATED), std::memory_order_acq_rel, std::memory_order_acquire)) {
			NETP_VERBOSE("[event_loop][%p][%u]__do_enter_terminated done", this, m_cfg.type);
			NETP_ASSERT(m_tb != nullptr);
			m_tb->expire_all();
		}
	}
	
	//terminating phase 1
	void event_loop::__notify_terminating() {
		u8_t running = u8_t(loop_state::S_RUNNING);
		if (m_state.compare_exchange_strong(running, u8_t(loop_state::S_TERMINATING), std::memory_order_acq_rel, std::memory_order_acquire)) {
			schedule([L = NRP<event_loop>(this)]() {
				L->__do_notify_terminating();
			});
		}
	}

	int event_loop::__launch() {
		u8_t _si = u8_t(loop_state::S_IDLE);
		bool upstate = m_state.compare_exchange_strong(_si, u8_t(loop_state::S_LAUNCHING), std::memory_order_acq_rel, std::memory_order_acquire);
		NETP_ASSERT(upstate == true);

		m_th = netp::make_ref<netp::thread>();
		int rt = m_th->start(&event_loop::__run, NRP<event_loop>(this));
		NETP_RETURN_V_IF_NOT_MATCH(rt, rt == netp::OK);
		int k = 0;
		while (m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_LAUNCHING)) {
			netp::this_thread::no_interrupt_yield(++k);
		}
		return netp::OK;
	}

	void event_loop::__terminate() {
		NETP_VERBOSE("[event_loop][%p][%u]__terminate begin", this, m_cfg.type );
		while (m_state.load(std::memory_order_acquire) != u8_t(loop_state::S_TERMINATED)) {
			netp::this_thread::no_interrupt_sleep(1);
		}
		u8_t terminated = u8_t(loop_state::S_TERMINATED);
		if (m_state.compare_exchange_strong(terminated, u8_t(loop_state::S_EXIT), std::memory_order_acq_rel, std::memory_order_acquire)) {
			NETP_VERBOSE("[event_loop][%p][%u]enter S_EXIT, last interrupt if needed", this, m_cfg.type);
			//@TODO
			//NOTE: interrupt_wait might failed, the the L->deinit() happens before interrupt_wait, the Loop enter wait ,then interrupted by system && m_state entered into S_EXIT at the time [eintr]
			m_poller->interrupt_wait();
			//@NOTE:
			//1, we don't interrupt these kinds of thread, cuz we want all the tasks be finished one by one
			//m_th->interrupt();
			m_th->join();
			m_th = nullptr;
		}
		NETP_INFO("[event_loop][%p][%u]__terminate end", this, m_cfg.type ) ;
	}

	event_loop::event_loop(event_loop_cfg const& cfg, NRP<poller_abstract> const& poller):
		m_waiting(false),
		m_state(u8_t(loop_state::S_IDLE)),
		m_cfg(cfg),
		m_poller(poller),
		m_io_ctx_count(0),
		m_io_ctx_count_before_running(0), 
		m_internal_ref_count(0),
		m_dns_hosts(cfg.dns_hosts.begin(), cfg.dns_hosts.end())
	{}

	event_loop::~event_loop() {
		NETP_ASSERT(m_tb == nullptr);
		NETP_ASSERT(m_th == nullptr);
		NETP_VERBOSE("[event_loop::~event_loop][%u]", m_cfg.type);
	}

	event_loop_group::event_loop_group( event_loop_cfg const& cfg, fn_event_loop_maker_t const& L_maker):
		m_curr_loop_idx(0),
		m_bye_state(bye_event_loop_state::S_IDLE),
		m_bye_ref_count(0),
		m_cfg(cfg),
		m_fn_loop_maker(L_maker)
	{
	}

	event_loop_group::~event_loop_group()
	{
		//guarentee to always return non-null for next()
		bye_event_loop_state bye_state_exit = bye_event_loop_state::S_EXIT;
		if (m_bye_state.compare_exchange_strong(bye_state_exit, bye_event_loop_state::S_IDLE, std::memory_order_acq_rel, std::memory_order_acquire) ) {
			m_bye_event_loop = nullptr;
			m_bye_ref_count = 0;
		}
		NETP_ASSERT(m_bye_event_loop == nullptr);
	}

	void event_loop_group::notify_terminating() {
		shared_lock_guard<shared_mutex> slg(m_loop_mtx);
		for(::size_t i=0;i<m_loop.size();++i) {
			m_loop[i]->__notify_terminating();
		}
	}

	/*
	@dealloc logic
		I, phase 1, detach event_loop from event_loop_vector one by one by the following procedure
			1) call event_loop->notify_terminating() to give a termination notification to all FDs
			2) loop enter in terminating state, all fd object(socket) would received a termination notification, do force close
				a) no new fd would be accepted on TERMINATING state
				b) fd ctx would be removed after socket be closed
			3) once all fds ctx has been removed from LOOP, LOOP enter TERMINATED state
				a) no new timer would be accepted on TERMINATED state
				b) update state to S_EXIT (by loop_group)
				c) LOOP do deinit
			4), remove LOOP from loop_vector
		II, when event_loop_vector.size() == 0, next() would always return m_bye_event_loop
		III, when event_loop_vector.size() ==0, we enter into phase 2

		IV, in phase 2, we terminate m_bye_event_loop
	*/

	void event_loop_group::_wait_loop() {
	__dealloc_begin:
		std::vector<NRP<event_loop>> to_deattach;
		{
			lock_guard<shared_mutex> lg(m_loop_mtx);
			if (m_loop.size() == 0) {
				event_loop_vector_t().swap(m_loop);
				//NETP_INFO("[event_loop][%u]__dealloc_poller end, all event loop dattached", t);
				return;//exit
			}

			bye_event_loop_state idle = bye_event_loop_state::S_IDLE;
			if (m_bye_state.compare_exchange_strong(idle, bye_event_loop_state::S_PREPARING, std::memory_order_acq_rel, std::memory_order_acquire)) {
				NETP_ASSERT(m_bye_event_loop == nullptr, "m_bye_event_loop check failed");
				NETP_VERBOSE("[event_loop][%u]launch bye begin", m_cfg.type);
				event_loop_cfg __cfg = m_cfg;
				__cfg.flag &= ~(f_th_thread_affinity | f_th_priority_above_normal | f_th_priority_time_critical);
				m_bye_event_loop = m_fn_loop_maker(__cfg);
				int rt = m_bye_event_loop->__launch();
				NETP_ASSERT(rt == netp::OK);
				m_bye_ref_count = m_bye_event_loop.ref_count();

				bye_event_loop_state preparing = bye_event_loop_state::S_PREPARING;
				bool set_to_running = m_bye_state.compare_exchange_strong(preparing, bye_event_loop_state::S_RUNNING, std::memory_order_acq_rel, std::memory_order_acquire);
				NETP_ASSERT(set_to_running == true);
				NETP_ASSERT(m_bye_state.load(std::memory_order_relaxed) == bye_event_loop_state::S_RUNNING);
				NETP_VERBOSE("[event_loop][%u]launch bye end", m_cfg.type);
			}

			event_loop_vector_t::iterator&& it = m_loop.begin();
			while (it != m_loop.end()) {
				//ref_count == internal_ref_count means no other ref for this LOOP, it is safe to deattach it from our pool
				if ((*it).ref_count() == (*it)->internal_ref_count()) {
					NETP_VERBOSE("[event_loop][%u]_wait_loop, dattached one event loop", m_cfg.type);

					to_deattach.push_back(*it);
					m_loop.erase(it);
					break;
				} else {
					++it;
				}
			}
		}
		while (to_deattach.size()) {
			//if L get here, it's probably in wait state (if L is not intrrupted by system[eintr])
			to_deattach.back()->__terminate();
			to_deattach.pop_back();
		}
		netp::this_thread::no_interrupt_sleep(1);
		goto __dealloc_begin;
	}

		void event_loop_group::wait() {

			//phase 2, deattach one by one
			NETP_INFO("[event_loop][%u]event_loop_group::_wait_loop", m_cfg.type);
			_wait_loop();
			NETP_INFO("[event_loop][%u]event_loop_group::_wait_loop done", m_cfg.type);

			bye_event_loop_state running = bye_event_loop_state::S_RUNNING;
			if (m_bye_state.compare_exchange_strong(running, bye_event_loop_state::S_EXIT, std::memory_order_acq_rel, std::memory_order_acquire)) {
				NETP_VERBOSE("[event_loop][%u]wait bye begin", m_cfg.type );
				NETP_ASSERT(m_bye_event_loop != nullptr);
				m_bye_event_loop->__notify_terminating();
				while (m_bye_event_loop.ref_count() != m_bye_ref_count) {
					//NETP_INFO("l.ref_count: %ld, ref_count: %ld", m_bye_event_loop.ref_count(), m_bye_ref_count.load(std::memory_order_acquire) );
					netp::this_thread::no_interrupt_sleep(1);
				}
				m_bye_event_loop->__terminate();
				NETP_VERBOSE("[event_loop][%u]wait bye end", m_cfg.type );
			}
		}

		void event_loop_group::start(u32_t count ) {
			NETP_VERBOSE("[event_loop_group]alloc poller: %u, count: %u, ch_buf_read_size: %u", m_cfg.type, count, m_cfg.channel_read_buf_size);
			lock_guard<shared_mutex> lg(m_loop_mtx);
			m_curr_loop_idx = 0;
			NETP_ASSERT( m_fn_loop_maker != nullptr );
			while (count-- > 0) {
				event_loop_cfg __cfg = m_cfg;
				if (m_cfg.flag&f_th_thread_affinity) {
					++m_cfg.thread_affinity;
				}
				NRP<event_loop> o = m_fn_loop_maker(__cfg);
				int rt = o->__launch();
				NETP_ASSERT(rt == netp::OK);
				o->store_internal_ref_count(o.ref_count());
				m_loop.push_back(std::move(o));
			}
		}

		void event_loop_group::stop() {
			notify_terminating();
			wait();
		}

		netp::size_t event_loop_group::size() {
			shared_lock_guard<shared_mutex> lg(m_loop_mtx);
			return netp::size_t(m_loop.size());
		}

		//if there is a event_loop_group instance, we must always guarantee to return non-null loop instance
		NRP<event_loop> event_loop_group::next(std::set<NRP<event_loop>> const& exclude_this_list_if_have_more) {
			{
				shared_lock_guard<shared_mutex> lg(m_loop_mtx);
				if (m_loop.size() > 0) {
					const std::size_t psize = m_loop.size();
					if (psize <= exclude_this_list_if_have_more.size()) {
						//skip one time
						//u32_t idx = netp::atomic_incre(&m_curr_poller_idx[t]) % psize;
						//if (exclude_this_list_if_have_more.find(pollers[idx]) == exclude_this_list_if_have_more.end()) {
						//	return pollers[idx];
						//}
						return m_loop[m_curr_loop_idx.fetch_add(1, std::memory_order_relaxed) % psize];
					}
					else {
						u32_t idx = m_curr_loop_idx.fetch_add(1, std::memory_order_relaxed) % psize;
						while (exclude_this_list_if_have_more.find(m_loop[idx]) != exclude_this_list_if_have_more.end()) {
							idx = m_curr_loop_idx.fetch_add(1, std::memory_order_relaxed) % psize;
						}
						return m_loop[idx];
					}
				}
			}
			//as the load might happens on other thread, we need a memory barrier
			NRP<event_loop> __tmp = m_bye_event_loop;//may a copy first
			if (m_bye_state.load(std::memory_order_acquire) != bye_event_loop_state::S_IDLE) {
				NETP_ASSERT(__tmp != nullptr);
				NETP_VERBOSE("[event_loop][%u]return bye type", m_cfg.type);
				return __tmp;
			}
			NETP_THROW("event_loop_group deinit logic issue");
		}

		NRP<event_loop> event_loop_group::next() {
			{
				shared_lock_guard<shared_mutex> lg(m_loop_mtx);
				if (m_loop.size() != 0) {
					return m_loop[m_curr_loop_idx.fetch_add(1, std::memory_order_relaxed) % m_loop.size()];
				}
			}
			NRP<event_loop> __tmp = m_bye_event_loop;//may a copy first
			if(m_bye_state.load(std::memory_order_acquire) != bye_event_loop_state::S_IDLE) {
				NETP_ASSERT(__tmp != nullptr);
				NETP_VERBOSE("[event_loop][%u]return bye type", m_cfg.type);
				return __tmp;
			}
			NETP_THROW("event_loop_group deinit logic issue");
		}

		void event_loop_group::execute(fn_task_t&& f) {
			next()->execute(std::forward<fn_task_t>(f));
		}
		void event_loop_group::schedule(fn_task_t&& f) {
			next()->schedule(std::forward<fn_task_t>(f));
		}
		void event_loop_group::launch(NRP<netp::timer> const& t, NRP<netp::promise<int>> const& lf) {
			next()->launch(t,lf);
		}
}
