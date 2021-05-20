#ifndef _NETP_RPC_HPP
#define _NETP_RPC_HPP

#include <functional>
#include <list>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/event_broker.hpp>
#include <netp/promise.hpp>
#include <netp/channel_handler_context.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel.hpp>
#include <netp/socket.hpp>

namespace netp {
	#define __NETP_RPC_DEFAULT_TIMEOUT std::chrono::seconds(30)

	enum class rpc_message_type {
		T_REQ,
		T_RESP,
		T_DATA
	};

	struct rpc_message final:
		public netp::ref_base
	{
		static std::atomic<netp::u32_t> __rpc_message_id__;
		const int __make_id__() { return __rpc_message_id__.fetch_add(1, std::memory_order_relaxed); }

		rpc_message_type type;
		netp::u32_t id;
		int code;
		NRP<netp::packet> data;

		rpc_message(rpc_message_type type_) :
			type(type_),
			id(0),
			code(0),
			data(nullptr)
		{}

		rpc_message(rpc_message_type type_, netp::u32_t id_) :
			type(type_),
			id(id_),
			code(0),
			data(nullptr)
		{}

		rpc_message(rpc_message_type type_, int code_, NRP<netp::packet> const& data_) :
			type(type_),
			id(__make_id__()),
			code(code_),
			data(data_)
		{}

		rpc_message(rpc_message_type type_, netp::u32_t id_, int code_, NRP<netp::packet> const& data_) :
			type(type_),
			id(id_),
			code(code_),
			data(data_)
		{}

		void encode(NRP<netp::packet>& outp);
		static int from_packet(NRP<netp::packet> const& inpack, NRP<rpc_message>& in_rpcm);
	};

	typedef netp::promise<std::tuple<int, NRP<netp::packet>>> rpc_call_promise;
	typedef netp::promise<int> rpc_push_promise;

	enum class rpc_req_message_state {
		S_WAIT_WRITE,
		S_WRITING,
		S_WRITE_DONE,
		S_WAIT_RESPOND,
		S_RESPOND,
		S_TIMEOUT
	};

	class rpc;
	struct rpc_req_message final :
		netp::ref_base
	{
		rpc_req_message_state state;
		NRP<netp::rpc_message> m;
		NRP<netp::rpc_call_promise> callp;
		NRP<netp::rpc_push_promise> pushp;
		timer_timepoint_t tp_timeout;
	};

	enum rpc_write_state {
		S_WRITE_CLOSED,
		S_WRITE_IDLE,
		S_WRITING
	};

	enum rpc_activity {
		E_RPC_CONNECTED = -2,
		E_RPC_ERROR = -3 //dial error, or other error, notify dial error to rpc_future
	};

	typedef std::function<void(NRP<netp::rpc> const& rpc_)> fn_rpc_activity_notify_t;
	typedef std::function<void(NRP<netp::rpc> const& rpc_, int err)> fn_rpc_activity_notify_error_t;

	typedef std::function<void(NRP<netp::rpc> const& rpc_, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& callp)> fn_rpc_call_t;
	typedef std::function<void(NRP<netp::rpc> const& rpc_, NRP<netp::packet> const& in)> fn_on_push_t;

	class rpc;
	typedef netp::promise<std::tuple<int, NRP<rpc>>> rpc_dial_promise;
	typedef netp::channel_listen_promise rpc_listen_promise;

	class rpc_event_broker_any : 
		protected event_broker_any 
	{
	protected:
		//I do not like virtual here
		void __insert_into_evt_map(int evt_id, evt_node_list* evt_node) {
			event_map_t::iterator&& it = m_handlers.find(evt_id);
			NETP_ASSERT( it == m_handlers.end() || it->second->cnt == 0);

			//make a header first, then insert h into the tail
			evt_node_list* evt_hl = evt_node_allocate_head();//head
			evt_hl->cnt = 1;

			netp::list_init(evt_hl);
			netp::list_append(evt_hl->prev, evt_node);
			m_handlers.insert({ evt_id, evt_hl });
		}
	public:
		template<class _callable_conv_to, class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, _callable_conv_to>::value>::type>
			inline i64_t bind(int evt_id, _callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to, _callable>(std::forward<_callable>(evt_callee));
			evt_node->id |= (i64_t(evt_id) << 32);
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}

		template<class _callable_conv_to, class _Fx, class... _Args>
		inline i64_t bind(int evt_id, _Fx&& _func, _Args&&... _args) {
			evt_node_list* evt_node = evt_node_allocate<_callable_conv_to>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			evt_node->id |= (i64_t(evt_id) << 32);
			__insert_into_evt_map(evt_id, evt_node);
			return evt_node->id;
		}
	};

