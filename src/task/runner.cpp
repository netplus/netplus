#include <netp/logger_broker.hpp>
#include <netp/task/scheduler.hpp>
#include <netp/task/runner.hpp>

namespace netp { namespace task {

	runner::runner( u8_t id, scheduler* s ) :
		m_id( id ),
		m_wait_flag(0),
		m_state(TR_S_IDLE),
		m_scheduler(s)
	{
		NETP_TRACE_TASK( "[TRunner][-%u-]construct new runner", m_id );
	}

	runner::~runner() {
		stop() ;
		NETP_TRACE_TASK( "[TRunner][-%u-]destructTaskRunner", m_id );
	}

	void runner::on_start() {
		m_state = TR_S_IDLE ;
	}

	void runner::on_stop() {
		NETP_TRACE_TASK("[TRunner][-%u-]runner exit...", m_id ) ;
	}

	void runner::stop() {
		{
			while (m_state != TR_S_WAITING) {
				netp::this_thread::no_interrupt_yield(1);
			}

			m_state = TR_S_ENDING;
			NETP_TRACE_TASK("[TRunner][-%u-]runner enter into state [%d]", m_id, m_state ) ;
		}

		{
			lock_guard<scheduler_mutext_t> lg_schd(m_scheduler->m_mutex);
			m_scheduler->m_condition.no_interrupt_notify_all();
		}
		thread_run_object_abstract::stop();
	}

	void runner::run() {

		NETP_ASSERT(m_scheduler != nullptr);
		while (1)
		{
			NRP<netp::task::task_abstract> task;
			{
			unique_lock<scheduler_mutext_t> _ulk(m_scheduler->m_mutex);
			check_begin:
				{
					if (m_state == TR_S_ENDING) {
						break;
					}
				}

				m_scheduler->m_tasks_assigning->front_and_pop(task);
				if (task == nullptr) {
					m_state = TR_S_WAITING;
					++(m_scheduler->m_tasks_runner_wait_count);
					m_scheduler->m_condition.no_interrupt_wait(_ulk);
					--(m_scheduler->m_tasks_runner_wait_count);
					goto check_begin;
				}
			}
			NETP_ASSERT(task != nullptr);

			m_state = TR_S_RUNNING;
			m_wait_flag = 0;
			try {
				NETP_TRACE_TASK("[TRunner][-%d-]task run begin, TID: %llu", m_id, reinterpret_cast<u64_t>(task.get()));
				task->run();
				NETP_TRACE_TASK("[TRunner][-%d-]task run end, TID: %llu", m_id, reinterpret_cast<u64_t>(task.get()));
			}
			catch (netp::exception& e) {
				NETP_ERR("[TRunner][-%d-]runner netp::exception: [%d]%s\n%s(%d) %s\n%s",
					m_id, e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
				throw;
			}
			catch (std::exception& e) {
				NETP_ERR("[TRunner][-%d-]runner exception: %s", m_id, e.what());
				throw;
			}
			catch (...) {
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

	void runner_pool::init( scheduler* s ) {
		netp::lock_guard<mutex> _lg(m_mutex);
		m_is_running = true;
		m_last_runner_idx = 0;

		u8_t i = 0;
		while (m_runners.size() < m_max_concurrency) {
			NRP<runner> tr = netp::make_ref<runner>(i,s);
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

}}//END OF NS