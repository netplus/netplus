#ifndef NETP_THREAD_IMPL_MUTEX_BASIC_HPP
#define NETP_THREAD_IMPL_MUTEX_BASIC_HPP

#include <netp/core/config.hpp>

//#define ENABLE_TRACE_MUTEX
#ifdef ENABLE_TRACE_MUTEX
	#include <netp/app.hpp>
	#define TRACE_MUTEX NETP_VERBOSE
#else
	#define TRACE_MUTEX(...)
#endif

#define _NETP_USE_MUTEX_WRAPPER

#if defined(_NETP_DEBUG)
	//#define _DEBUG_MUTEX
	//#define _DEBUG_SHARED_MUTEX
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
#include <unordered_set>
#include <netp/tls.hpp>
namespace netp {
	typedef std::unordered_set<void*> mutex_set_t;
}
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
	#define _MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		do { \
			void*_this = (void*)(this); \
			mutex_set_t* mtxset = tls<mutex_set_t>::get(); \
			mtxset && (NETP_ASSERT(mtxset->find(_this)==mtxset->end()),0); \
		} while (0);
	#define _MUTEX_DEBUG_CHECK_FOR_UNLOCK_ \
		do { \
			void*_this = (void*)(this); \
			mutex_set_t* mtxset = tls<mutex_set_t>::get(); \
			mtxset && (NETP_ASSERT(mtxset->find(_this)!=mtxset->end()),0); \
		} while (0);
	#define _MUTEX_DEBUG_LOCK_ \
		do { \
			void*_this = (void*)(this); \
			mutex_set_t* mtxset = tls<mutex_set_t>::get(); \
			mtxset && (mtxset->insert(_this),0); \
			TRACE_MUTEX("[lock]insert to MutexSet for: %p", _this ); \
		} while (0);

	#define _MUTEX_DEBUG_UNLOCK_ \
		do { \
			void*_this = (void*)(this); \
			mutex_set_t* mtxset = tls<mutex_set_t>::get(); \
			mtxset && (mtxset->erase(_this),0); \
			TRACE_MUTEX("[unlock]erase from MutexSet for: %p", _this ); \
		} while (0);
#else
	#define _MUTEX_DEBUG_CHECK_FOR_LOCK_
	#define _MUTEX_DEBUG_CHECK_FOR_UNLOCK_
	#define _MUTEX_DEBUG_LOCK_
	#define _MUTEX_DEBUG_UNLOCK_
#endif

#define _MUTEX_IMPL_LOCK(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		impl.lock(); \
		_MUTEX_DEBUG_LOCK_ \
	} while (0);
#define _MUTEX_IMPL_UNLOCK(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_UNLOCK_ \
		impl.unlock(); \
		_MUTEX_DEBUG_UNLOCK_ \
	} while (0);

#define _MUTEX_IMPL_LOCK_SHARED(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		impl.lock_shared(); \
		_MUTEX_DEBUG_LOCK_ \
	} while (0);
#define _MUTEX_IMPL_UNLOCK_SHARED(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_UNLOCK_ \
		impl.unlock_shared(); \
		_MUTEX_DEBUG_UNLOCK_ \
	} while (0);

#define _MUTEX_IMPL_TRY_LOCK(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		bool b = impl.try_lock(); \
		if (true==b) { \
			_MUTEX_DEBUG_LOCK_ \
		} \
		return b; \
	} while (0);

namespace netp {

	struct adopt_lock_t {};
	struct defer_lock_t {};
	struct try_to_lock_t {};

	template <class _MutexT>
	class unique_lock final
	{
		NETP_DECLARE_NONCOPYABLE(unique_lock)

		typedef unique_lock<_MutexT> UniqueLockType;
		typedef _MutexT MutexType;

	private:
		_MutexT& m_mtx;
		bool m_own;

	public:
		unique_lock( _MutexT& mutex ):
			m_mtx(mutex),m_own(false)
		{
			m_mtx.lock();
			m_own = true;
		}
		~unique_lock() {
			if( m_own ) {
				m_mtx.unlock();
			}
		}

		unique_lock( _MutexT& mutex, adopt_lock_t ):
			m_mtx(mutex),m_own(true)
		{// construct and assume already locked
		}

		unique_lock( _MutexT& mutex, defer_lock_t ):
			m_mtx(mutex),m_own(false)
		{// construct but don't lock
		}

		unique_lock( _MutexT& mutex, try_to_lock_t ):
			m_mtx(mutex),m_own(m_mtx.try_lock())
		{
		}

		inline void lock() {
			m_mtx.lock();
			m_own = true;
		}

		inline void unlock() {
			m_mtx.unlock();
			m_own = false;
		}

		inline bool try_lock() {
			m_own = m_mtx.try_lock();
			return m_own;
		}

		inline bool own_lock() {
			return m_own;
		}

		inline _MutexT const& mutex() const {
			return m_mtx;
		}

		inline _MutexT& mutex() {
			return m_mtx;
		}
	};

	template <class _MutexT>
	struct lock_guard
	{
		NETP_DECLARE_NONCOPYABLE(lock_guard)
	public:
		lock_guard( _MutexT& mtx ) : m_mtx(mtx) {
			m_mtx.lock();
		}

		~lock_guard() {
			m_mtx.unlock();
		}

		_MutexT& m_mtx;
	};

	template <class _MutexT>
	struct shared_lock_guard final
	{
		NETP_DECLARE_NONCOPYABLE(shared_lock_guard)
	public:
		shared_lock_guard(_MutexT& s_mtx):m_mtx(s_mtx) {
			m_mtx.lock_shared();
		}
		~shared_lock_guard() {
			m_mtx.unlock_shared() ;
 		}
		_MutexT& m_mtx;
	};

	/*
	template <class _MutexT>
	struct shared_upgrade_to_lock_guard
	{
		NETP_DECLARE_NONCOPYABLE(shared_upgrade_to_lock_guard)
	public:
		shared_upgrade_to_lock_guard(_MutexT& s_mtx):m_mtx(s_mtx) {
			m_mtx.unlock_shared_and_lock();
		}
		~shared_upgrade_to_lock_guard() {
			m_mtx.unlock_and_lock_shared();
		}
		_MutexT& m_mtx;
	};
	*/

	template <typename mutex_t>
	struct scoped_lock {
		mutex_t* m;
		bool locked;
		explicit scoped_lock(mutex_t* m_) :
			m(m_), locked(true)
		{
			m->lock();
		}

		inline void unlock() {
			m->unlock();
			locked = false;
		}
		~scoped_lock() {
			if (locked) {
				m->unlock();
			}
		}
	};
}
#endif