	class rpc final:
		public netp::channel_handler_abstract,
		private rpc_event_broker_any
	{
		typedef std::deque<NRP<netp::rpc_message>, netp::allocator<NRP<netp::rpc_message>>> rpc_message_reply_queue_t;
		typedef std::list<NRP<netp::rpc_req_message>, netp::allocator<NRP<netp::rpc_req_message>>> rpc_message_req_list_t;
		typedef std::deque<NRP<netp::rpc_req_message>, netp::allocator<NRP<netp::rpc_req_message>>> rpc_message_req_queue_t;

	private:
		NRP<netp::io_event_loop> m_loop;
		rpc_write_state m_wstate;
		fn_on_push_t m_fn_on_push;

		NRP<promise<int>> m_close_promise;
		NRP<netp::channel_handler_context> m_ctx;
		NRP<netp::ref_base> m_rpc_ctx;

		rpc_message_reply_queue_t m_reply_q;

		rpc_message_req_list_t m_wait_respond_list;
		rpc_message_req_list_t m_write_list;

		netp::u32_t m_queue_size;

		void _do_reply(NRP<netp::rpc_message> const& reply);
		void _do_reply_done(int code);
		void _do_write_req_done(int code);

		void _do_flush();

		void _timer_timeout(NRP<netp::timer> const& t);
		void _do_timer_timeout();

		void _do_close(NRP<netp::promise<int>> const& op_future);

		void _do_call(NRP<netp::rpc_call_promise> const& p, int id, NRP<netp::packet> const& data, netp::timer_duration_t const& timeout );
		void _do_push(NRP<netp::rpc_push_promise> const& p, NRP<netp::packet> const& data, netp::timer_duration_t const& timeout);

		void connected(NRP<netp::channel_handler_context> const& ctx);
		void closed(NRP<netp::channel_handler_context> const& ctx);
		void error(NRP<netp::channel_handler_context> const& ctx, int err);
		void read_closed(NRP<netp::channel_handler_context> const& ctx);
		void write_closed(NRP<netp::channel_handler_context> const& ctx);

		void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const &income);

	public:
		rpc(NRP<netp::io_event_loop> const& L);
		~rpc();

		NRP<netp::io_event_loop> const& event_loop() const { return m_loop; }
		NRP<netp::channel> const& channel() const { return m_ctx->ch; }

		template <class ctx_t>
		inline NRP<ctx_t> get_ctx() { return netp::static_pointer_cast<ctx_t>(m_rpc_ctx);}
		inline void set_ctx(NRP<ref_base> const& ctx) { m_rpc_ctx = ctx;}

		template<class _callable
			, class = typename std::enable_if<std::is_convertible<_callable, fn_rpc_call_t>::value>::type>
		inline i64_t bindcall(int id, _callable&& callee) {
			return rpc_event_broker_any::bind<fn_rpc_call_t>(id, std::forward<_callable>(callee));
		}

		template<class _Fx, class... _Args>
		inline i64_t bindcall(int id, _Fx&& _func, _Args&&... _args) {
			return rpc_event_broker_any::bind<fn_rpc_call_t>(id, std::forward<_Fx>(_func), std::forward<_Args>(_args)...);
		}

		inline void unbindcall(int id) {
			rpc_event_broker_any::unbind(id);
		}

		void on_push(fn_on_push_t const& fn);
		void operator >> (fn_on_push_t const& fn) ;

		NRP<netp::promise<int>> set_queue_size( netp::u32_t s ) {
			NETP_ASSERT(m_loop != nullptr);
			NRP<netp::promise<int>> rf = netp::make_ref<netp::promise<int>>();
			m_loop->execute([rpc_=NRP<rpc>(this), s,rf](){
				rpc_->m_queue_size = s;
				rf->set(netp::OK);
			});

			return rf;
		}

