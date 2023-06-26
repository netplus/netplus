#ifndef _NETP_PROMISE_HPP
#define _NETP_PROMISE_HPP

#include <functional>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

#include <netp/condition.hpp>
#include <netp/mutex.hpp>

namespace netp {

	class promise_exception:
		public exception
	{
		promise_exception( int const& code, std::string const& reason ) :
			exception(code, reason.c_str(), __FILE__, __LINE__, __FUNCTION__)
		{
		}
	};

	template <typename V, int INTERNAL_SLOTS=2>
	struct event_broker_promise 
	{
		typedef std::function<void(V const&)> fn_promise_callee_t;
		fn_promise_callee_t* __callees[INTERNAL_SLOTS];
		fn_promise_callee_t** __callees_dy;
		u32_t __callees_idx:16;
		u32_t __callees_dy_max:16;

		event_broker_promise() :
			__callees_dy(0),
			__callees_idx(0),
			__callees_dy_max(0)
		{
		}

		~event_broker_promise() {
			for (::size_t s = 0; s < __callees_idx; ++s) {
				//free all related mem if there is a callee left here
				//if the promise object missed the set call, we get here 
				if (NETP_LIKELY(s < INTERNAL_SLOTS)) {
					netp::allocator<fn_promise_callee_t>::trash(__callees[s]);
				} else {
					netp::allocator<fn_promise_callee_t>::trash(__callees_dy[s-INTERNAL_SLOTS]);
				}
			}
			netp::allocator<fn_promise_callee_t>::free((fn_promise_callee_t*)(__callees_dy));
			//__callees = 0;
			//__callees_slot_max = 0;
			//__callees_slot_idx = 0;
		}

		template <class fn_callee_t>
		__NETP_FORCE_INLINE void bind(fn_callee_t&& fn_pcallee) {
			fn_promise_callee_t* fnpcallee = netp::allocator<fn_promise_callee_t>::make(std::forward<fn_callee_t>(fn_pcallee));
			if (NETP_LIKELY(__callees_idx < INTERNAL_SLOTS)) {
				__callees[__callees_idx++] = fnpcallee;
			} else {
				if (__callees_idx == (INTERNAL_SLOTS + __callees_dy_max)) {
					__callees_dy_max += INTERNAL_SLOTS;
					__callees_dy = (fn_promise_callee_t**)netp::allocator<fn_promise_callee_t>::realloc((fn_promise_callee_t*)__callees_dy, __callees_dy_max * sizeof(fn_promise_callee_t*));
					NETP_ALLOC_CHECK(__callees_dy, sizeof(fn_promise_callee_t*) * __callees_dy_max);
				}
				__callees_dy[(__callees_idx++) - INTERNAL_SLOTS] = fnpcallee;
			}
		}
		
		__NETP_FORCE_INLINE void invoke( V const& v) {
			for (::size_t s = 0; s< __callees_idx; ++s) {
				if ( NETP_LIKELY(s < INTERNAL_SLOTS)) {
					(*__callees[s])(v);
					netp::allocator<fn_promise_callee_t>::trash(__callees[s]);
				} else {
					(*__callees_dy[s - INTERNAL_SLOTS])(v);
					netp::allocator<fn_promise_callee_t>::trash(__callees_dy[s-INTERNAL_SLOTS]);
				}
			}
			__callees_idx = 0;
		}
	};

	//@note: remove cancel state, cancel operation should a user level logic, 
	enum class promise_state {
		S_IDLE, //wait to operation
		S_UPDATING,
		S_DONE //operation done
	};

	#define __NETP_PROMISE_EBP_INTERNAL_SLOTS (2)
	template <typename V>
	class promise :
		public ref_base,
		protected event_broker_promise<V, __NETP_PROMISE_EBP_INTERNAL_SLOTS>
	{

	private:
		typedef promise<V> promise_t;
		typedef event_broker_promise<V, __NETP_PROMISE_EBP_INTERNAL_SLOTS> event_broker_promise_t;
		typedef typename event_broker_promise_t::fn_promise_callee_t fn_promise_callee_t;

		std::atomic<u8_t> m_state;//memory order constraint var
		V m_v;
		spin_mutex m_mutex;
		condition_variable_any* m_cond;
		int m_waiter;

#define __COND_ALLOCATE_CHECK(cond_var) \
	if(cond_var==0) { \
		cond_var = netp::allocator<condition_variable_any>::make(); \
	} \

#define __COND_DEALLOCATE_CHECK(cond_var) \
		netp::allocator<condition_variable_any>::trash(cond_var); \
		cond_var = 0; \


		//__NETP_FORCE_INLINE void __cond_allocate_check() {
		//	if (m_cond == 0) {
		//		m_cond = netp::allocator<condition_variable_any>::make();
		//	}
		//}
		//__NETP_FORCE_INLINE void __cond_deallocate_check() {
		//	netp::allocator<condition_variable_any>::trash(m_cond);
		//	m_cond = 0;
		//}

	public:
		promise():
			m_state(u8_t(promise_state::S_IDLE)),
			m_v(V()),
			m_cond(0),
			m_waiter(0)
		{}

		~promise() {
			__COND_DEALLOCATE_CHECK(m_cond);
		}

		const __NETP_FORCE_INLINE bool is_idle() const {
			return m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_IDLE);
		}
		const __NETP_FORCE_INLINE bool is_done() const {
			return m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_DONE);
		}

		//wait and wait_for should be public
		//in case: user could call wait_for a duration, then do a query on a var that would set by promise::set opeation if user want to..
		void wait() {
			//we need acquire to place a load barrier
			//refer to set 
			while (!is_done()) {
				lock_guard<spin_mutex> lg(m_mutex);
				if (!is_done()) {
					++promise_t::m_waiter;
					__COND_ALLOCATE_CHECK(m_cond);
					m_cond->wait(m_mutex);
					--promise_t::m_waiter;
				}
			}
		}

		template <class _Rep, class _Period>
		void wait_for(std::chrono::duration<_Rep, _Period>&& dur) {
			if (!is_done()) {
				lock_guard<spin_mutex> lg(m_mutex);
				const std::chrono::time_point< std::chrono::steady_clock> tp_expire = std::chrono::steady_clock::now() + dur;
				while (!is_done()) {
					const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
					if (now >= tp_expire) {
						break;
					}
					++promise_t::m_waiter;
					__COND_ALLOCATE_CHECK(m_cond);
					m_cond->wait_for<_Rep, _Period>(m_mutex, now- tp_expire);
					--promise_t::m_waiter;
				}
			}
		}


		/*
		*  This function is implemented here only for NECESSARY
		* 
		* please do not call this function directly in any thread
		* if you do it, please make sure the following things might nerver happen
			a: the promised value should be signaled from the same thread
			b: thread 1 wait on a promised value which might be signaled from thread 2, and thread 2 might be waiting a promised value from thread1
				this is a very common case in a event loop based architecture

				the solution is to use schedule always
		*/

		__NETP_FORCE_INLINE
		const V& get() {
			wait();
			return m_v;
		}

		template <class _Rep, class _Period>
		__NETP_FORCE_INLINE const V& get(std::chrono::duration<_Rep, _Period>&& dur) {
			wait_for<_Rep, _Period>(std::forward<std::chrono::duration<_Rep, _Period>>(dur));
			return m_v;
		}

		//@note: cancel logic should be implmented in user's business level 
