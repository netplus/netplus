#include <netp/timer.hpp>

namespace netp {


	void timer_broker::expire_all() {
		while (!m_tq.empty()) {
			NRP<timer>& tm = m_tq.front();
			NETP_ASSERT(tm->delay.count() >= 0);
			NETP_ASSERT(tm->expiration > timer_timepoint_t());
			m_heap.push(std::move(tm));
			m_tq.pop_front();
		}

		while (!m_heap.empty()) {
			NRP<timer>& tm = m_heap.front();
			tm->invoke(true);
			m_heap.pop();
		}
	}

	void timer_broker::expire(timer_duration_t& ndelay) {
		const bool shrink_or_not = m_tq.size() > NETP_TM_INIT_CAPACITY;
		while (!m_tq.empty()) {
			NRP<timer>& tm = m_tq.front();
			NETP_ASSERT(tm->delay.count() >= 0);
			NETP_ASSERT(tm->expiration > timer_timepoint_t());
			m_heap.push(std::move(tm));
			m_tq.pop_front();
		}
		if (shrink_or_not) { m_tq.shrink_to_fit(); }

		while (!m_heap.empty()) {
			NRP<timer>& tm = m_heap.front();
			ndelay = tm->invoke();
			if (ndelay.count() > 0) {
				goto _recalc_nexpire;
			} else {
				m_heap.pop();
			}
		}
		NETP_ASSERT(m_heap.size() == 0);
		//wait infinite
		ndelay = _TIMER_DURATION_INFINITE;
	_recalc_nexpire:
		{//double check, and recalc wait time
			if (m_tq.size() != 0) {
				ndelay = timer_duration_t();
			}
		}
	}

	/*
	timer_broker_ts::~timer_broker_ts()
	{
		{
			lock_guard<mutex> lg(m_mutex);
			m_in_exit = true;
		}

		if (m_th != nullptr) {
			m_th->interrupt();
			m_th->join();
		}

		NETP_WARN("[timer]cancel timer count: %u", m_heap.size());
		while (!m_heap.empty()) {
			NRP<timer>& tm = m_heap.front();
			tm->invoke();
			m_heap.pop();
		}
		NETP_WARN("[timer]cancel timer task count: %u", m_tq.size());
	}

	void timer_broker_ts::_run() {
		NETP_ASSERT(m_th != nullptr);
		{
			unique_lock<mutex> ulg(m_mutex);
			NETP_ASSERT(m_th != nullptr);
			NETP_ASSERT(m_th_break == false);
		}
		while (true) {
			update(m_ndelay);
			unique_lock<mutex> ulg(m_mutex);
			if (m_tq.size() == 0) {
				//exit this thread
				if (m_ndelay == _TIMER_DURATION_INFINITE) {
					m_th_break = true;
					break;
				}
				else {
					m_in_wait = true;
					m_cond.wait_for(ulg, m_ndelay);
					m_in_wait = false;
				}
			}
		}
	}

	void timer_broker_ts::update(timer_duration_t& ndelay) {
		{
			unique_lock<mutex> ulg(m_mutex);
			while (!m_tq.empty()) {
				NRP<timer>& tm = m_tq.front();
				NETP_ASSERT(tm->delay.count() >= 0);
				NETP_ASSERT(tm->expiration > timer_timepoint_t());
				m_heap.push(std::move(tm));
				m_tq.pop_front();
			}
		}
		while (!m_heap.empty()) {
			NRP<timer>& tm = m_heap.front();
			ndelay = tm->invoke();
			if (ndelay.count() > 0) {
				goto _recalc_nexpire;
			}
			else {
				m_heap.pop();
			}
		}
		NETP_ASSERT(m_heap.size() == 0);
		//wait infinite
		ndelay = _TIMER_DURATION_INFINITE;
	_recalc_nexpire:
		{//double check, and recalc wait time
			unique_lock<mutex> ulg(m_mutex);
			if (m_tq.size() != 0) {
				ndelay = timer_duration_t();
			}
		}
	}
	*/
}