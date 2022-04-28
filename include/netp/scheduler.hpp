#ifndef _NETP_TASK_TASK_DISPATCHER_HPP_
#define _NETP_TASK_TASK_DISPATCHER_HPP_

#include <deque>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/singleton.hpp>
#include <netp/thread.hpp>
#include <netp/condition.hpp>

namespace netp {

	static const u8_t		P_HIGH = 0;
	static const u8_t		P_NORMAL = 1;
	static const u8_t		P_MAX = 2;

	typedef std::function<void()> fn_task_t;

	typedef std::deque<netp::non_atomic_shared_ptr<fn_task_t>, netp::allocator<fn_task_t> > task_queue_t;
	struct priority_task_queue {
		task_queue_t tasks[P_MAX];
		inline void push(fn_task_t const& t, u8_t const& p) {
			tasks[p].push_back( netp::make_non_atomic_shared<fn_task_t>(t) );
		}
		inline void push(fn_task_t&& t, u8_t const& p) {
			tasks[p].push_back(netp::make_non_atomic_shared<fn_task_t>(std::forward<fn_task_t>(t)));
		}
		inline bool front_and_pop(netp::non_atomic_shared_ptr<fn_task_t>& t) {
			for (u8_t i = 0; i < P_MAX; ++i) {
				if (tasks[i].size()) {
					t = tasks[i].front();
					tasks[i].pop_front();
					return true;
				}
			}
			return false;
		}
		inline bool empty() const {
			return	tasks[P_NORMAL].empty() && tasks[P_HIGH].empty();
		}
	};

		enum task_runner_state {
			TR_S_IDLE,
			TR_S_WAITING,
			TR_S_ASSIGNED,
			TR_S_RUNNING,
			TR_S_ENDING
		};

		class scheduler;
		class runner final :
			public thread_run_object_abstract
		{
			NETP_DECLARE_NONCOPYABLE(runner)

			u8_t m_id;
			std::atomic<u8_t> m_wait_flag;
			std::atomic<u8_t> m_state;

			scheduler* m_scheduler;
		public:
			runner(u8_t runner_id, scheduler* s);
			~runner();

			void stop();
			void on_start();
			void on_stop();
			void run();

			inline bool is_waiting() const { return m_state.load(std::memory_order_acquire) == TR_S_WAITING; }
			inline bool is_running() const { return m_state.load(std::memory_order_acquire) == TR_S_RUNNING; }
			inline bool is_idle() const { return m_state.load(std::memory_order_acquire) == TR_S_IDLE; }
			inline bool is_ending() const { return m_state.load(std::memory_order_acquire) == TR_S_ENDING; }

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

		typedef std::vector<NRP<runner>,netp::allocator<NRP<runner>> > TRV;

		class runner_pool {
		public:
			runner_pool(u8_t const& max_runner = 4);
			~runner_pool();

			void init(scheduler* s);
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

	//@please note that, this class is deprecated
	class scheduler:
		public netp::singleton<scheduler>
	{
		friend class runner;
		NETP_DECLARE_NONCOPYABLE(scheduler)

	private:
		spin_mutex m_mutex;
		condition_any m_condition;
		int m_state;
		u8_t m_tasks_runner_wait_count;
		u8_t m_max_concurrency;
		priority_task_queue* m_tasks_assigning;
		runner_pool* m_runner_pool;
	public:
		enum task_manager_state {
			S_IDLE,
			S_RUN,
			S_EXIT,
		};

		scheduler(u8_t const& max_runner_count = static_cast<u8_t>(std::thread::hardware_concurrency()));
		~scheduler();

		inline void schedule(fn_task_t const& t, u8_t const& p = P_NORMAL ) {
			lock_guard<spin_mutex> _lg(m_mutex);
			NETP_ASSERT( m_state == S_RUN );
			m_tasks_assigning->push(t, p);
			if (m_tasks_runner_wait_count > 0) m_condition.notify_one();
		}

		inline void schedule(fn_task_t&& t, u8_t const& p = P_NORMAL) {
			lock_guard<spin_mutex> _lg(m_mutex);
			NETP_ASSERT(m_state == S_RUN);
			m_tasks_assigning->push(std::move(t), p);
			if (m_tasks_runner_wait_count > 0) m_condition.notify_one();
		}

		void set_concurrency( u8_t const& max ) {
			unique_lock<spin_mutex> _lg( m_mutex );

			NETP_ASSERT( m_state != S_RUN );
			m_max_concurrency = max;
		}

		inline u8_t const& get_max_task_runner() const {return m_runner_pool->get_max_task_runner();}

		int start();
		void stop();

		void __on_start();
		void __on_stop();

		void __block_until_no_new_task();
	};
}

#define NETP_SCHEDULER (netp::scheduler::instance())
#endif