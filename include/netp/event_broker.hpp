#ifndef _NETP_EVENT_TRIGGER_HPP
#define _NETP_EVENT_TRIGGER_HPP

#include <unordered_map>
#include <netp/core.hpp>
#include <netp/list.hpp>
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

	struct nested_invoke_limit_exception :
		public netp::exception
	{
		nested_invoke_limit_exception() :
			exception(netp::E_NESTED_INVOKING_REACH_LIMIT, "callee_exception", __FILE__, __LINE__, __FUNCTION__)
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
	using event_handle_id_t = i64_t;

	struct evt_node_list 
	{
		evt_node_list *prev, *next;
		//H:32 -> evt_id
		//L:32 -> evt_handler_id
		event_handle_id_t id;
		u16_t invoking_nest_level;
		u16_t flag;
		long ref_cnt;
		//H:32 would be updated when insert happens
		evt_node_list() :
			prev(0),
			next(0),
			id(i32_t(s_event_handler_id.fetch_add(1, std::memory_order_relaxed))),
			invoking_nest_level(0),
			flag(0)
		{}
		virtual ~evt_node_list() {}
		virtual void* address_of_callee() const { return 0; };
		virtual void destroy(void*) {};
	};
	
	enum evt_node_flag {
		f_insert_pending = 1<<0,
		f_oneshot = 1<<1
	};

#ifdef _NETP_DEBUG
	#define NETP_EVT_BROKER_ASSERT NETP_ASSERT
#else
	#define NETP_EVT_BROKER_ASSERT(...)
#endif

	template <class _callable>
	class event_handler_any : public evt_node_list
	{
		friend class event_broker_any;
		typedef event_handler_any<_callable> _this_type_t;
		typedef typename std::decay<_callable>::type __callee_type;
		
		__callee_type __callee;//this member must be aligned to 8bytes: arm32
		static_assert((sizeof(evt_node_list) % 8) == 0, "check alignof of evt_node_list");

		void* address_of_callee() const { return (void*)&__callee; }

		void destroy(void* node) {
			NETP_EVT_BROKER_ASSERT(((_this_type_t*)node) == this);
			netp::allocator<_this_type_t>::trash((_this_type_t*)node);
		}
#ifdef __NETP_DEBUG_BROKER_INVOKER_
		template<class... Args>
		inline auto call(Args&&... args)
			-> decltype(__callee(std::forward<Args>(args)...))
		{
			return __callee(std::forward<Args>(args)...);
		}
#endif

	public:
		template<class _Callable_from
			, class = typename std::enable_if<std::is_convertible<_Callable_from, __callee_type>::value>::type>
			event_handler_any(_Callable_from&& callee) :
			evt_node_list(),
			__callee(std::forward<_Callable_from>(callee))
		{}
	};


	inline evt_node_list* evt_node_allocate_head() {
		return netp::allocator<evt_node_list>::make();
	}
	inline void evt_node_deallocate_head(evt_node_list* node) {
		return netp::allocator<evt_node_list>::trash(node);
	}

	//it's save to cast to parent class
	template <class _callable>
	inline evt_node_list* evt_node_allocate(_callable&& _func) {
		typedef event_handler_any<typename std::decay<_callable>::type> evt_handler_any_type_t;
		return netp::allocator<evt_handler_any_type_t>::make(std::forward<_callable>(_func));
	}

	template <class _callable_conv_to, class _callable
		, class = typename std::enable_if<std::is_convertible<_callable, typename std::decay<_callable_conv_to>::type>::value>::type>
		inline evt_node_list* evt_node_allocate(_callable&& _func) {
		typedef event_handler_any<typename std::decay<_callable_conv_to>::type> evt_handler_any_type_t;
		return netp::allocator<evt_handler_any_type_t>::make(std::forward<_callable>(_func));
	}

	inline void evt_node_deallocate(evt_node_list* node) {
		//no placement delete for virtual dtor impl..., just hack it with virtual func
		node->destroy(node);
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

	#define NETP_NESTED_INVOKING_LEVEL_LIMIT 0x1f

	typedef std::unordered_map<int, evt_node_list*, id_hash, id_equal, netp::allocator<std::pair<const int, evt_node_list*>>> event_map_t;
	class event_broker_any
	{
		//note: no thread safe
	protected:
		event_map_t m_handlers;
		void __insert_into_evt_map(int evt_id, evt_node_list* evt_node) {
			//(1) always insert into tail
			//(2) if we are in invoking stage, we flag with in_invoking,
			//(3) the flag in_invoking would be cleared once invoking done
			evt_node->ref_cnt = 1;
			std::pair<event_map_t::iterator, bool> inserted = m_handlers.insert({evt_id, nullptr});
			if( inserted.second == false ) {
				//already has a entry
				evt_node_list* evt_hl = inserted.first->second;
				if(evt_hl->invoking_nest_level>0) {
					evt_node->flag |= evt_node_flag::f_insert_pending;
					//flag invoker that we have pending insert
					evt_hl->flag |= evt_node_flag::f_insert_pending;
				}
				netp::list_append(evt_hl->prev, evt_node );
			} else {
				//make a header first, then insert h into the tail
				evt_node_list* evt_hl = evt_node_allocate_head();//head
				evt_hl->ref_cnt = 1;

				netp::list_init(evt_hl);
				netp::list_append(evt_hl->prev, evt_node);
				inserted.first->second = evt_hl;
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
				evt_node_list* evt_hl = it->second;
				evt_node_list* cur, *nxt;
				NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
					NETP_EVT_BROKER_ASSERT((cur->flag & (evt_node_flag::f_insert_pending)) == 0);
					NETP_EVT_BROKER_ASSERT(evt_hl->ref_cnt == 1 );
					netp::list_delete(cur);
					evt_node_deallocate(cur);
				}

				NETP_EVT_BROKER_ASSERT( evt_hl->ref_cnt == 1 );
				evt_node_deallocate_head(evt_hl);
				event_map_t::iterator it_cur = it++;
				m_handlers.erase(it_cur);
			}
		}

		//delete all node that equal to evt_id
		inline void unbind(int evt_id) {
			if (m_handlers.size() == 0) {
				return;
			}
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end() ) {
				return;
			}

			evt_node_list* evt_hl = it->second;
			evt_node_list* cur, *nxt;
			
			NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
				netp::list_delete(cur);
				if ( (--(cur->ref_cnt)) == 0) {
					NETP_EVT_BROKER_ASSERT(evt_hl->invoking_nest_level==0);
					evt_node_deallocate(cur);
				}
			}
		}

		//only delete the specific node that exactly equal to handler_id
		inline void unbind_by_handle_id(event_handle_id_t handler_id) {
			if (m_handlers.size() == 0) {
				return;
			}

			int evt_id = int(handler_id>>32);
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return;
			}

			evt_node_list* evt_hl = it->second;
			evt_node_list* cur, *nxt;

			NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
				if (cur->id == handler_id) {
					netp::list_delete(cur);
					if ((--(cur->ref_cnt)) == 0) {
						NETP_EVT_BROKER_ASSERT(evt_hl->invoking_nest_level == 0);
						evt_node_deallocate(cur);
					}
					return;
				}
			}
		}

		//@WARN:
		//a lambda is not a std::function, a lambda is just unnamed callable object
		//if we bind a lambda without specifiec template _callable type, and do invoke<_callable> then, WE GOT CRASH

		template<class _callable>
		inline event_handle_id_t bind(int evt_id, _callable&& callee ) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<typename std::decay<_callable>::type,_callable>(std::forward<_callable>(callee));
			evt_node->id |= (event_handle_id_t(evt_id)<<32);
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable>
		inline event_handle_id_t bind_oneshot(int evt_id, _callable&& callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<typename std::decay<_callable>::type, _callable>(std::forward<_callable>(callee));
			evt_node->id |= (event_handle_id_t(evt_id)<<32);
			evt_node->flag |= evt_node_flag::f_oneshot;
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
		inline event_handle_id_t bind(int evt_id, _callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to,_callable>(std::forward<_callable>(evt_callee));
			evt_node->id |= (event_handle_id_t(evt_id)<<32);
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
			inline event_handle_id_t bind_oneshot(int evt_id, _callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to, _callable>(std::forward<_callable>(evt_callee));
			evt_node->id |= (event_handle_id_t(evt_id) << 32);
			evt_node->flag |= evt_node_flag::f_oneshot;
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable_conv_to, class _Fx, class... _Args>
		inline event_handle_id_t bind(int evt_id, _Fx&& _func, _Args&&... _args) {
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			evt_node->id |= (event_handle_id_t(evt_id)<<32);
			__insert_into_evt_map(evt_id, evt_node );
			return evt_node->id;
		}

		template<class _callable_conv_to, class _Fx, class... _Args>
		inline event_handle_id_t bind_oneshot(int evt_id, _Fx&& _func, _Args&&... _args) {
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			evt_node->id |= (event_handle_id_t(evt_id) << 32);
			evt_node->flag |= evt_node_flag::f_oneshot;
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		//@WARN: DO NOT invoking_a nested in a invoking_a
					 //impl consideration:
			 //(1) if we call bind/unbind for another evt_id in context of this invoking, no side effect
			 //(2) if we call bind for current evt_id in context of this invoking
			 //		(a) new node inserted from tail, nxt == tail, no side effect
			 //		(b) new node inserted from tail, nxt != tail, we have to skip this new inserting for this invoking
			 //(3) if we call unbind for current evt_id or (evt_id, handler_id) in context of this invoking, just disable all related nodes

			 //(4) if we call invoking (evt_id=xx) in a context of this invoking (evt_id=xx), we fail with a throw if the nested level exceed NETP_NESTED_INVOKING_LEVEL_LIMIT

			//(5) bind/unbind has no thread safe gurantee

			//edge case:
			// 1: 
			// invoke [evt_id_a] {call (1)}
			//		a. do bind [evt_id_a] in call (1), (append new handler {h2})
			//		b. invoke [evt_id_a] in {call (1)} (nested call) {call (2)}
			// should we enable h2 in call (2) ? NO for the current impl
			// 

			// 2: 
			// invoke [evt_id_a] {call (1)}
			//		a. do bind [evt_id_a] in call (1), (append new handler {h2})
			//		b. do unbind [evt_id_a] in call (1), (remove all handlers)
			//		c. invoke [evt_id_a] in {call (1)} (nested call) {call (2)}
			// should we enable h2 in call (2) ? NO for the current impl
			// 
		template<class _callable_conv_to, class... _Args>
		void invoke(int evt_id, _Args&&... _args) {
			typename event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return;
			}
			//head->flag works as a invoking/insert/delete barrier to simplify insert/delete operation of the list
			evt_node_list* ev_hl = it->second;
			if (ev_hl->invoking_nest_level == u8_t(NETP_NESTED_INVOKING_LEVEL_LIMIT) ) {
				//@WARN
				//THIS KIND OF EXCEPTION SHOULD ALWAYS BE TREATED AS UNRECOVERED ERROR
				//IN THIS CASE, THE PROGRAMMER HAVE TO RE-ORGANIZE THE BUSYNESS LOGIC RELATED TO THIS EVT_ID				
				//ANALOGY TO stackoverflow/go PANIC
				throw netp::nested_invoke_limit_exception();
			}

			//@NOTE: stop compiler optimization (++ --) == 0
			//should we use WRITE_ONCE for invoking_nest_level ?

			++(ev_hl->invoking_nest_level);
			//ev_hl->flag |= evt_node_flag::f_invoking;
			evt_node_list* cur, *nxt;
			 NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
				 if ((cur->flag&(evt_node_flag::f_insert_pending/*skip this invoking*/)) == 0 ) {
					if (cur->flag&evt_node_flag::f_oneshot) {
						--(cur->ref_cnt);
						netp::list_delete(cur);
					}
					//prevent cur from being deleted by invoking
					++(cur->ref_cnt);
					try {
						(*((_callable_conv_to*)(cur->address_of_callee())))(std::forward<_Args>(_args)...);
					} catch (...) {
						if ((--(cur->ref_cnt)) == 0) {
							evt_node_deallocate(cur);
						}
						--(ev_hl->invoking_nest_level);
						throw;
					}

					if ( (--(cur->ref_cnt)) == 0) {
						evt_node_deallocate(cur);
					}
				 }
			 }
			 --(ev_hl->invoking_nest_level);
			 if (ev_hl->flag&(evt_node_flag::f_insert_pending)) {
				 NETP_LIST_SAFE_FOR(cur,nxt, ev_hl) {
					if (cur->flag & evt_node_flag::f_insert_pending) {
						 cur->flag &= ~evt_node_flag::f_insert_pending;
					 }
				 }
				 ev_hl->flag &= ~(evt_node_flag::f_insert_pending);
			 }
		}

#ifdef __NETP_DEBUG_BROKER_INVOKER_
		template<class _callable_conv_to, class... _Args>
		void invoke_without_nest_limit(int evt_id, _Args&&... _args) _NETP_NOEXCEPT {
			typename event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return;
			}
			evt_node_list* ev_hl = it->second;
			//head->flag works as a invoking/insert/delete barrier to simplify insert/delete operation of the list
			//@NOTE: stop compiler optimization (++ --) == 0
			//should we use WRITE_ONCE for invoking_nest_level ?
			++(ev_hl->invoking_nest_level);
			//ev_hl->flag |= evt_node_flag::f_invoking;
			evt_node_list* cur, * nxt;
			NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
				if ((cur->flag & (evt_node_flag::f_insert_pending/*skip this invoking*/)) == 0) {
					if (cur->flag & evt_node_flag::f_oneshot) {
						--(cur->ref_cnt);
						netp::list_delete(cur);
					}
					//prevent cur from being deleted by invoking
					++(cur->ref_cnt);
					(*((_callable_conv_to*)(cur->address_of_callee())))(std::forward<_Args>(_args)...);
					if ((--(cur->ref_cnt)) == 0) {
						evt_node_deallocate(cur);
					}
				}
			 }
			--(ev_hl->invoking_nest_level);
			if (ev_hl->flag & (evt_node_flag::f_insert_pending)) {
				NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
					if (cur->flag & evt_node_flag::f_insert_pending) {
						cur->flag &= ~evt_node_flag::f_insert_pending;
					}
				}
				ev_hl->flag &= ~(evt_node_flag::f_insert_pending);
			}
		}

		template<class _callable_conv_to, class... _Args>
		int dynamic_invoke(int evt_id, _Args&&... _args) {
			typename event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return netp::E_EVENT_BROKER_NO_CALLEE;
			}
			evt_node_list* ev_hl = it->second;
			++(ev_hl->invoking_nest_level);
			evt_node_list *cur, *nxt;
			NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
				//NOTE: implict convertible do not mean castable
				if (cur->flag & evt_node_flag::f_oneshot) {
					--(cur->ref_cnt);
					netp::list_delete(cur);
				}
				event_handler_any<_callable_conv_to>* callee = dynamic_cast<event_handler_any<_callable_conv_to>*>(cur);
				NETP_ASSERT(callee != nullptr);
				++(cur->ref_cnt);
				callee->call(std::forward<_Args>(_args)...);
				if ((--(cur->ref_cnt)) == 0) {
					evt_node_deallocate(cur);
				}
			}
			--(ev_hl->invoking_nest_level);
			if (ev_hl->flag & (evt_node_flag::f_insert_pending)) {
				NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
					if (cur->flag & evt_node_flag::f_insert_pending) {
						cur->flag &= ~evt_node_flag::f_insert_pending;
					}
				}
				ev_hl->flag &= ~(evt_node_flag::f_insert_pending);
			}
			return netp::OK;
		}

		template<class _callable_conv_to, class... _Args>
		int static_invoke(int evt_id, _Args&&... _args) {
			typename event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return netp::E_EVENT_BROKER_NO_CALLEE;
			}
			evt_node_list* ev_hl = it->second;
			++(ev_hl->invoking_nest_level);
			evt_node_list *cur, *nxt;
			NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
				//NOTE: implict convertible do not mean castable
				if (cur->flag & evt_node_flag::f_oneshot) {
					--(cur->ref_cnt);
					netp::list_delete(cur);
				}

				event_handler_any<_callable_conv_to>* callee = static_cast<event_handler_any<_callable_conv_to>*>(cur);
				NETP_ASSERT(callee != nullptr);
				++(cur->ref_cnt);
				callee->call(std::forward<_Args>(_args)...);
				if ((--(cur->ref_cnt)) == 0) {
					evt_node_deallocate(cur);
				}
			}
			--(ev_hl->invoking_nest_level);
			if (ev_hl->flag & (evt_node_flag::f_insert_pending)) {
				NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
					if (cur->flag & evt_node_flag::f_insert_pending) {
						cur->flag &= ~evt_node_flag::f_insert_pending;
					}
				}
				ev_hl->flag &= ~(evt_node_flag::f_insert_pending);
			}
			return netp::OK;
		}
#endif
	};
}
#endif