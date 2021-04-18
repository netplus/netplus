#ifndef _NETP_EVENT_TRIGGER_HPP
#define _NETP_EVENT_TRIGGER_HPP

#include <unordered_map>
#include <netp/list.hpp>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

namespace netp {
	extern std::atomic<i32_t> s_event_handler_id;

	struct callee_exception :
		public netp::exception
	{
		callee_exception(int code) :
			exception(code, "callee_exception", __FILE__, __LINE__, __FUNCTION__)
		{
		}
	};

	//union evt_id {
	//	struct _id_{
	//		int evt_id;
	//		int handler_id;
	//	}ID_INTERNAL;
	//	i64_t ID;
	//};

	class event_handler_base_any :
		public netp::non_atomic_ref_base
	{
		friend class event_broker_any;
	protected:
		//H:32 would be updated when insert happens
		//H:32 -> evt_id
		//L:32 -> handler_id
		i64_t id;
	public:
		event_handler_base_any() :
			id(i64_t(netp::atomic_incre(&s_event_handler_id, std::memory_order_relaxed)))
		{}
		virtual void* address_of_callee() const = 0;
	};


	class event_broker_any
	{
		template <class _callable>
		class event_handler_any : public event_handler_base_any
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

		enum evt_node_flag {
			f_invoking = 1<<0,
			f_insert_pending=1<<1,
			f_delete_pending=1<<2
		};

		//(1) by this data struct, we support embeded bind/unbind/invoking
		//(2) should we enable a feature of invoking(evt_id=3) inner an invoking (evt_id=3), does it make sense to support this ?
		//for now, we just fail with a throw, if we happens to occur on the case (2)
		struct evt_handler_list {
			evt_handler_list* prev, *next;
			int flag;
			NRP<event_handler_base_any> ehb;
		};

		inline static evt_handler_list* evt_hl_allocate( int flag, NRP<event_handler_base_any> const& ehb_ ) {
			evt_handler_list* evt_hl = netp::allocator<evt_handler_list>::make();
			evt_hl->flag = flag;
			evt_hl->ehb = ehb_;
			return evt_hl;
		}

		inline static void evt_hl_deallocate(evt_handler_list* evt_hl) {
			evt_hl->ehb = nullptr;
			netp::allocator<evt_handler_list>::trash(evt_hl);
		}
		
		typedef std::unordered_map<int, evt_handler_list*, id_hash, id_equal, netp::allocator<std::pair<const int, evt_handler_list*>>> event_map_t;
		event_map_t m_handlers;

		void __insert_into_evt_map(int evt_id, NRP<event_handler_base_any>&& handler) {
			//(1) always insert into tail
			//(2) if we are in invoking stage, we flag with in_invoking,
			//(3) the flag in_invoking would be cleared once invoking done
			event_map_t::iterator&& it = m_handlers.find(evt_id); 
			if (it != m_handlers.end()) {
				const evt_handler_list* evt_hl = it->second;
				netp::list_append(evt_hl->prev, evt_hl_allocate( (evt_hl->flag & evt_node_flag::f_invoking) ? evt_node_flag::f_insert_pending: 0, handler) );
			} else {
				//make a header first, then insert h into the tail
				evt_handler_list* evt_hl = evt_hl_allocate(0, 0);//head
				netp::list_init(evt_hl);
				netp::list_append(evt_hl->prev, evt_hl_allocate(0, handler));
				m_handlers.insert({ evt_id, evt_hl });
			}
		}

	public:
		event_broker_any() :
			m_handlers()
		{}

		virtual ~event_broker_any() {
			//dealloc list header
			event_map_t::iterator&& it = m_handlers.begin();
			while (it != m_handlers.end()) {
				evt_handler_list* evt_hl = it->second;
				evt_handler_list* cur, * nxt;
				NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
					netp::list_delete(cur);
					evt_hl_deallocate(evt_hl);
				}
				event_map_t::iterator it_cur = it++;
				m_handlers.erase(it_cur);
			}
		}

		inline void unbind(int evt_id) {
			if (m_handlers.size() == 0) {
				return;
			}
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end() ) {
				return;
			}

			evt_handler_list* evt_hl = it->second;
			evt_handler_list* cur, *nxt;
			
