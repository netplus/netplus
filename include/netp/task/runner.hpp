#ifndef _NETP_TASK_TASKRUNNER_HPP_
#define _NETP_TASK_TASKRUNNER_HPP_

#include <queue>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/mutex.hpp>
#include <netp/condition.hpp>
#include <netp/task/task.hpp>

//#define NETP_TASK_RUNNER_USE_SPIN_MUTEX

//#define NETP_PULL_TASK
//#define NETP_PUSH_TASK

namespace netp { namespace task {
	 
	static const u8_t		P_HIGH = 0;
	static const u8_t		P_NORMAL = 1;
	static const u8_t		P_MAX = 2;

	typedef std::deque< NRP<task_abstract>,netp::allocator<NRP<task_abstract>> > task_queue;
	struct priority_task_queue {
		task_queue tasks[P_MAX];
		inline void push(NRP<task_abstract> const& t, u8_t const& p) {
			tasks[p].push_back(t);
		}
		inline void front_and_pop(NRP<task_abstract>& t) {
			for (u8_t i = 0; i < P_MAX; ++i) {
				if (tasks[i].size()) {
					t = tasks[i].front();
					tasks[i].pop_front();
					return ;
				}
			}
		}
		inline bool empty() const {
			return	tasks[P_NORMAL].empty() && tasks[P_HIGH].empty();
		}
	};
}}

namespace netp { namespace task {
	
	typedef std::vector< NRP<task_abstract>,netp::allocator<NRP<task_abstract>> > task_vector;

	enum task_runner_state {
		TR_S_IDLE,
		TR_S_WAITING,
		TR_S_ASSIGNED,
		TR_S_RUNNING,
		TR_S_ENDING
	};

#ifdef NETP_TASK_RUNNER_USE_SPIN_MUTEX
	typedef spin_mutex task_runner_mutex_t;
#else
	typedef mutex task_runner_mutex_t;
#endif

	class scheduler;
	class task;
	class runner final:
		public thread_run_object_abstract
	{
		NETP_DECLARE_NONCOPYABLE(runner)

		u8_t m_id;
		std::atomic<u8_t> m_wait_flag;
		std::atomic<u8_t> m_state;

		scheduler* m_scheduler;
	public:
		runner( u8_t runner_id, scheduler* s );
		~runner();

		void stop();
		void on_start() ;
		void on_stop() ;
		void run();

		inline bool is_waiting() const { return m_state == TR_S_WAITING ;}
		inline bool is_running() const { return m_state == TR_S_RUNNING ;}
		inline bool is_idle() const { return m_state == TR_S_IDLE ;}
		inline bool is_ending() const { return m_state == TR_S_ENDING ;}

		inline bool test_waiting_step1() {
			if (m_state.load(std::memory_order_acquire) == TR_S_WAITING) {
				m_wait_flag.store(1, std::memory_order_release);
				return true;
			}
			return false;
		}

		inline bool test_waiting_step2() {
			return (m_wait_flag.load(std::memory_order_acquire) == 1) && (m_state.load(std::memory_order_acquire) == TR_S_WAITING);
		}
	};

	typedef std::vector< NRP<runner> > TRV;

	class runner_pool {
	public:
		runner_pool(u8_t const& max_runner = 4);
		~runner_pool();

		void init( scheduler* s );
		void deinit();

	public:

		inline void set_max_task_runner(u8_t const& count);
		inline u8_t const& get_max_task_runner() const { return m_max_concurrency; }

		bool test_waiting_step1() {
			for (u32_t i = 0; i < m_runners.size(); ++i) {
				if (!m_runners[i]->test_waiting_step1()) {
					return false;
				}
			}
			return true;
		}

		bool test_waiting_step2() {
			for (u32_t i = 0; i < m_runners.size(); ++i) {
				if (!m_runners[i]->test_waiting_step2()) {
					return false;
				}
			}
			return true;
		}

	private:
		mutex m_mutex;
		TRV m_runners;
		bool m_is_running : 1;
		u8_t m_max_concurrency;
		u8_t m_last_runner_idx;
	};
}}
#endif