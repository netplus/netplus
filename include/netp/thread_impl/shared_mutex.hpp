#ifndef NETP_THREAD_IMPL_SHARD_MUTEX_HPP
#define NETP_THREAD_IMPL_SHARD_MUTEX_HPP

#include <netp/core.hpp>
#include <netp/condition.hpp>
#include <netp/thread_impl/mutex_basic.hpp>

namespace netp { namespace impl {
		
	class shared_mutex final
	{
		NETP_DECLARE_NONCOPYABLE(shared_mutex)

		enum exclusive {
			NO = 0,
			YES = 1
		};

		//the idea borrowed from boost
		class state_data {
		public:
			enum UpgradeState {
				S_UP_SET = 1,
				S_UP_UPING,
				S_UP_NOSET
			};

			netp::u32_t shared_count;
			netp::u8_t upgrade;
			netp::u8_t exclu;

			state_data() :
				shared_count(0),
				upgrade(S_UP_NOSET),
				exclu(exclusive::NO)
			{}

			inline bool can_lock() const {
				return !(shared_count || exclu == exclusive::YES);
			}
			inline void lock() {
				exclu = exclusive::YES;
			}
			inline void unlock() {
				exclu = exclusive::NO;
			}
			inline bool can_lock_shared() const {
				return !(exclu || upgrade != S_UP_NOSET);
			}
			inline bool has_more_shared() const {
				return shared_count > 0;
			}
			inline u32_t get_shared_count() const {
				return shared_count;
			}
			inline void lock_shared() {
				(++shared_count);
//				return shared_count;
			}
			inline void unlock_shared() {
				NETP_ASSERT(!exclu);
				NETP_ASSERT(shared_count > 0);
				--shared_count;
			}

			inline bool can_lock_upgrade() const {
				return !(exclu || upgrade != S_UP_NOSET);
			}
			inline void lock_upgrade() {
				NETP_ASSERT(!is_upgrade_set());
				++shared_count;
				upgrade = S_UP_SET;
			}
			inline void unlock_upgrade() {
				NETP_ASSERT(upgrade == S_UP_SET);
				upgrade = S_UP_NOSET;
				NETP_ASSERT(shared_count > 0);
				--shared_count;
			}
			inline bool is_upgrade_set() const {
				return upgrade == S_UP_SET;
			}
			inline void unlock_upgrade_and_up() {
				NETP_ASSERT(upgrade == S_UP_SET);
				upgrade = S_UP_UPING;
				NETP_ASSERT(shared_count > 0);
				--shared_count;
			}
			inline void lock_upgrade_up_done() {
				NETP_ASSERT(upgrade == S_UP_UPING);
				upgrade = S_UP_NOSET;
			}
		};

		//shared -> upgrade -> lock

		state_data m_state;
		spin_mutex m_state_mutex;

		netp::condition_any m_shared_cond;
		netp::condition_any m_exclusive_cond;
		netp::condition_any m_upgrade_cond;

		inline void _notify_waiters() {
			m_exclusive_cond.notify_one();
			m_shared_cond.notify_all();
		}

	public:
		shared_mutex() :
			m_state(),
			m_state_mutex(),
			m_shared_cond(),
			m_exclusive_cond(),
			m_upgrade_cond()
		{}
		~shared_mutex() {}

		void lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			while (!m_state.can_lock_shared()) {
				m_shared_cond.wait(m_state_mutex);
			}
			m_state.lock_shared();
		}
		void unlock_shared() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			NETP_ASSERT(m_state.has_more_shared());

