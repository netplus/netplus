#ifndef _NETP_EVENT_TRIGGER_HPP
#define _NETP_EVENT_TRIGGER_HPP

#include <unordered_map>
#include <vector>
#include <algorithm>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

namespace netp {
	extern std::atomic<long> s_event_handler_id;

	struct callee_exception :
		public netp::exception
	{
		callee_exception(int code) :
			exception(code, "callee_exception", __FILE__, __LINE__, __FUNCTION__)
		{
		}
	};

	class event_broker_any
	{
		class event_handler_any_base :
			public netp::ref_base
		{
			friend class event_broker_any;
		protected:
			long id;
		public:
			event_handler_any_base() :
				id(netp::atomic_incre(&s_event_handler_id))
			{}

			virtual void* address_of_callee() const = 0;
		};

		template <class _callable>
		class event_handler_any : public event_handler_any_base
		{

			friend class event_broker_any;

			template <class __NETP_T__, typename... _Args>
			friend netp::ref_ptr<__NETP_T__> netp::make_ref(_Args&&... args);

			typedef typename std::remove_reference<_callable>::type __callee_type;
			__callee_type __callee;

			void* address_of_callee() const { return (void*)&__callee; }

			template<class _Callable_from
				, class = typename std::enable_if<std::is_convertible<_Callable_from, __callee_type>::value>::type>
			event_handler_any(_Callable_from&& callee) :
				__callee(std::forward<_Callable_from>(callee))
			{}

#ifdef __NETP_DEBUG_BROKER_INVOKER_
			template<class... Args>
			inline auto call(Args&&... args)
				-> decltype(__callee(std::forward<Args>(args)...))
			{
				return __callee(std::forward<Args>(args)...);
			}
#endif
		};

		template <class _callable>
		inline NRP<event_handler_any<typename std::decay<_callable>::type>> make_event_handler_any(_callable&& _func) {
			return netp::make_ref<event_handler_any<typename std::decay<_callable>::type>,_callable>(std::forward<_callable>(_func));
		}

		template <class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
		inline NRP<event_handler_any<_callable_conv_to>> make_event_handler_any(_callable&& _func) {
			return netp::make_ref<event_handler_any<_callable_conv_to>, _callable>(std::forward<_callable>(_func));
		}

		struct id_hash {
			inline std::size_t operator()(int k) const
			{
				return k;
			}
		};

		struct id_equal {
			inline bool operator()(int lhs, int rhs) const
			{
				return lhs == rhs;
			}
		};

		typedef std::vector<NRP<event_handler_any_base>,netp::allocator<NRP<event_handler_any_base>>> handler_vector_t;
		typedef std::unordered_map<int, handler_vector_t, id_hash, id_equal, netp::allocator<std::pair<const int, handler_vector_t>>> event_map_t;
		//typedef std::unordered_map<int, handler_vector_t, id_hash, id_equal> event_map_t;

		#define __INSERT_INTO_EVT_MAP(id,H) \
				event_map_t::iterator&& __it = m_handlers.find(id); \
				if (__it != m_handlers.end()) { \
					__it->second.emplace_back(std::move(H)); \
				} else { \
					m_handlers.insert({ id, handler_vector_t{std::move(H)} }); \
				}\


		event_map_t m_handlers;
	public:
		event_broker_any() :
			m_handlers() {}
		virtual ~event_broker_any() {}

		inline void unbind(int evt_id) {
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end() || m_handlers.size() == 0) {
				return;
			}
			it->second.clear();
		}

		inline void unbind(int evt_id, long handler_id) {
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end() || it->second.size() == 0) {
				return;
			}