			//@WARN can not just flag, then delete it in the nxt invoke, if no invoke happens, we might got memory leak if the broker and event handler has ABBA relation
			NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
				if (evt_hl->flag & evt_node_flag::f_invoking) {
					cur->flag |= evt_node_flag::f_delete_pending;
				} else {
					netp::list_delete(cur);
					evt_hl_deallocate(cur);
				}
			}
		}

		inline void unbind_by_handle_id(i64_t handler_id) {
			if (m_handlers.size() == 0) {
				return;
			}

			int evt_id = int(handler_id >> 32);
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return;
			}

			evt_handler_list* evt_hl = it->second;
			evt_handler_list* cur, * nxt;

			NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
				if (cur->ehb->id != handler_id) {
					continue;
				}

				if (evt_hl->flag & evt_node_flag::f_invoking) {
					cur->flag |= evt_node_flag::f_delete_pending;
				} else {
					netp::list_delete(cur);
					evt_hl_deallocate(cur);
				}
				return;
			}
		}

		inline void unbind(i64_t handler_id) {
			return unbind_by_handle_id(handler_id);
		}
		
		/*
		inline void unbind_by_handle_id(i64_t handler_id) {
			event_map_t::iterator&& it = m_handlers.begin();
			while (it != m_handlers.end()) {
				evt_handler_list* evt_hl = it->second;
				evt_handler_list* cur, * nxt;
				NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
					if (cur->ehb->id != handler_id) {
						continue;
					}
					if (evt_hl->flag & evt_node_flag::f_invoking) {
						cur->flag |= evt_node_flag::f_delete_pending;
					} else {
						evt_hl_deallocate(cur);
					}
					return;
				}
				++it;
			}
		}
		*/

		template<class _callable>
		inline i64_t bind(int evt_id, _callable&& callee ) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			NRP<event_handler_base_any> evt_handler = make_event_handler_any<typename std::decay<_callable>::type,_callable>(std::forward<_callable>(callee));
			evt_handler->id |= (i64_t(evt_id) << 32);
			i64_t hid = evt_handler->id;
			__insert_into_evt_map(evt_id, std::move(evt_handler));
			return hid;
		}

		template<class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
		inline i64_t bind(int evt_id, _callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			NRP<event_handler_base_any> evt_handler = make_event_handler_any<_callable_conv_to,_callable>(std::forward<_callable>(evt_callee));
			evt_handler->id |= (i64_t(evt_id) << 32);
			i64_t hid = evt_handler->id;
			__insert_into_evt_map(evt_id, std::move(evt_handler));
			return hid;
		}

		template<class _callable_conv_to, class _Fx, class... _Args>
		inline i64_t bind(int evt_id, _Fx&& _func, _Args&&... _args) {
			NRP<event_handler_base_any> evt_handler = make_event_handler_any<_callable_conv_to>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			evt_handler->id |= (i64_t(evt_id) << 32);
			i64_t hid = evt_handler->id;
			__insert_into_evt_map(evt_id, std::move(evt_handler));
			return hid;
		}

		//do not invoke call inner a invoking stage
		template<class _callable_conv_to, class... _Args>
		void invoke(int evt_id, _Args&&... _args) {
			typename event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return;
			}

			 //impl consideration:
			 // 
			 //(1) bind/unbind for another evt_id in context of this invoking should have no side effect
			 //(2) bind for current evt_id should not be invoked in this invoking (would be invoked from the next invoking)

			 //(1) if we call bind/unbind for another evt_id in conext of invoking, no side effect
			 //(2) if we call bind for current evt_id in context of invoiking
			 //		(a) if the new callee's pos after cur invoking node, and if the nxt assignment happens before the new inserting && nxt == tail, no side effect
			 //		(b) if the new callee's pos after cur invoking node, and if the nxt assignment happens before the new inserting && nxt != tail, we have to skip this new inserting for this invoking
			 //(3) if we call unbind for current evt_id or (evt_id, handler_id) in context of invoiking, just disable all related nodes

			 //(4) if we call invoking (evt_id=xx) in a context of invokding (evt_id=xx), we fail with a throw

			//(5) bind/unbind has no thread safe gurantee


			//head->flag works as a invoking/insert/delete barrier 
			evt_handler_list* const ev_hl = it->second;
			if (ev_hl->flag & evt_node_flag::f_invoking) {
				NETP_THROW("embeded invoking forbidden");
			}

			ev_hl->flag |= evt_node_flag::f_invoking;
			evt_handler_list* cur, * nxt;
			 NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
				 if (cur->flag & (evt_node_flag::f_insert_pending|evt_node_flag::f_delete_pending)) {
					 continue;
				 } else {
					 (*((_callable_conv_to*)(cur->ehb->address_of_callee())))(std::forward<_Args>(_args)...);
				 }
			 }
			 
			 if (ev_hl->flag & (evt_node_flag::f_insert_pending|evt_node_flag::f_delete_pending)) {
				 //full scan for deallocateing
				 NETP_LIST_SAFE_FOR(cur,nxt, ev_hl) {
					 if (cur->flag & evt_node_flag::f_insert_pending) {
						 cur->flag &= ~evt_node_flag::f_insert_pending;
					 } else if (cur->flag & evt_node_flag::f_delete_pending) {
						 netp::list_delete(cur);
						 evt_hl_deallocate(cur);
					 } else {}
				 }
			 }
			 ev_hl->flag &= ~(evt_node_flag::f_invoking| evt_node_flag::f_insert_pending);
		}

#ifdef __NETP_DEBUG_BROKER_INVOKER_
		template<class _callable_conv_to, class... _Args>
		int dynamic_invoke(int evt_id, _Args&&... _args) {
			typename event_map_t::const_iterator&& it = m_handlers.find(evt_id);
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
		int static_invoke(int evt_id, _Args&&... _args) {
			typename event_map_t::const_iterator&& it = m_handlers.find(evt_id);
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
		auto invoke_first(int evt_id, _Args&&... _args)
			-> decltype( std::declval<typename std::remove_reference<_callable_conv_to>::type>()(std::forward<_Args>(_args)...))
		{
			typename event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				throw netp::callee_exception(netp::E_EVENT_BROKER_NO_LISTENER);
			}
			evt_handler_list* ev_hl = it->second;
			if (NETP_LIST_IS_EMPTY(ev_hl)) {
				throw netp::callee_exception(netp::E_EVENT_BROKER_NO_LISTENER);
			}
			return (*((_callable_conv_to*)(ev_hl->next->ehb->address_of_callee()) ))(std::forward<_Args>(_args)...);
		}
	};
}
#endif