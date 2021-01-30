#ifndef _NETP_TASK_TASK_DISPATCHER_HPP_
#define _NETP_TASK_TASK_DISPATCHER_HPP_

#include <vector>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/singleton.hpp>

#include <netp/thread.hpp>
#include <netp/task/task.hpp>
#include <netp/task/runner.hpp>

namespace netp { namespace task {

#ifdef NETP_SCHEDULER_USE_SPIN
	typedef spin_mutex scheduler_mutext_t;
#else
	typedef mutex scheduler_mutext_t;
#endif

	class scheduler:
		public netp::singleton<scheduler>
	{
		friend class runner;
		NETP_DECLARE_NONCOPYABLE(scheduler)

	private:
		scheduler_mutext_t m_mutex;

#ifdef NETP_SCHEDULER_USE_SPIN
		condition_any m_condition;
#else
		condition m_condition;
#endif
		volatile int m_state;
		u8_t m_tasks_runner_wait_count;
		u8_t m_max_concurrency;
		priority_task_queue* m_tasks_assigning;
		runner_pool* m_runner_pool;

#ifdef NETP_ENABLE_SEQUENCIAL_RUNNER
		u8_t m_max_seq_concurrency;
		sequencial_runner_pool* m_sequencial_runner_pool;
#endif

	public:
		enum task_manager_state {
			S_IDLE,
			S_RUN,
			S_EXIT,
		};


		scheduler(u8_t const& max_runner_count = static_cast<u8_t>(std::thread::hardware_concurrency())
#ifdef NETP_ENABLE_SEQUENCIAL_RUNNER
			,u8_t const& max_sequential_runner = static_cast<u8_t>(NETP_MAX2(1, static_cast<int>(std::thread::hardware_concurrency() >> 2)))
#endif
		);

		~scheduler();

		inline void schedule( NRP<task_abstract> const& ta, u8_t const& p = P_NORMAL ) {
			lock_guard<scheduler_mutext_t> _lg(m_mutex);
			NETP_ASSERT( m_state == S_RUN );
			m_tasks_assigning->push(ta, p);
			if (m_tasks_runner_wait_count > 0) m_condition.notify_one();
		}

		inline void schedule(fn_task_void const& task_fn_, u8_t const& priority = netp::task::P_NORMAL) {
			schedule(netp::make_ref<task>(task_fn_), priority);
		}

#ifdef NETP_ENABLE_SEQUENCIAL_RUNNER
		inline void schedule(NRP<sequencial_task> const& ta) {
			NETP_ASSERT(m_state == S_RUN);
			m_sequencial_runner_pool->assign_task(ta);
		}
#endif

		void set_concurrency( u8_t const& max ) {
			unique_lock<scheduler_mutext_t> _lg( m_mutex );

			NETP_ASSERT( m_state != S_RUN );
			m_max_concurrency = max;
		}

#ifdef NETP_ENABLE_SEQUENCIAL_RUNNER
		void set_seq_concurrency(u8_t const& max) {
			unique_lock<scheduler_mutext_t> _lg(m_mutex);

			NETP_ASSERT(m_state != S_RUN);
			m_max_seq_concurrency = max;
		}
#endif
		inline u8_t const& get_max_task_runner() const {return m_runner_pool->get_max_task_runner();}

		int start();
		void stop();

		void __on_start();
		void __on_stop();

		void __block_until_no_new_task();
	};
}}

#define WW_SCHEDULER (netp::task::scheduler::instance())
#endif