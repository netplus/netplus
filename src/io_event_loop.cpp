#include <netp/core.hpp>
#if defined(NETP_HAS_POLLER_EPOLL)
	#include <netp/poller_epoll.hpp>
#elif defined(NETP_HAS_POLLER_IOCP)
	#include <netp/poller_iocp.hpp>
#elif defined(NETP_HAS_POLLER_KQUEUE)
	#include <netp/poller_kqueue.hpp>
#else
	#include <netp/poller_select.hpp>
#endif


#include <netp/io_event_loop.hpp>
#include <netp/socket_api.hpp>

namespace netp {

	inline static NRP<io_event_loop> default_poller_maker(io_poller_type t, poller_cfg const& cfg) {
		NRP<io_event_loop> poller;
		switch (t) {
#if defined(NETP_HAS_POLLER_EPOLL)
		case T_EPOLL:
		{
			poller = netp::make_ref<poller_epoll>(cfg);
			NETP_ALLOC_CHECK(poller, sizeof(poller_epoll));
		}
		break;
#elif defined(NETP_HAS_POLLER_IOCP)
		case T_IOCP:
		{
			poller = netp::make_ref<poller_iocp>(cfg);
			NETP_ALLOC_CHECK(poller, sizeof(poller_iocp));
		}
		break;
#elif defined(NETP_HAS_POLLER_KQUEUE)
		case T_KQUEUE:
		{
			poller = netp::make_ref<poller_kqueue>(cfg);
			NETP_ALLOC_CHECK(poller, sizeof(poller_kqueue));
		}
		break;
#else
		case T_SELECT:
		{
			poller = netp::make_ref<poller_select>(cfg);
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
		return poller;

	}

	void io_event_loop::_do_poller_init() {
		NETP_ASSERT(in_event_loop());

		int rt = netp::socketpair( int(NETP_AF_INET), int(NETP_SOCK_STREAM), int(NETP_PROTOCOL_TCP), m_signalfds);
		NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

		rt = netp::turnon_nonblocking(netp::NETP_DEFAULT_SOCKAPI,m_signalfds[0]);
		NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

		rt = netp::turnon_nonblocking(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[1]);
		NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

		rt = netp::turnon_nodelay(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[1]);
		NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

		NETP_ASSERT(rt == netp::OK);

		m_signalfds_aio_ctx = aio_begin(m_signalfds[0]);
		NETP_ASSERT(m_signalfds_aio_ctx != 0);
		m_signalfds_aio_ctx->fn_read = [](int status, aio_ctx* ctx) {
			if (status == netp::OK) {
				byte_t tmp[1];
				int ec = netp::OK;
				do {
					u32_t c = netp::recv(netp::NETP_DEFAULT_SOCKAPI, ctx->fd, tmp, 1, ec, 0);
					if (c == 1) {
						NETP_ASSERT(ec == netp::OK);
						NETP_ASSERT(tmp[0] == 'i', "c: %d", tmp[0]);
					}
				} while (ec == netp::OK);
			}
			return netp::OK;
		};
		rt = aio_do(aio_action::READ, m_signalfds_aio_ctx);
		NETP_ASSERT(rt == netp::OK);
	}

	void io_event_loop::_do_poller_deinit() {
		NETP_ASSERT(in_event_loop());
		aio_do(aio_action::END_READ, m_signalfds_aio_ctx);
		m_signalfds_aio_ctx->fn_read = nullptr;
		aio_end(m_signalfds_aio_ctx) ;

		NETP_CLOSE_SOCKET(m_signalfds[0]);
		NETP_CLOSE_SOCKET(m_signalfds[1]);
		m_signalfds[0] = (SOCKET)NETP_INVALID_SOCKET;
		m_signalfds[1] = (SOCKET)NETP_INVALID_SOCKET;

		NETP_TRACE_IOE("[io_event_loop][default]deinit done");
	}

	 void io_event_loop::_do_poller_interrupt_wait() {
		 NETP_ASSERT(!in_event_loop());
		NETP_ASSERT(m_signalfds[0] > 0);
		NETP_ASSERT(m_signalfds[1] > 0);
		int ec;
		const byte_t interrutp_a[1] = { (byte_t) 'i' };
		NETP_ASSERT(interrutp_a[0] == 'i');

		u32_t c = netp::send(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[1], interrutp_a, 1, ec, 0);
		if (NETP_UNLIKELY(ec != netp::OK)) {
			NETP_WARN("[io_event_loop]interrupt send failed: %d", ec);
		}
		(void)c;
	}

		//@NOTE: promise to execute all task already in tq or tq_standby
		void io_event_loop::__run() {
//			NETP_ASSERT(!"CHECK EXCEPTION STACK");
			init();
			u8_t _SL = u8_t(loop_state::S_LAUNCHING);
			const bool rt = m_state.compare_exchange_strong(_SL, u8_t(loop_state::S_RUNNING), std::memory_order_acq_rel, std::memory_order_acquire);
			NETP_ASSERT(rt == true);
			try {
				//this load also act as a memory synchronization fence to sure all release operation happen before this line is synchronized
				//if we make_ref a atomic_ref object, then we call L->schedule([o=atomic_ref_instance](){});
				//all member value of that object must be synchronized after this line, cuz we have netp::atomic_incre inside ref object
				while( NETP_UNLIKELY(u8_t(loop_state::S_EXIT) != m_state.load(std::memory_order_acquire)) ) {
					{
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
						} else {
							m_tq.clear();
						}
					}

					//__do_execute_act();
					_do_poll(_calc_wait_dur_in_nano());
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
				// EDGE check
				// scenario 1:
				// 1) do schedule, 2) set L -> null
				std::size_t i = 0;
				std::size_t vecs = m_tq_standby.size();
				while (i<vecs) {
					m_tq_standby[i++]();
				}
				m_tq_standby.clear();
			}
			deinit();
		}

		//terminating phase 1
		void io_event_loop::__notify_terminating() {
			u8_t running = u8_t(loop_state::S_RUNNING);
			if (m_state.compare_exchange_strong(running, u8_t(loop_state::S_TERMINATING), std::memory_order_acq_rel, std::memory_order_acquire)) {
				schedule([L = NRP<io_event_loop>(this)]() {
#ifdef NETP_HAS_POLLER_IOCP
					if (L->type() == T_IOCP) {
						L->iocp_do(iocp_action::NOTIFY_TERMINATING, 0, nullptr, nullptr);
					} else {//patch for bye
						L->aio_do(aio_action::NOTIFY_TERMINATING, 0, nullptr);
					}
#else
					L->aio_do(aio_action::NOTIFY_TERMINATING, 0);
#endif
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
			return netp::OK;
		}

		void io_event_loop::__terminate() {
			NETP_DEBUG("[io_event_loop][%u]__terminate begin", m_type );
			while (m_state.load(std::memory_order_acquire) != u8_t(loop_state::S_TERMINATED)) {
				netp::this_thread::usleep(16);
			}
			u8_t terminated = u8_t(loop_state::S_TERMINATED);
			if (m_state.compare_exchange_strong(terminated, u8_t(loop_state::S_EXIT), std::memory_order_acq_rel, std::memory_order_acquire)) {
				NETP_DEBUG("[io_event_loop][%u]enter S_EXIT, last interrupt if needed", m_type );
				_do_poller_interrupt_wait();
				//@NOTE:
				//1, we don't interrupt these kinds of thread, cuz we want all the tasks be finished one by one
				//m_th->interrupt();
				m_th->join();
				m_th = nullptr;
			}
			NETP_INFO("[io_event_loop][%u]__terminate end", m_type );
		}

		void bye_event_loop::_do_poll(long long wait_in_nano) {
			/*just wat...*/
			netp::this_thread::usleep(8);
			__LOOP_EXIT_WAITING__();
			(void)wait_in_nano;
		}

		int bye_event_loop::_do_watch(u8_t flag, aio_ctx* ctx ) {
			NETP_ASSERT(in_event_loop());
			//NETP_ASSERT(flag == IOE_INIT);

			NETP_ERR("[bye_event_loop]_do_watch(%d,%d), cancel", flag, ctx->fd);
			return netp::E_IO_EVENT_LOOP_BYE_DO_NOTHING;

			//(void)fn;
			//NETP_THROW("[do_watch]io_event_loop_group dealloc logic issue");
		}

		int bye_event_loop::_do_unwatch(u8_t flag, aio_ctx* ctx) {
			NETP_ERR("[bye_event_loop]do_unwatch(%d,%d)", flag, ctx->fd);
			NETP_THROW("[do_unwatch]io_event_loop_group dealloc logic issue");
		}

		io_event_loop_group::io_event_loop_group():
			m_bye_ref_count(0),
			m_bye_state(bye_event_loop_state::S_IDLE)
		{
			//NETP_DEBUG("netp::io_event_loop_group::io_event_loop_group()");
		}
		io_event_loop_group::~io_event_loop_group()
		{
			//NETP_DEBUG("netp::io_event_loop_group::~io_event_loop_group()");
		}

		void io_event_loop_group::notify_terminating(io_poller_type t) {
			shared_lock_guard<shared_mutex> slg(m_pollers_mtx[t]);
			for(::size_t i=0;i<m_pollers[t].size();++i) {
				m_pollers[t][i]->__notify_terminating();
			}
		}

		void io_event_loop_group::alloc_add_poller(io_poller_type t, int count, poller_cfg const& cfg, fn_poller_maker_t const& fn_maker ) {
			NETP_DEBUG("[io_event_loop_group]alloc poller: %u, count: %u, ch_buf_size: %u, maxiumctx: %u", t, count, cfg.ch_buf_size, cfg.maxiumctx );
			lock_guard<shared_mutex> lg(m_pollers_mtx[t]);
			m_curr_poller_idx[t] = 0;
			while (count-- > 0) {
				NRP<io_event_loop> o = fn_maker == nullptr ?
					default_poller_maker(t,cfg) : 
					fn_maker(t,cfg);

				int rt = o->__launch();
				NETP_ASSERT(rt == netp::OK);
				m_pollers[t].push_back(o);
				o->__internal_ref_count_inc();
			}
		}

		void io_event_loop_group::dealloc_remove_poller(io_poller_type t) {

		__dealloc_begin:
			std::vector<NRP<io_event_loop>> to_deattach;
			{
				lock_guard<shared_mutex> lg(m_pollers_mtx[t]);
				if (m_pollers[t].size() == 0) {
					//NETP_INFO("[io_event_loop][%u]__dealloc_poller end, all event loop dattached", t);
					return;//exit
				}

				bye_event_loop_state idle = bye_event_loop_state::S_IDLE;
				if (m_bye_state.compare_exchange_strong(idle, bye_event_loop_state::S_PREPARING, std::memory_order_acq_rel, std::memory_order_acquire)) {
					NETP_ASSERT(m_bye_event_loop == nullptr);
					m_bye_event_loop = netp::make_ref<bye_event_loop>(T_BYE, poller_cfg{});
					int rt = m_bye_event_loop->__launch();
					NETP_ASSERT(rt == netp::OK);
					m_bye_ref_count = m_bye_event_loop.ref_count();

					bye_event_loop_state preparing = bye_event_loop_state::S_PREPARING;
					bool set_to_running = m_bye_state.compare_exchange_strong(preparing, bye_event_loop_state::S_RUNNING, std::memory_order_acq_rel, std::memory_order_acquire);
					NETP_ASSERT(set_to_running == true);
				}

				io_event_loop_vector::iterator&& it = m_pollers[t].begin();
				while( it != m_pollers[t].end() ) {
					//ref_count == internal_ref_count means no other ref for this LOOP, we must deattach it from our pool
					if((*it).ref_count() == (*it)->internal_ref_count() ) {
						NETP_DEBUG("[io_event_loop][%u]__dealloc_poller, dattached one event loop", t);

						to_deattach.push_back(*it);
						m_pollers[t].erase(it);
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
			netp::this_thread::usleep(8);
			goto __dealloc_begin;
		}

		void io_event_loop_group::init(int count[io_poller_type::T_POLLER_MAX], poller_cfg cfgs[io_poller_type::T_POLLER_MAX]) {
			for (int i = 0; i < T_POLLER_MAX; ++i) {
				if (count[i] > 0) {
					alloc_add_poller(io_poller_type(i), count[i], cfgs[i]);
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
		void io_event_loop_group::deinit() {

			//phase 1, terminating
			for (int i= T_POLLER_MAX-1; i >=0; --i) {
				if (m_pollers[i].size()) {
					NETP_INFO("[io_event_loop]__notify_terminating, type: %d", io_poller_type(i) );
					notify_terminating(io_poller_type(i));
				}
			}

			//phase 2, deattach one by one
			for (int i = T_POLLER_MAX - 1; i >= 0; --i) {
				if (m_pollers[i].size()) {
					NETP_INFO("[io_event_loop]__dealloc_poller, type: %d", io_poller_type(i));
					dealloc_remove_poller(io_poller_type(i));
				}
			}

			bye_event_loop_state running = bye_event_loop_state::S_RUNNING;
			if (m_bye_state.compare_exchange_strong(running, bye_event_loop_state::S_EXIT, std::memory_order_acq_rel, std::memory_order_acquire)) {
				NETP_ASSERT(m_bye_event_loop != nullptr);
				m_bye_event_loop->__notify_terminating();
				while (m_bye_ref_count != m_bye_event_loop.ref_count()) {
					netp::this_thread::usleep(8);
				}
				m_bye_event_loop->__terminate();
				m_bye_event_loop = nullptr;
			}
		}

		io_poller_type io_event_loop_group::query_available_custom_poller_type() {
			for (int i = T_POLLER_CUSTOM_1; i < T_POLLER_MAX; ++i) {
				if (m_pollers[i].size() == 0) {
					return io_poller_type(i);
				}
			}
			return T_NONE;
		}

		netp::size_t io_event_loop_group::size(io_poller_type t) {
			shared_lock_guard<shared_mutex> lg(m_pollers_mtx[t]);
			return netp::size_t(m_pollers[t].size());
		}

		NRP<io_event_loop> io_event_loop_group::next(io_poller_type t, std::set<NRP<io_event_loop>> const& exclude_this_list_if_have_more) {
			{
				shared_lock_guard<shared_mutex> lg(m_pollers_mtx[t]);
				const io_event_loop_vector& pollers = m_pollers[t];
				if (pollers.size() > 0) {
					const std::size_t psize = pollers.size();
					if (psize <= exclude_this_list_if_have_more.size()) {
						//skip one time
						//u32_t idx = netp::atomic_incre(&m_curr_poller_idx[t]) % psize;
						//if (exclude_this_list_if_have_more.find(pollers[idx]) == exclude_this_list_if_have_more.end()) {
						//	return pollers[idx];
						//}
						return pollers[netp::atomic_incre(&m_curr_poller_idx[t]) % psize];
					}
					else {
						u32_t idx = netp::atomic_incre(&m_curr_poller_idx[t]) % psize;
						while (exclude_this_list_if_have_more.find(pollers[idx]) != exclude_this_list_if_have_more.end()) {
							idx = netp::atomic_incre(&m_curr_poller_idx[t]) % psize;
						}
						return pollers[idx];
					}
				}
			}

			if (m_bye_state.load(std::memory_order_acquire) == bye_event_loop_state::S_RUNNING) {
				return m_bye_event_loop;
			}
			NETP_THROW("io_event_loop_group deinit logic issue");
		}

		NRP<io_event_loop> io_event_loop_group::next(io_poller_type t) {
			{
				shared_lock_guard<shared_mutex> lg(m_pollers_mtx[t]);
				const io_event_loop_vector& pollers = m_pollers[t];
				if (pollers.size() != 0) {
					return pollers[netp::atomic_incre(&m_curr_poller_idx[t]) % pollers.size()];
				}
			}
			if(m_bye_state.load(std::memory_order_acquire) == bye_event_loop_state::S_RUNNING) {
				return m_bye_event_loop;
			}
			NETP_THROW("io_event_loop_group deinit logic issue");
		}

		NRP<io_event_loop> io_event_loop_group::internal_next(io_poller_type t) {
			shared_lock_guard<shared_mutex> lg(m_pollers_mtx[t]);
			const io_event_loop_vector& pollers = m_pollers[t];
			NETP_ASSERT(m_pollers[t].size() != 0);
			int idx = netp::atomic_incre(&m_curr_poller_idx[t]) % pollers.size();
			pollers[idx]->__internal_ref_count_inc();
			return pollers[idx];
			//NETP_THROW("io_event_loop_group deinit logic issue");
		}

		void io_event_loop_group::execute(fn_io_event_task_t&& f, io_poller_type poller_t) {
			next(poller_t)->execute(std::forward<fn_io_event_task_t>(f));
		}
		void io_event_loop_group::schedule(fn_io_event_task_t&& f, io_poller_type poller_t) {
			next(poller_t)->schedule(std::forward<fn_io_event_task_t>(f));
		}
		void io_event_loop_group::launch(NRP<netp::timer> const& t, NRP<netp::promise<int>> const& lf, io_poller_type poller_t) {
			next(poller_t)->launch(t,lf);
		}
}
