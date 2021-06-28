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

	struct evt_node_list 
	{
		evt_node_list *prev, *next;
		int flag:8;
		int cnt : 24;
		//H:32 would be updated when insert happens
		//H:32 -> evt_id
		//L:32 -> handler_id
		i64_t id;
		evt_node_list() :
			prev(0),
			next(0),
			flag(0),
			id(i32_t(s_event_handler_id.fetch_add(1, std::memory_order_relaxed)))
		{}
		virtual ~evt_node_list() {}
		virtual void* address_of_callee() const { return 0; };
		virtual void destroy(void*) {};
	};

	enum evt_node_flag {
		f_invoking = 1 << 0,
		f_insert_pending = 1 << 1,
		f_delete_pending = 1 << 2
	};

	template <class _callable>
	class event_handler_any : public evt_node_list
	{
		friend class event_broker_any;
		typedef event_handler_any<_callable> _this_type_t;
		typedef typename std::decay<_callable>::type __callee_type;
		__callee_type __callee;//this member must be aligned to 8bytes: arm32

		void* address_of_callee() const { return (void*)&__callee; }

		void destroy(void* node) {
			NETP_ASSERT(((_this_type_t*)node) == this);
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
		//no placement delete for virtual dtor impl..., just hack it with virutal func
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

	typedef std::unordered_map<int, evt_node_list*, id_hash, id_equal, netp::allocator<std::pair<const int, evt_node_list*>>> event_map_t;

	class event_broker_any
	{
	protected:
		event_map_t m_handlers;
		void __insert_into_evt_map(int evt_id, evt_node_list* evt_node) {
			//(1) always insert into tail
			//(2) if we are in invoking stage, we flag with in_invoking,
			//(3) the flag in_invoking would be cleared once invoking done
			event_map_t::iterator&& it = m_handlers.find(evt_id); 
			if (it != m_handlers.end()) {
				evt_node_list* evt_hl = it->second;
				if(evt_hl->flag & evt_node_flag::f_invoking) {
					evt_node->flag |= evt_node_flag::f_insert_pending;
					//told invoker that we have pending insert
					evt_hl->flag |= evt_node_flag::f_insert_pending;
				}
				++evt_hl->cnt;
				netp::list_append(evt_hl->prev, evt_node );
			} else {
				//make a header first, then insert h into the tail
				evt_node_list* evt_hl = evt_node_allocate_head();//head
				evt_hl->cnt = 1;

				netp::list_init(evt_hl);
				netp::list_append(evt_hl->prev, evt_node);
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
				evt_node_list* evt_hl = it->second;
				evt_node_list* cur, *nxt;
				NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
					NETP_ASSERT( (cur->flag&(evt_node_flag::f_insert_pending|evt_node_flag::f_delete_pending)) == 0);
					NETP_ASSERT(evt_hl->cnt >0 );
					--evt_hl->cnt;
					netp::list_delete(cur);
					evt_node_deallocate(cur);
				}
				NETP_ASSERT( evt_hl->cnt == 0 );
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
			
			//@WARN can not just do flag option
			//then wish it to be deleted in the nxt invoke, if no invoke happens, we might got memory leak if the broker and event handler has ABBA relation
			NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
				if (evt_hl->flag & evt_node_flag::f_invoking) {
					cur->flag |= evt_node_flag::f_delete_pending;
					evt_hl->flag |= evt_node_flag::f_delete_pending;
				} else {
					NETP_ASSERT(evt_hl->cnt > 0);
					--evt_hl->cnt;
					netp::list_delete(cur);
					evt_node_deallocate(cur);
				}
			}
		}

		//only delete the specific node that exactly equal to handler_id
		inline void unbind_by_handle_id(i64_t handler_id) {
			if (m_handlers.size() == 0) {
				return;
			}

			int evt_id = int(handler_id >> 32);
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			if (it == m_handlers.end()) {
				return;
			}

			evt_node_list* evt_hl = it->second;
			evt_node_list* cur, *nxt;

			NETP_LIST_SAFE_FOR(cur, nxt, evt_hl) {
				if (cur->id != handler_id) {
					continue;
				}

				if (evt_hl->flag & evt_node_flag::f_invoking) {
					cur->flag |= evt_node_flag::f_delete_pending;
					evt_hl->flag |= evt_node_flag::f_delete_pending;
				} else {
					NETP_ASSERT(evt_hl->cnt > 0);
					--evt_hl->cnt;
					netp::list_delete(cur);
					evt_node_deallocate(cur);
				}
				return;
			}
		}

		template<class _callable>
		inline i64_t bind(int evt_id, _callable&& callee ) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<typename std::decay<_callable>::type,_callable>(std::forward<_callable>(callee));
			evt_node->id |= (i64_t(evt_id) << 32);
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
		inline i64_t bind(int evt_id, _callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to,_callable>(std::forward<_callable>(evt_callee));
			evt_node->id |= (i64_t(evt_id) << 32);
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable_conv_to, class _Fx, class... _Args>
		inline i64_t bind(int evt_id, _Fx&& _func, _Args&&... _args) {
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			evt_node->id |= (i64_t(evt_id) << 32);
			__insert_into_evt_map(evt_id, evt_node );
			return evt_node->id;
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
			evt_node_list* ev_hl = it->second;
			if (ev_hl->flag & evt_node_flag::f_invoking) {
				NETP_THROW("embeded invoking forbidden");
			}

			ev_hl->flag |= evt_node_flag::f_invoking;
			evt_node_list* cur, *nxt;
			 NETP_LIST_SAFE_FOR(cur, nxt, ev_hl) {
				 if (cur->flag & (evt_node_flag::f_insert_pending|evt_node_flag::f_delete_pending)) {
					 continue;
				 } else {
					 (*((_callable_conv_to*)(cur->address_of_callee())))(std::forward<_Args>(_args)...);
				 }
			 }
			 
			 //full scan for deallocating
			 if (ev_hl->flag & (evt_node_flag::f_insert_pending|evt_node_flag::f_delete_pending)) {
				 NETP_LIST_SAFE_FOR(cur,nxt, ev_hl) {
					 if (cur->flag & evt_node_flag::f_insert_pending) {
						 cur->flag &= ~evt_node_flag::f_insert_pending;
					 } else if (cur->flag & evt_node_flag::f_delete_pending) {
						 NETP_ASSERT(ev_hl->cnt > 0);
						 --ev_hl->cnt;

						 netp::list_delete(cur);
						 evt_node_deallocate(cur);
					 } else {}
				 }
			 }
			 ev_hl->flag &= ~(evt_node_flag::f_invoking| evt_node_flag::f_insert_pending|evt_node_flag::f_delete_pending);
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
	};
}
#endif