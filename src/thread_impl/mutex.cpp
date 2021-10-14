#include <netp/core.hpp>
#include <netp/mutex.hpp>
#include <netp/thread.hpp>

namespace netp {

	//const adopt_lock_t	adopt_lock;
	//const defer_lock_t		defer_lock;
	//const try_to_lock_t	try_to_lock;

	namespace impl {

		atomic_spin_mutex::atomic_spin_mutex()
		{
#ifdef _NETP_CHECK_ATOMIC_BOOL
			std::atomic_bool ab;
			bool is_lock_free = ab.is_lock_free();
			std::atomic<bool> abb;
			bool is_lock_free_b = abb.is_lock_free();

			int sizeof_atomic_bool = sizeof( std::atomic_bool );
			int sizeof_atomic_bool_ins = sizeof( ab );

			int sizeof_atomic_bool_my_bool = sizeof( std::atomic<bool> );
			int sizeof_atomic_bool_my_bool_ins = sizeof( abb );
#endif

		}
		atomic_spin_mutex::~atomic_spin_mutex() {}

		void atomic_spin_mutex::lock() {
			int k = 0;
			while(!try_lock()) {
				++k;
				netp::this_thread::no_interrupt_yield(NETP_MIN((k>>1),60));
			}
		}

		void atomic_spin_mutex::unlock() {
			m_flag.clear(std::memory_order_release);
		}

		bool atomic_spin_mutex::try_lock() {
			//it is said that :This function generates a full memory barrier (or fence) to ensure that memory operations are completed in order.
			//refer to: https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedexchange
			return !m_flag.test_and_set(std::memory_order_acq_rel);
		}

	} //endif impl
}