			handler_vector_t::iterator&& it_handler = std::find_if(it->second.begin(), it->second.end(), [handler_id](NRP<event_handler_any_base> const& H) {
				return handler_id == H->id;
			});
			if (it_handler != it->second.end()) {
				it->second.erase(it_handler);
			}
		}

		inline void unbind_by_handle_id(long handler_id) {
			event_map_t::iterator&& it = m_handlers.begin();
			while (it != m_handlers.end()) {
				handler_vector_t::iterator&& it_handler = std::find_if(it->second.begin(), it->second.end(), [handler_id]( NRP<event_handler_any_base> const& H) {
					return handler_id ==H->id;
				});
				if (it_handler != it->second.end()) {
					it->second.erase(it_handler);
					return;
				}
				++it;
			}
		}

		template<class _callable>
		inline long bind(int id, _callable&& callee ) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			NRP<event_handler_any_base> evt_handler = make_event_handler_any<typename std::decay<_callable>::type,_callable>(std::forward<_callable>(callee));
			long hid = evt_handler->id;
			__INSERT_INTO_EVT_MAP(id, evt_handler);
			return hid;
		}

		template<class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
		inline long bind(int id, _callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			NRP<event_handler_any_base> evt_handler = make_event_handler_any<_callable_conv_to,_callable>(std::forward<_callable>(evt_callee));
			long hid = evt_handler->id;
			__INSERT_INTO_EVT_MAP(id, evt_handler);
			return hid;
		}

		template<class _callable_conv_to, class _Fx, class... _Args>
		inline long bind(int id, _Fx&& _func, _Args&&... _args) {
			NRP<event_handler_any_base> evt_handler = make_event_handler_any<_callable_conv_to>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			long hid = evt_handler->id;
			__INSERT_INTO_EVT_MAP(id, evt_handler);
			return hid;
		}

		//do not invoke call inner a invoking stage
		template<class _callable_conv_to, class... _Args>
		int invoke(int id, _Args&&... _args) {
			typename event_map_t::iterator&& it = m_handlers.find(id);
			if (it == m_handlers.end()) {
				return netp::E_EVENT_BROKER_NO_LISTENER;
			}
			handler_vector_t::iterator&& it_handler = it->second.begin();
			while (it_handler != it->second.end()) {
				(*((_callable_conv_to*)((*(it_handler++))->address_of_callee())))(std::forward<_Args>(_args)...);
			}
			return netp::OK;
		}

#ifdef __NETP_DEBUG_BROKER_INVOKER_

		template<class _callable_conv_to, class... _Args>
		int dynamic_invoke(int id, _Args&&... _args) {
			typename event_map_t::const_iterator&& it = m_handlers.find(id);
			if (it == m_handlers.end()) {
				return netp::E_EVENT_BROKER_EVENT_NO_LISTENER;
			}
			handler_vector_t::const_iterator&& it_handler = it->second.begin();
			while (it_handler != it->second.end()) {
				//NOTE: implict convertible do not mean castable
				NRP<event_handler_any<_callable_conv_to>> callee = netp::dynamic_pointer_cast<event_handler_any<_callable_conv_to>>( (*(it_handler++)) );
				NETP_ASSERT(callee != nullptr);
				callee->call(std::forward<_Args>(_args)...);
			}
			return netp::OK;
		}

		template<class _callable_conv_to, class... _Args>
		int static_invoke(int id, _Args&&... _args) {
			typename event_map_t::const_iterator&& it = m_handlers.find(id);
			if (it == m_handlers.end()) {
				return netp::E_EVENT_BROKER_EVENT_NO_LISTENER;
			}
			handler_vector_t::const_iterator&& it_handler = it->second.begin();
			while (it_handler != it->second.end()) {
				NRP<event_handler_any<_callable_conv_to>> callee = netp::static_pointer_cast<event_handler_any<_callable_conv_to>>( (*(it_handler++)) );
				callee->call(std::forward<_Args>(_args)...);
			}
			return netp::OK;
		}

		//this type of impl has a little performance advantage on wn10
		template<class _callable_conv_to, class... _Args>
		int virtual_read_callee_address_invoke(int id, _Args&&... _args) {
			typename event_map_t::const_iterator&& it = m_handlers.find(id);
			if (it == m_handlers.end()) {
				return netp::E_EVENT_BROKER_EVENT_NO_LISTENER;
			}
			handler_vector_t::const_iterator&& it_handler = it->second.begin();
			while (it_handler != it->second.end()) {
				(*((_callable_conv_to*)( (*(it_handler++))->address_of_callee())))(std::forward<_Args>(_args)...);
			}
			return netp::OK;
		}
#endif

		template<class _callable_conv_to, class... _Args>
		auto invoke_first(int id, _Args&&... _args)
			-> decltype( std::declval<typename std::remove_reference<_callable_conv_to>::type>()(std::forward<_Args>(_args)...))
		{
			typename event_map_t::iterator&& it = m_handlers.find(id);
			if (it == m_handlers.end()) {
				throw netp::callee_exception(netp::E_EVENT_BROKER_NO_LISTENER);
			}
			if (NETP_LIKELY(it->second.size())) {
				NETP_ASSERT(it->second.size() == 1);
				return (*((_callable_conv_to*)(it->second[0])->address_of_callee()))(std::forward<_Args>(_args)...);
			}
			throw netp::callee_exception(netp::E_EVENT_BROKER_NO_LISTENER);
		}
	};
}
#endif