			m_state.unlock_shared();
			if (!m_state.has_more_shared()) {
				if (m_state.is_upgrade_set()) {
					//notify upgrade thread
					m_upgrade_cond.notify_one();
				} else {
					_notify_waiters();
				}
			}
		}
		void unlock_shared_and_lock() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			NETP_ASSERT(m_state.has_more_shared());
			NETP_ASSERT(m_state.exclu == exclusive::NO);
			m_state.unlock_shared();
			while (m_state.has_more_shared() || m_state.exclu == exclusive::YES ) {
				m_exclusive_cond.wait(m_state_mutex);
			}
			NETP_ASSERT(!m_state.has_more_shared());
			NETP_ASSERT(m_state.exclu == exclusive::NO);
			m_state.exclu = exclusive::YES;
		}

		void lock() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			//upgrade count on has_more_shared
			while (m_state.has_more_shared() || m_state.exclu) {
				m_exclusive_cond.wait(m_state_mutex);
			}
			NETP_ASSERT(!m_state.has_more_shared());
			NETP_ASSERT(m_state.exclu == exclusive::NO);
			m_state.exclu = exclusive::YES;
		}

		void unlock() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			NETP_ASSERT(!m_state.has_more_shared());
			NETP_ASSERT(m_state.exclu == exclusive::YES);

			m_state.exclu = exclusive::NO;
			_notify_waiters();
		}

		void unlock_and_lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			NETP_ASSERT(!m_state.has_more_shared());
			NETP_ASSERT(m_state.exclu == exclusive::YES);

			m_state.exclu = false;
			NETP_ASSERT(m_state.can_lock_shared());
			m_state.lock_shared();
		}

		bool try_lock() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			if (m_state.has_more_shared() || m_state.exclu) {
				return false;
			}

			m_state.exclu = true;
			return true;
		}
		bool try_lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			if (!m_state.can_lock_shared()) {
				return false;
			}
			m_state.lock_shared();
			return true;
		}

		void lock_upgrade() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			while (!m_state.can_lock_upgrade()) {
				m_shared_cond.wait(m_state_mutex);
			}
			m_state.lock_upgrade();
		}

		void unlock_upgrade() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			m_state.unlock_upgrade();
			if (m_state.has_more_shared()) {
				m_shared_cond.notify_all();
			} else {
				_notify_waiters();
			}
		}

		void unlock_upgrade_and_lock() {
			netp::lock_guard<spin_mutex> state_lg(m_state_mutex);
			m_state.unlock_upgrade_and_up();
			while (m_state.has_more_shared() || m_state.exclu == exclusive::YES) {
				m_upgrade_cond.wait(m_state_mutex);
			}

			NETP_ASSERT(!m_state.has_more_shared());
			NETP_ASSERT(m_state.exclu == exclusive::NO);
			m_state.exclu = exclusive::YES;
			m_state.lock_upgrade_up_done();
		}
	};
}//end of ns impl

namespace _mutex_detail {
	class shared_mutex
	{

	public:
		impl::shared_mutex m_impl;
		NETP_DECLARE_NONCOPYABLE(shared_mutex)
			//hint, it's usually be used conjucted with lock for write, lock_shared for read
	public:
		shared_mutex() :
			m_impl()
		{
		}

		~shared_mutex() {
			_MUTEX_DEBUG_CHECK_FOR_LOCK_
		}

		inline void lock_shared() {
			_MUTEX_IMPL_LOCK_SHARED(m_impl);
		}
		inline void unlock_shared() {
			_MUTEX_IMPL_UNLOCK_SHARED(m_impl);
		}
		inline void unlock_shared_and_lock() {
			_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
				m_impl.unlock_shared_and_lock();
			_MUTEX_DEBUG_LOCK_
		}

		inline void lock() {
			_MUTEX_IMPL_LOCK(m_impl);
		}
		inline void unlock() {
			_MUTEX_IMPL_UNLOCK(m_impl);
		}
		inline bool try_lock() {
			_MUTEX_IMPL_TRY_LOCK(m_impl);
		}
		inline bool try_lock_shared() {

			_MUTEX_DEBUG_CHECK_FOR_LOCK_
				bool rt = m_impl.try_lock_shared();
#ifdef _DEBUG_SHARED_MUTEX
			if (rt == true) {
				_MUTEX_DEBUG_LOCK_;
			}
#endif
			return rt;
		}

		inline void lock_upgrade() {
			_MUTEX_DEBUG_CHECK_FOR_LOCK_
				m_impl.lock_upgrade();
			_MUTEX_DEBUG_LOCK_
		}
		inline void unlock_upgrade() {
			_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
				m_impl.unlock_upgrade();
			_MUTEX_DEBUG_UNLOCK_
		}
		inline void unlock_upgrade_and_lock() {
			_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
				m_impl.unlock_upgrade_and_lock();
		}

		inline void unlock_and_lock_shared() {
			_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
				m_impl.unlock_and_lock_shared();
			_MUTEX_DEBUG_LOCK_
		}

		inline impl::shared_mutex* impl() { return (impl::shared_mutex*)&m_impl; }
	};
}}

namespace netp {
#ifdef _NETP_USE_MUTEX_WRAPPER
		typedef netp::_mutex_detail::shared_mutex shared_mutex;
#else
		typedef netp::impl::shared_mutex shared_mutex;
#endif
}
#endif