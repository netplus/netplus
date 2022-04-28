
#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/logger_broker.hpp>

#include <netp/scheduler.hpp>

namespace netp {

	runner::runner(u8_t id, scheduler* s) :
		m_id(id),
		m_wait_flag(0),
		m_state(task_runner_state::TR_S_IDLE),
		m_scheduler(s)
	{
		NETP_TRACE_TASK("[TRunner][-%u-]construct new runner", m_id);
	}

	runner::~runner() {
		stop();
		NETP_TRACE_TASK("[TRunner][-%u-]destructTaskRunner", m_id);
	}

	void runner::on_start() {
		m_state.store(task_runner_state::TR_S_IDLE, std::memory_order_release);
	}

	void runner::on_stop() {
		NETP_TRACE_TASK("[TRunner][-%u-]runner exit...", m_id);
	}

	void runner::stop() {
		{
			while (m_state.load(std::memory_order_acquire) != TR_S_WAITING) {
				netp::this_thread::no_interrupt_yield(1);
			}

			m_state = TR_S_ENDING;
			NETP_TRACE_TASK("[TRunner][-%u-]runner enter into state [%d]", m_id, m_state);
		}

		{
			lock_guard<spin_mutex> lg_schd(m_scheduler->m_mutex);
			m_scheduler->m_condition.no_interrupt_notify_all();
		}
		thread_run_object_abstract::stop();
	}

	void runner::run() {

		NETP_ASSERT(m_scheduler != nullptr);
		while (1)
		{
			netp::non_atomic_shared_ptr<fn_task_t> task = nullptr;
			{
				unique_lock<spin_mutex> _ulk(m_scheduler->m_mutex);
			check_begin:
				{
					if (m_state.load(std::memory_order_acquire) == TR_S_ENDING) {
						break;
					}
				}

				bool has_any = m_scheduler->m_tasks_assigning->front_and_pop(task);
				if (!has_any) {
					m_state.store(TR_S_WAITING, std::memory_order_release);
					++(m_scheduler->m_tasks_runner_wait_count);
					m_scheduler->m_condition.no_interrupt_wait(_ulk);
					--(m_scheduler->m_tasks_runner_wait_count);
					goto check_begin;
				}
			}
			m_state.store(TR_S_RUNNING, std::memory_order_release);
			m_wait_flag.store(0, std::memory_order_release);

			NETP_ASSERT(task != nullptr);
			try {
				(*task)();
			} catch (netp::exception& e) {
				NETP_ERR("[TRunner][-%d-]runner netp::exception: [%d]%s\n%s(%d) %s\n%s",
					m_id, e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
				throw;
			} catch (std::exception& e) {
				NETP_ERR("[TRunner][-%d-]runner exception: %s", m_id, e.what());
				throw;
			} catch (...) {
				NETP_ERR("[TRunner][-%d-]runner, unknown exception", m_id);
				throw;
			}
		}
	}

	runner_pool::runner_pool(u8_t const& max_runner) :
		m_mutex(),
		m_runners(),
		m_is_running(false),
		m_max_concurrency(max_runner),
		m_last_runner_idx(0)
	{
	}

	runner_pool::~runner_pool() {}

	void runner_pool::init(scheduler* s) {
		netp::lock_guard<mutex> _lg(m_mutex);
		m_is_running = true;
		m_last_runner_idx = 0;

		u8_t i = 0;
		while (m_runners.size() < m_max_concurrency) {
			NRP<runner> tr = netp::make_ref<runner>(i, s);
			NETP_ALLOC_CHECK(tr, sizeof(runner));
			int rt = tr->start();
			if (rt != netp::OK) {
				tr->stop();
				NETP_THROW("create thread failed");
			}
			++i;
			m_runners.push_back(tr);
		}
	}

	void runner_pool::deinit() {
		netp::lock_guard<mutex> _lg(m_mutex);
		if (m_is_running == false) { return; }

		m_is_running = false;
		while (!m_runners.empty()) {
			m_runners.pop_back();
		}
	}

	void runner_pool::set_max_task_runner(u8_t const& count) {
		unique_lock<mutex> _lg(m_mutex);
		m_max_concurrency = count;
		//dynamic clear ,,if ...
		while (m_runners.size() > m_max_concurrency) {
			m_runners.pop_back();
		}
	}
	scheduler::scheduler( u8_t const& max_runner):
		m_mutex(),
		m_condition(),
		m_state( S_IDLE ),
		m_max_concurrency(max_runner),
		m_runner_pool(nullptr)
	{
	}

	scheduler::~scheduler() {
		stop();
		NETP_TRACE_TASK("[scheduler]~scheduler()");
	}

	int scheduler::start() {
		{
			unique_lock<spin_mutex> _ul(m_mutex);
			if (m_state == S_EXIT) {
				m_state = S_IDLE;
			}
		}
		__on_start();
		return netp::OK;
	}

	void scheduler::stop() {
		{
			NETP_TRACE_TASK("scheduler::stop begin");
			{
				lock_guard<spin_mutex> _ul(m_mutex);
				if (m_state == S_EXIT || m_state == S_IDLE) { return; }
			}
			NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task()");

			//experiment
			__block_until_no_new_task();
			NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task() exit");

			lock_guard<spin_mutex> _ul( m_mutex );
			if( m_state == S_EXIT || m_state == S_IDLE ) { return; }

			m_state = S_EXIT;
			m_condition.notify_one();
		}
		NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task() on_stop");
		__on_stop();
		NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task() on_stop exit");
	}

	void scheduler::__on_start() {
		unique_lock<spin_mutex> _ul( m_mutex );

		m_state = S_RUN;
		m_tasks_assigning = netp::allocator<priority_task_queue>::make();
		m_tasks_runner_wait_count = 0;

		m_runner_pool = netp::allocator<runner_pool>::make( m_max_concurrency ) ;
		NETP_ALLOC_CHECK( m_runner_pool, sizeof(runner_pool) ) ;
		m_runner_pool->init(this);
	}

	void scheduler::__on_stop() {
		NETP_ASSERT( m_state == S_EXIT );
		NETP_ASSERT( m_tasks_assigning->empty() );

		m_runner_pool->deinit();

		netp::allocator<priority_task_queue>::trash( m_tasks_assigning );
		netp::allocator<runner_pool>::trash(m_runner_pool);
		m_tasks_assigning = 0;
		 m_runner_pool =0;
	}

	void scheduler::__block_until_no_new_task() {

		u8_t self_flag = 0;
		u8_t runner_flag = 0;
		while (1) {
		begin:

			if (self_flag == 0) {
				while (!m_tasks_assigning->empty()) {
					netp::this_thread::no_interrupt_yield(50);
					runner_flag = 0;
				}
				self_flag = 1;
			}
			else if (self_flag == 1) {
				while (!m_tasks_assigning->empty()) {
					self_flag = 0;
					runner_flag = 0;
					netp::this_thread::no_interrupt_yield(50);
					goto begin;
				}
				self_flag = 2;
			}

			if (runner_flag == 0) {
				while (!m_runner_pool->test_waiting_step1()) {
					netp::this_thread::no_interrupt_yield(50);
					self_flag = 0;
					runner_flag = 0;
					goto begin;
				}
				runner_flag = 1;
			}
			else if (runner_flag == 1) {
				while (!m_runner_pool->test_waiting_step2()	) {
					netp::this_thread::no_interrupt_yield(50);
					self_flag = 0;
					runner_flag = 0;
					goto begin;
				}
				runner_flag = 2;
			}

			if (self_flag == 2 && runner_flag == 2) {
				break;
			}
		}
	}
}//end of ns
