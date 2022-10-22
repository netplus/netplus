#ifndef NETP_THREAD_IMPL_SHARD_MUTEX_HPP
#define NETP_THREAD_IMPL_SHARD_MUTEX_HPP

#include <netp/core.hpp>
#include <netp/condition.hpp>
#include <netp/thread_impl/mutex_basic.hpp>

namespace netp { namespace impl {
		
	class shared_mutex final
	{
		NETP_DECLARE_NONCOPYABLE(shared_mutex)

		//the idea borrowed from boost
		//Note, the upgradeable lock is not exclusive (other shared locks can be held) just that it has special privileges to increase its strength.
		//Unfortunately, multiple upgradeable threads are not allowed at the same time.
		struct state_data {
			int shared_count;
			bool exclusive;
			bool upgrade;
			bool exclusive_waiting_blocked;

			state_data() :
				shared_count(0),
				exclusive(false),
				upgrade(false),
				exclusive_waiting_blocked(false)
			{}

			inline void assert_free() const
			{
				NETP_ASSERT(!exclusive);
				NETP_ASSERT(!upgrade);
				NETP_ASSERT(shared_count == 0);
			}

			inline void assert_locked() const
			{
				NETP_ASSERT(exclusive);
				NETP_ASSERT(shared_count == 0);
				NETP_ASSERT(!upgrade);
			}

			inline void assert_lock_shared() const
			{//lock_shared & lock_upgraded could happen at the same time
				NETP_ASSERT(!exclusive);
				NETP_ASSERT(shared_count > 0);
			}

			inline void assert_lock_upgraded() const
			{
				NETP_ASSERT(!exclusive);
				NETP_ASSERT(upgrade);
				NETP_ASSERT(shared_count > 0);
			}

			inline void assert_lock_not_upgraded() const
			{
				NETP_ASSERT(!upgrade);
			}

			inline bool can_lock() const {
				return !(shared_count || exclusive);
			}
			inline void lock() {
				exclusive = true;
			}
			inline void unlock() {
				exclusive = false;
				exclusive_waiting_blocked = false;
			}
			inline bool can_lock_shared() const {
				return !(exclusive || exclusive_waiting_blocked);
			}
			inline bool more_shared() const {
				return shared_count > 0;
			}
			inline bool no_shared() const
			{
				return shared_count == 0;
			}
			inline bool one_shared() const
			{
				return shared_count == 1;
			}
			inline void lock_shared() {
				++shared_count;
			}
			inline void unlock_shared() {
				--shared_count;
			}
			inline bool can_lock_upgrade() const {
				return !(exclusive || exclusive_waiting_blocked || upgrade);
			}
			inline void lock_upgrade() {
				++shared_count;
				upgrade = true;
			}
			inline void unlock_upgrade() {
				upgrade = false;
				--shared_count;
			}
		};

		//shared <->  lock
		//upgrade <-> lock
		state_data state;
		spin_mutex state_mutex;
		netp::condition_any shared_cond;
		netp::condition_any exclusive_cond;
		netp::condition_any upgrade_cond;

		inline void release_waiters() {
			exclusive_cond.notify_one();
			shared_cond.notify_all();
		}

	public:
		shared_mutex() :
			state(),
			state_mutex(),
			shared_cond(),
			exclusive_cond(),
			upgrade_cond()
		{}
		~shared_mutex() {}

		void lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			while (!state.can_lock_shared()) {
				shared_cond.wait(state_mutex);
			}
			state.lock_shared();
		}

		bool try_lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			if (!state.can_lock_shared()) {
				return false;
			}
			state.lock_shared();
			return true;
		}
		void unlock_shared() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_lock_shared();

			state.unlock_shared();
			if (state.no_shared()) {
				if (state.upgrade) {
					//if we get here , there must have a thread waiting for upgrade_cond
					//notify upgrade thread
					//state.upgrade = false;
					// 
					//act as a lock barrier, no more shared, upgraded, or lock shall be acquired
					state.exclusive = true; 
					upgrade_cond.notify_one();
				} else {
					state.exclusive_waiting_blocked = false;
					release_waiters();
				}
			}
		}

		void unlock_shared_and_lock() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_lock_shared();
			state.unlock_shared();
			while ( !state.can_lock() ) {
				state.exclusive_waiting_blocked = true;
				exclusive_cond.wait(state_mutex);
			}
			state.exclusive = true;
		}

		void lock() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			//upgrade count on has_more_shared
			while ( !state.can_lock() ) {
				state.exclusive_waiting_blocked = true;
				exclusive_cond.wait(state_mutex);
			}
			state.exclusive = true;
		}

		bool try_lock() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			if (!state.can_lock()) {
				return false;
			}
			state.exclusive = true;
			return true;
		}

		void unlock() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_locked();
			state.exclusive = false;
			state.exclusive_waiting_blocked = false;
			state.assert_free();
			release_waiters();
		}

		// Shared <-> Exclusive
		void unlock_and_lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_locked();
			state.exclusive = false;
			state.lock_shared();
			state.exclusive_waiting_blocked = false;
			release_waiters();
		}

		void lock_upgrade() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			while (!state.can_lock_upgrade()) {
				shared_cond.wait(state_mutex);
			}
			state.lock_upgrade();
		}

		bool try_lock_upgrade() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			if (!state.can_lock_upgrade()) {
				return false;
			}

			state.lock_upgrade();
			state.assert_lock_upgraded();
			return true;
		}

		void unlock_upgrade() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.unlock_upgrade();
			if (state.no_shared()) {
				state.exclusive_waiting_blocked = false;
				release_waiters();
			} else {
				shared_cond.notify_all();
			}
		}

		// Upgrade <-> Exclusive
		void unlock_upgrade_and_lock() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_lock_upgraded();
			state.unlock_shared();

			//if we have shared, no exclusive lock would succeed, 
			//when no_shared match, upgrade_cond have the highest priority
			while (!state.no_shared() ) {
				upgrade_cond.wait(state_mutex);
			}
			NETP_ASSERT( state.upgrade == true );
			state.exclusive = true;
			state.upgrade = false;
			state.assert_locked();
		}

		// lock_upgrade -> lock_shared
		void unlock_upgrade_and_lock_shared() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_lock_upgraded();
			state.upgrade = false;
			state.exclusive_waiting_blocked = false;
			release_waiters();
		}

		void unlock_and_lock_upgrade() {
			netp::lock_guard<spin_mutex> state_lg(state_mutex);
			state.assert_locked();
			state.exclusive = false;
			state.lock_upgrade();
			state.exclusive_waiting_blocked = false;
			release_waiters(); 
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

		//inline void unlock_shared_and_lock() {
		//	_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
		//		m_impl.unlock_shared_and_lock();
		//	_MUTEX_DEBUG_LOCK_
		//}

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
		inline bool try_lock_upgrade() {
			return m_impl.try_lock_upgrade();
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