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

	template <typename V>
	struct event_broker_promise 
	{
		typedef std::function<void(V const&)> fn_promise_callee_t;
		typedef std::vector<fn_promise_callee_t,netp::allocator<fn_promise_callee_t>> fn_promise_callee_vector_t;
		fn_promise_callee_vector_t __callees;
		event_broker_promise() :__callees() {}

		inline void bind(fn_promise_callee_t&& callee) {
			__callees.emplace_back(std::forward<fn_promise_callee_t>(callee));
		}

		inline void bind(fn_promise_callee_t const& callee) {
			__callees.emplace_back(callee);
		}
		
		inline void invoke( V const& v) {
			for (::size_t i = 0, s=__callees.size() ; i <s; ++i) {
				__callees[i](v);
			}
			__callees.clear();
		}
	};
	enum class promise_state {
		S_IDLE, //wait to operation
		S_UPDATING,
		S_CANCELLED, //operation cancelled
		S_DONE //operation done
	};

	template <typename V>
	class promise :
		public ref_base,
		protected event_broker_promise<V>
	{

	private:
		typedef promise<V> promise_t;
		typedef event_broker_promise<V> event_broker_promise_t;
		typedef typename event_broker_promise<V>::fn_promise_callee_t fn_promise_callee_t;

		std::atomic<u8_t> m_state;//memory order constraint var
		V m_v;
		spin_mutex m_mutex;
		condition_variable_any* m_cond;
		int m_waiter;

		__NETP_FORCE_INLINE void __cond_allocate_check() {
			if (m_cond == 0) {
				m_cond = netp::allocator<condition_variable_any>::make();
			}
		}

		__NETP_FORCE_INLINE void __cond_deallocate_check() {
			netp::allocator<condition_variable_any>::trash(m_cond);
			m_cond = 0;
		}

	public:
		promise():
			m_state(u8_t(promise_state::S_IDLE)),
			m_v(V()),
			m_cond(0),
			m_waiter(0)
		{}

		~promise() {
			__cond_deallocate_check();
		}

		inline const V& get() {
			wait();
			return m_v;
		}

		inline void memory_sync() {
			std::atomic_thread_fence(std::memory_order_acquire);
		}

		template <class _Rep, class _Period>
		inline const V& get(std::chrono::duration<_Rep, _Period>&& dur) {
			wait_for<_Rep,_Period>(std::forward<std::chrono::duration<_Rep, _Period>>(dur));
			return m_v;
		}

		void wait() {
			while (m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_IDLE)) {
				lock_guard<spin_mutex> lg(m_mutex);
				if (m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_IDLE) ) {
					++promise_t::m_waiter;
					__cond_allocate_check();
					m_cond->wait(m_mutex);
					--promise_t::m_waiter;
				}
			}
		}

		template <class _Rep, class _Period>
		void wait_for(std::chrono::duration<_Rep, _Period>&& dur) {
			if (m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_IDLE)) {
				lock_guard<spin_mutex> lg(m_mutex);
				const std::chrono::time_point< std::chrono::steady_clock> tp_expire = std::chrono::steady_clock::now() + dur;
				while (m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_IDLE )) {
					const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
					if (now >= tp_expire) {
						break;
					}
					++promise_t::m_waiter;
					__cond_allocate_check();
					m_cond->wait_for<_Rep, _Period>(m_mutex, now- tp_expire);
					--promise_t::m_waiter;
				}
			}
		}
		const inline bool is_idle() const {
			return m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_IDLE);
		}
		const inline bool is_done() const {
			return m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_DONE);
		}
		const inline bool is_cancelled() const {
			return m_state.load(std::memory_order_acquire) == u8_t(promise_state::S_CANCELLED);
		}

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

		//NOTE: failed to call set might lead to memory leak if there was resource/dependency in callee
		template<class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, fn_promise_callee_t>::value>::type>
		void if_done(_callable&& callee) {
			lock_guard<spin_mutex> lg(m_mutex);
			event_broker_promise_t::bind(std::bind(std::forward<_callable>(callee), std::placeholders::_1));
			if (is_done()) {
				event_broker_promise_t::invoke(promise_t::m_v);
			}
		}

		template<class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, fn_promise_callee_t>::value>::type>
		void if_done(_callable const& callee) {
			lock_guard<spin_mutex> lg(m_mutex);
			event_broker_promise_t::bind(std::bind(std::forward<_callable>(callee), std::placeholders::_1));
			if (is_done()) {
				event_broker_promise_t::invoke(promise_t::m_v);
			}
		}

		//if future was destructed during set by accident, we would get a ~mutex(){} assert failed on DEBUG version
		void set(V const& v) {
			
			//only one thread, one try succeed
			u8_t s = u8_t(promise_state::S_IDLE);
			if (NETP_UNLIKELY(!promise_t::m_state.compare_exchange_strong(s, u8_t(promise_state::S_UPDATING), std::memory_order_acq_rel, std::memory_order_acquire)) ) {
				NETP_THROW("set failed: DO NOT set twice on a same promise");
			}

			//caution: 
			//(1) if callee is executed on another thread, we need call std::atomic_thread_fence(std::memory_order_acquire) before reading v
			//(2) ofc, we could just call is_done() instead

			//(3) luckly, you do not need to do any synchronization mentioned above, if you schedule/execute callee by io_event_loop
			//refer to void io_event_loop::schedule()/__run() for detail

			//alternative impl:
				//we can pass a object such as promise itself, then do get() on callee thread to sure memory synchronization
				//but if we do it like this, we would have an additional memory synchronization if the invoker and callee happen in the same thread

			//In regards to cache coherency issue, for store queue, load queue
			// there is absolution no room to make any further optimization
			// refer to :why_memory_barrier.2010.06.07c.pdf

			//the spin_lock below sure the assign of v happens before invoke && a std::atomic_thread_fence would be forced by unload of the spin_lock
			//if the other thread do std::atomic_thread_fence(std::memory_order_acquire) before reading v , it's safe to get the latest v
			promise_t::m_v = v;
			promise_t::m_state.store(u8_t(promise_state::S_DONE), std::memory_order_release);

			NETP_DEBUG_STACK_SIZE();
			lock_guard<spin_mutex> lg(promise_t::m_mutex);
			event_broker_promise_t::invoke(m_v);
			promise_t::m_waiter >0 ?m_cond->notify_all():(void)0;
		}

		void set(V&& v) {
			u8_t s = u8_t(promise_state::S_IDLE);
			if (NETP_UNLIKELY(!promise_t::m_state.compare_exchange_strong(s, u8_t(promise_state::S_UPDATING), std::memory_order_acq_rel, std::memory_order_acquire))) {
				NETP_THROW("set failed: DO NOT set twice on a same promise");
			}
			promise_t::m_v = v;
			promise_t::m_state.store(u8_t(promise_state::S_DONE), std::memory_order_release);

			NETP_DEBUG_STACK_SIZE();
			lock_guard<spin_mutex> lg(promise_t::m_mutex);
			event_broker_promise_t::invoke(m_v);
			promise_t::m_waiter > 0 ? m_cond->notify_all() : (void)0;
		}
	};
}
#endif