/*
//		const inline bool is_cancelled() const {
//			return m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_CANCELLED);
//		}
		bool cancel() {
			lock_guard<spin_mutex> lg(m_mutex);
			u8_t s = u8_t(promise_state::S_IDLE);
			if ( !m_state.compare_exchange_strong(s, u8_t(promise_state::S_CANCELLED), std::memory_order_acq_rel, std::memory_order_acquire)) {
				return false;
			}

			NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_CANCELLED));
			promise_t::m_waiter >0 ? m_cond->notify_all():(void)0;
			event_broker_promise_t::invoke(V());
			return true;
		}
		*/

		//NOTE: failed to call set might lead to memory leak if there was resource/dependency in callee
		template<class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, fn_promise_callee_t>::value>::type>
		void if_done(_callable&& callee) {
			if (is_done()) {
				//@note: the below line might happen before event_broker_promise_t::invoke(m_v);
				//@note: users should pay attention on the following note
				//@note1: callee's sequence does not make sense if multi-callee do not have relation for each other, so we could have a fast path, the current impl prefer to this fast path
				//@note2: callee's sequence make sense the following case
				//	1) thread 1 call if_done[callee1], before state done
				// 2) thread 1 call if_done[callee2] right after thread 2 call set (update state to done,  but not reach to evt invoke yet)
				//  callee 2 happens before callee 1 in this case
				//fast path
				callee(promise_t::m_v);
			} else {
				lock_guard<spin_mutex> lg(m_mutex);
				//slow path: double check 
				if (is_done()) {
					//@note: the below line might happen before event_broker_promise_t::invoke(m_v);
					callee(promise_t::m_v);
				} else {
					//if we miss a if_done in this place, we must not miss it in promise::set, cuz state store must happen before lock of m_mutex
					event_broker_promise_t::bind(std::bind<void>(std::forward<_callable>(callee), std::placeholders::_1));
				}
			}
		}

		//if promise was destructed during set by accident, we would get a ~mutex(){} assert failed on DEBUG version
		template <class rV/*rV means returned V*/>
		void set(rV&& rv) {
			
			//only one thread, one try succeed
			u8_t s = u8_t(promise_state::S_IDLE);
			if (NETP_UNLIKELY(!promise_t::m_state.compare_exchange_strong(s, u8_t(promise_state::S_UPDATING), std::memory_order_acq_rel, std::memory_order_acquire)) ) {
				NETP_THROW("set failed: DO NOT set twice on a same promise");
			}

			//caution: 
			//(1) if callee is executed on another thread, we need call std::atomic_thread_fence(std::memory_order_acquire) before reading v
			//(2) ofc, we could just call is_done() instead

			//(3) luckly, you do not need to do any synchronization mentioned above, if you schedule/execute callee by event_loop
			//refer to void event_loop::schedule()/__run() for detail

			//alternative impl:
				//we can pass a object such as promise itself, then do get() on callee thread to sure memory synchronization
				//but if we do it like this, we would have an additional memory synchronization if the invoker and callee happen in the same thread

			//In regards to cache coherency issue, for store queue, load queue
			// there is absolution no room to make any further optimization
			// refer to :why_memory_barrier.2010.06.07c.pdf

			//the spin_lock below sure the assign of v happens before invoke && a std::atomic_thread_fence would be forced by unload of the spin_lock
			//if the other thread do std::atomic_thread_fence(std::memory_order_acquire) before reading v , it's safe to get the latest v
			promise_t::m_v = std::forward<rV>(rv);
			//we need a a memory barrier to prevent m_v be reordered 
			//m_v's assign must happen before state update
			//any m_v read operation after a state query which result in S_DONE by option (std::memory_order_acquire) shall have the latest set value
			promise_t::m_state.store(u8_t(promise_state::S_DONE), std::memory_order_release);

			NETP_DEBUG_STACK_SIZE();
			lock_guard<spin_mutex> lg(promise_t::m_mutex);
			event_broker_promise_t::invoke(m_v);
			promise_t::m_waiter>0 ?m_cond->notify_all():(void)0;
		}
	};
}
#endif