		template <class dur = std::chrono::seconds>
		void call(NRP<netp::rpc_call_promise> const& callp, int id, NRP<netp::packet> const& data, dur const& timeout = __NETP_RPC_DEFAULT_TIMEOUT) {
			m_loop->execute([rpc = NRP<rpc>(this), id, data, callp, timeout]() {
				rpc->_do_call(callp, id, data, timeout);
			});
		}

		template <class dur=std::chrono::seconds>
		NRP<netp::rpc_call_promise> call(int id, NRP<netp::packet> const& data, dur const& timeout = __NETP_RPC_DEFAULT_TIMEOUT) {
			NRP<rpc_call_promise> callp = netp::make_ref<rpc_call_promise>();
			m_loop->execute([rpc = NRP<rpc>(this), callp, id, data, timeout]() {
				rpc->_do_call(callp, id, data, timeout);
			});
			return callp;
		}

		template <class dur = std::chrono::seconds>
		NRP<rpc_call_promise> operator()(int api_id, NRP<netp::packet> const& data, dur const& timeout = __NETP_RPC_DEFAULT_TIMEOUT) {
			return call(api_id, data,timeout);
		}

		template <class dur = std::chrono::seconds>
		void do_push(NRP<rpc_push_promise> const& pushp, NRP<netp::packet> const& data, dur const& timeout = __NETP_RPC_DEFAULT_TIMEOUT) {
			m_loop->execute([R = NRP<netp::rpc>(this), data, pushp, timeout](){
				R->_do_push(pushp,data, timeout);
			});
		}

		template <class dur=std::chrono::seconds>
		NRP<netp::rpc_push_promise> push(NRP<netp::packet> const& data, dur const& timeout = __NETP_RPC_DEFAULT_TIMEOUT ) {
			NRP<rpc_push_promise> pushp = netp::make_ref<rpc_push_promise>();
			m_loop->execute([R = NRP<netp::rpc>(this), data, pushp, timeout](){
				R->_do_push(pushp, data, timeout);
			});
			return pushp;
		}

		//@NOTE: operator << can only have a single parameter 
		NRP<rpc_push_promise> operator<< (NRP<netp::packet> const& data ) {
			return push(data, __NETP_RPC_DEFAULT_TIMEOUT);
		}

		NRP<netp::promise<int>> close_promise() const {
			return m_close_promise;
		}
		NRP<netp::promise<int>> close();

		static netp::fn_channel_initializer_t __decorate_initializer(fn_rpc_activity_notify_t const& fn_notify_connected, fn_rpc_activity_notify_error_t const& fn_notify_err, netp::fn_channel_initializer_t const& fn);
		static NRP<rpc_dial_promise> dial(const char* host, size_t len, netp::fn_channel_initializer_t const& fn_ch_initializer, NRP<socket_cfg> const& cfg = netp::make_ref<socket_cfg>());
		static NRP<rpc_dial_promise> dial(std::string const& host, netp::fn_channel_initializer_t const& fn_ch_initializer, NRP<socket_cfg> const& cfg = netp::make_ref<socket_cfg>() );

		static NRP<rpc_dial_promise> dial(std::string const& host) {
			return rpc::dial(host, nullptr);
		}

		static NRP<rpc_listen_promise> listen(std::string const& host, fn_rpc_activity_notify_t const& fn_accepted, netp::fn_channel_initializer_t const& fn_ch_initializer, NRP<socket_cfg> const& cfg);

		static NRP<rpc_listen_promise> listen(std::string const& host, fn_rpc_activity_notify_t const& fn_accepted, netp::fn_channel_initializer_t const& fn_ch_initializer) {
			return rpc::listen(host, fn_accepted, fn_ch_initializer, netp::make_ref<socket_cfg>());
		}
		static NRP<rpc_listen_promise> listen(std::string const& host, fn_rpc_activity_notify_t const& fn_accepted) {
			return rpc::listen(host, fn_accepted,nullptr, netp::make_ref<socket_cfg>());
		}
		static NRP<rpc_listen_promise> listen(std::string const& host) {
			return rpc::listen(host, nullptr, nullptr, netp::make_ref<socket_cfg>());
		}
	};
}
#endif