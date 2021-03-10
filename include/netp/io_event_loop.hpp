#ifndef _NETP_IO_EVENT_LOOP_HPP
#define _NETP_IO_EVENT_LOOP_HPP

#include <vector>
#include <unordered_map>
#include <set>

#include <netp/singleton.hpp>
#include <netp/mutex.hpp>
#include <netp/thread.hpp>

#include <netp/io_event.hpp>
#include <netp/timer.hpp>

#include <netp/promise.hpp>
#include <netp/packet.hpp>

#if defined(NETP_IO_POLLER_EPOLL)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_EPOLL
#elif defined(NETP_IO_POLLER_SELECT)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_SELECT
#elif defined(NETP_IO_POLLER_KQUEUE)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_KQUEUE
#elif defined(NETP_IO_POLLER_IOCP)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_IOCP
#else
	#error "unknown poller type"
#endif

namespace netp {

	enum io_poller_type {
		T_SELECT, //win&linux&android
		T_IOCP, //win
		T_EPOLL, //linux,epoll,et
		T_KQUEUE,//bsd
		T_POLLER_CUSTOM_1,
		T_POLLER_CUSTOM_2,
		T_POLLER_MAX,
		T_BYE,
		T_NONE
	};

	class io_event_loop;
	struct poller_cfg {
		u32_t ch_buf_size;
		u32_t maxiumctx;
	};
	typedef std::function< NRP<io_event_loop>(io_poller_type t, poller_cfg const& cfg) > fn_poller_maker_t;

	enum aio_flag {
		AIO_NOTIFY = 0,
		AIO_READ	=1,
		AIO_WRITE	= 1<<1,
		AIO_FLAG_MAX =3
	};

	enum class aio_action {
		READ = 1<<0, //check read, sys io
		END_READ=1<<1,

		WRITE = 1<<2, //check write, sys io
		END_WRITE =1<<3,

		BEGIN =1<<4,
		NOTIFY_TERMINATING=1<<5,
		END =1<<6,

		BEGIN_READ_WRITE = (BEGIN | READ | WRITE)
	};

#ifdef NETP_ENABLE_IOCP
	enum class iocp_action {
		READ,
		END_READ,
		WRITE,
		END_WRITE,
		ACCEPT,
		END_ACCEPT,
		CONNECT,
		END_CONNECT,
		BEGIN,
		END
	};
#endif

#ifdef _DEBUG
	#define NETP_DEBUG_TERMINATING
#endif

	struct watch_ctx :
		public netp::ref_base
	{
		SOCKET fd;
		fn_aio_event_t iofn[aio_flag::AIO_FLAG_MAX];//notify,read,write
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
		u8_t flag;
#endif

#ifdef NETP_DEBUG_TERMINATING
		bool terminated;
#endif
	};

	typedef std::unordered_map<SOCKET, NRP<watch_ctx>,std::hash<SOCKET>, std::equal_to<SOCKET>, netp::allocator<std::pair<const SOCKET, NRP<watch_ctx>>>> watch_ctx_map_t;

	typedef std::pair<SOCKET, NRP<watch_ctx>> watch_ctx_map_pair_t;
	typedef std::function<void()> fn_io_event_task_t;
	typedef std::vector<fn_io_event_task_t, netp::allocator<fn_io_event_task_t>> io_task_q_t;

	class io_event_loop :
		public ref_base
	{
		enum LOOP_WAIT_FLAG {
			F_LOOP_WAIT_NONE_ZERO_TIME_WAIT =1,
			F_LOOP_WAIT_ENTER_WAITING = 1<<1
		};
		friend class io_event_loop_group;
		enum class loop_state {
			S_IDLE,
			S_LAUNCHING,
			S_RUNNING,
			S_TERMINATING, //no more watch evt
			S_TERMINATED, //no more new timer, we need this state to make sure all channel have a chance to check terminating flag
			S_EXIT //exit..
		};

		struct act_op {
			aio_action act;
			SOCKET fd;
			fn_aio_event_t fn;
		};
		typedef std::vector<act_op, netp::allocator<act_op>> act_queue_t;

	protected:
		std::thread::id m_tid;
		act_queue_t m_acts;
		watch_ctx_map_t m_ctxs;

		spin_mutex m_tq_mutex;
		io_task_q_t m_tq_standby;
		io_task_q_t m_tq;
		NRP<timer_broker> m_tb;

		u8_t m_type;
		std::atomic<bool> m_waiting;
		std::atomic<u8_t> m_state;

		SOCKET m_signalfds[2];
		NRP<netp::packet> m_channel_rcv_buf;
		NRP<netp::thread> m_th;

		//timer_timepoint_t m_wait_until;
		std::atomic<u16_t> m_internal_ref_count;
		poller_cfg m_cfg;

#ifdef NETP_DEBUG_TERMINATING
		bool m_terminated;
#endif
	protected:
		inline u16_t internal_ref_count() { return m_internal_ref_count.load(std::memory_order_acquire); }
		inline void __internal_ref_count_inc() { netp::atomic_incre(&m_internal_ref_count); }
		//0,	NO WAIT
		//~0,	INFINITE WAIT
		//>0,	WAIT nanosecond
		__NETP_FORCE_INLINE long long _calc_wait_dur_in_nano() {

			NETP_ASSERT( m_waiting.load(std::memory_order_acquire) == false );
			netp::timer_duration_t ndelay;
			m_tb->expire(ndelay);
			long long ndelayns = ndelay.count();
			if (ndelayns == 0 || m_acts.size() != 0 ) {
				return 0;
			}

			lock_guard<spin_mutex> lg(m_tq_mutex);
			if (m_tq_standby.size() != 0) {
				return 0;
			}
			NETP_ASSERT( u64_t(TIMER_TIME_INFINITE) > NETP_POLLER_WAIT_IGNORE_DUR);
			if ( (u64_t(ndelayns)>NETP_POLLER_WAIT_IGNORE_DUR) ) {
				m_waiting.store(true, std::memory_order_release);
			}
			return ndelayns;
		}

		virtual void init() {
			m_channel_rcv_buf = netp::make_ref<netp::packet>(m_cfg.ch_buf_size);
			m_tid = std::this_thread::get_id();
			m_tb = netp::make_ref<timer_broker>();
			_do_poller_init();

#ifdef NETP_DEBUG_TERMINATING
			m_terminated = false;
#endif
		}

		virtual void deinit() {
			NETP_ASSERT(in_event_loop());
			NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_EXIT));

			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				NETP_ASSERT(m_tq_standby.empty());
			}

			NETP_ASSERT(m_acts.size() == 0);
			NETP_ASSERT(m_tq.empty());
			NETP_ASSERT(m_tb->size() == 0);
			m_tb = nullptr;
			_do_poller_deinit();
			NETP_ASSERT(m_ctxs.size() == 0);
		}

		inline void __do_execute_act() {
			std::size_t vecs = m_acts.size();
			if (vecs>0) {
				std::size_t acti = 0;
				while (acti < vecs) {
					act_op& actop = m_acts[acti++];
					//m_acts.pop();
					watch_ctx_map_t::iterator&& ctxit = m_ctxs.find(actop.fd);
					switch (actop.act) {
					case aio_action::READ:
					{
#ifdef NETP_DEBUG_TERMINATING
						NETP_ASSERT(m_terminated == false);
#endif

						NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::READ", m_type, actop.fd);
						NETP_ASSERT(ctxit != m_ctxs.end());
						int rt = _do_watch(actop.fd, aio_flag::AIO_READ, ctxit->second);
						if (netp::OK == rt) {
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
							NETP_ASSERT(((ctxit->second->flag & aio_flag::AIO_READ) == 0 && ctxit->second->iofn[aio_flag::AIO_READ] == nullptr), "fd: %d, flag: %d", actop.fd, ctxit->second->flag);
							ctxit->second->flag |= aio_flag::AIO_READ;
#endif
							ctxit->second->iofn[aio_flag::AIO_READ] = actop.fn;
						} else {
							const int ec = netp_socket_get_last_errno();
							NETP_WARN("[io_event_loop][type:%d][#%d]aio_action::READ failed", m_type, actop.fd, ec);
							actop.fn(ec);
						}
					}
					break;
					case aio_action::END_READ:
					{
						NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END_READ", m_type, actop.fd);
						NETP_ASSERT(ctxit != m_ctxs.end());
						if (ctxit->second->iofn[aio_flag::AIO_READ] != nullptr) {
							//we need this condition check ,cuz epoll might fail to watch
							_do_unwatch(actop.fd, aio_flag::AIO_READ, ctxit->second);
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
							NETP_ASSERT(((ctxit->second->flag & aio_flag::AIO_READ) != 0 && ctxit->second->iofn[aio_flag::AIO_READ] != nullptr), "fd: %d, flag: %d", actop.fd, ctxit->second->flag);
							ctxit->second->flag &= ~aio_flag::AIO_READ;
#endif
							ctxit->second->iofn[aio_flag::AIO_READ] = nullptr;
						}
					}
					break;
					case aio_action::WRITE:
					{

#ifdef NETP_DEBUG_TERMINATING
						NETP_ASSERT(m_terminated == false);
#endif
						NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::WRITE", m_type, actop.fd);
						NETP_ASSERT(ctxit != m_ctxs.end());
						int rt = _do_watch(actop.fd, aio_flag::AIO_WRITE, ctxit->second);
						if (netp::OK == rt) {
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
							NETP_ASSERT(((ctxit->second->flag & aio_flag::AIO_WRITE) == 0 && ctxit->second->iofn[aio_flag::AIO_WRITE] == nullptr), "fd: %d, flag: %d", actop.fd, ctxit->second->flag);
							ctxit->second->flag |= aio_flag::AIO_WRITE;
#endif
							ctxit->second->iofn[aio_flag::AIO_WRITE] = actop.fn;
						} else {
							const int ec = netp_socket_get_last_errno();
							NETP_WARN("[io_event_loop][type:%d][#%d]aio_action::WRITE failed, ec: %d", m_type, actop.fd, ec);
							actop.fn(ec);
						}
					}
					break;
					case aio_action::END_WRITE:
					{
						NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END_WRITE", m_type, actop.fd);
						NETP_ASSERT(ctxit != m_ctxs.end());
						if (ctxit->second->iofn[aio_flag::AIO_WRITE] != nullptr) {
							//we need this condition check ,cuz epoll might fail to watch
							_do_unwatch(actop.fd, aio_flag::AIO_WRITE, ctxit->second);
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
							NETP_ASSERT(((ctxit->second->flag & aio_flag::AIO_WRITE) != 0 && ctxit->second->iofn[aio_flag::AIO_WRITE] != nullptr), "fd: %d, flag: %d", actop.fd, ctxit->second->flag);
							ctxit->second->flag &= ~aio_flag::AIO_WRITE;
#endif
							ctxit->second->iofn[aio_flag::AIO_WRITE] = nullptr;
						}
					}
					break;
					case aio_action::BEGIN:
					{

#ifdef NETP_DEBUG_TERMINATING
						NETP_ASSERT(m_terminated == false);
#endif

						if (m_cfg.maxiumctx != 0 && m_ctxs.size() >= m_cfg.maxiumctx) {
							NETP_WARN("[io_event_loop][type:%d][#%d]aio_action::BEGIN limitation(%u)", m_type, actop.fd, m_cfg.maxiumctx);
							actop.fn(netp::E_IO_EVENT_LOOP_MAXIMUM_CTX_LIMITATION);
						} else {
							NETP_ASSERT(ctxit == m_ctxs.end(), "fd: %d", actop.fd);
							NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::BEGIN", m_type, actop.fd);

							NRP<watch_ctx> ctx_ = netp::make_ref<watch_ctx>();
							ctx_->fd = actop.fd;
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
							ctx_->flag = 0;
#endif

#ifdef NETP_DEBUG_TERMINATING
							ctx_->terminated = false;
#endif
							ctx_->iofn[aio_flag::AIO_NOTIFY] = actop.fn;
							ctx_->iofn[aio_flag::AIO_READ] = nullptr;
							ctx_->iofn[aio_flag::AIO_WRITE] = nullptr;

							m_ctxs.insert({ actop.fd,std::move(ctx_) });
							actop.fn(netp::OK);
						}
					}
					break;
					case aio_action::NOTIFY_TERMINATING:
					{
						//no more add(ctx) opertion after terminating
#ifdef NETP_DEBUG_TERMINATING
						NETP_ASSERT(m_terminated == false);
						m_terminated = true;
#endif
						watch_ctx_map_t::iterator&& it = m_ctxs.begin();
						while (it != m_ctxs.end()) {
							NRP<watch_ctx> ctx = (it++)->second;
							if (ctx->fd == m_signalfds[0]) {
								continue;
							}
							NETP_ASSERT(ctx->fd > 0);
							NETP_ASSERT(ctx->iofn[aio_flag::AIO_NOTIFY] != nullptr);
							for (i8_t i = aio_flag::AIO_WRITE; i >= aio_flag::AIO_NOTIFY; --i) {
								if (ctx->iofn[i] != nullptr) {
									NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::NOTIFY_TERMINATING, io_flag: %d", m_type, ctx->fd, i);
									ctx->iofn[i](netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING);
								}
							}

#ifdef NETP_DEBUG_TERMINATING
							ctx->terminated = true;
#endif
						}

						//no competitor here, store directly
						NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_TERMINATING));
						m_state.store(u8_t(loop_state::S_TERMINATED), std::memory_order_release);

						NETP_ASSERT(m_tb != nullptr);
						m_tb->expire_all();
					}
					break;
					case aio_action::END:
					{
						NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END", m_type, actop.fd);
						NETP_ASSERT(ctxit != m_ctxs.end(), "fd: %d", actop.fd);
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
						NETP_ASSERT(ctxit->second->flag == 0);
#endif
						NETP_ASSERT((ctxit->second->iofn[aio_flag::AIO_READ] == nullptr));
						NETP_ASSERT((ctxit->second->iofn[aio_flag::AIO_WRITE] == nullptr));
						NETP_ASSERT((ctxit->second->iofn[aio_flag::AIO_NOTIFY] != nullptr));

						ctxit->second->iofn[aio_flag::AIO_NOTIFY] = nullptr;
						m_ctxs.erase(ctxit);
						NETP_ASSERT(actop.fn != nullptr);
						actop.fn(netp::OK);
					}
					break;
					case aio_action::BEGIN_READ_WRITE:
					{//for compiler warning...
					}
					break;
					}
					vecs = m_acts.size();//update vecs
				}

				m_acts.clear();
				if (vecs > 8192) {
					act_queue_t().swap(m_acts);
				}
			}
		}

		void __run();
		void __notify_terminating();
		int __launch();
		void __terminate();

	public:
		io_event_loop( io_poller_type t, poller_cfg const& cfg) :
			m_type(u8_t(t)),
			m_waiting(false),
			m_state(u8_t(loop_state::S_IDLE)),
			m_signalfds{ (SOCKET)NETP_INVALID_SOCKET, (SOCKET)NETP_INVALID_SOCKET },
			m_internal_ref_count(1),
			m_cfg(cfg)
		{}

		~io_event_loop() {
			NETP_ASSERT(m_tb == nullptr);
			NETP_ASSERT(m_th == nullptr);
		}

		inline void schedule(fn_io_event_task_t&& f) {
			//disable compiler order opt by barrier
			std::atomic<bool> _interrupt_poller(false);
			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				m_tq_standby.push_back(std::move(f));
				_interrupt_poller.store( m_tq_standby.size() == 1 && !in_event_loop() && m_waiting.load(std::memory_order_acquire), std::memory_order_release);
			}
			if (NETP_UNLIKELY(_interrupt_poller.load(std::memory_order_acquire))) {
				_do_poller_interrupt_wait();
			}
		}

		inline void schedule(fn_io_event_task_t const& f) {
			std::atomic<bool> _interrupt_poller(false);
			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				m_tq_standby.push_back(f);
				_interrupt_poller.store( m_tq_standby.size() == 1 && !in_event_loop() && m_waiting.load(std::memory_order_acquire), std::memory_order_release);
			}
			if (NETP_UNLIKELY(_interrupt_poller.load(std::memory_order_acquire))) {
				_do_poller_interrupt_wait();
			}
		}

		inline void execute(fn_io_event_task_t&& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(std::move(f));
		}

		inline void execute(fn_io_event_task_t const& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(f);
		}

		__NETP_FORCE_INLINE bool in_event_loop() const {
			return std::this_thread::get_id() == m_tid;
		}

		void launch(NRP<netp::timer> const& t , NRP<netp::promise<int>> const& lf = nullptr ) {
			if(!in_event_loop()) {
				schedule([L = NRP<io_event_loop>(this), t, lf]() {
					L->launch(t, lf);
				});
				return;
			}
			if (NETP_LIKELY(m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATED))) {
				m_tb->launch(t);
				(lf != nullptr)? lf->set(netp::OK):(void)0;
			} else {
				(lf != nullptr) ? lf->set(netp::E_IO_EVENT_LOOP_TERMINATED):NETP_THROW("DO NOT LAUNCH AFTER TERMINATED, OR PASS A PROMISE TO OVERRIDE THIS ERRO");
			}
		}

		void launch(NRP<netp::timer>&& t, NRP<netp::promise<int>> const& lf = nullptr) {
			if (!in_event_loop()) {
				schedule([L = NRP<io_event_loop>(this), t=std::move(t) , lf]() {
					L->launch(std::move(t), lf);
				});
				return;
			}
			if (NETP_LIKELY(m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATED))) {
				m_tb->launch(std::move(t));
				(lf != nullptr) ? lf->set(netp::OK) : (void)0;
			} else {
				(lf != nullptr) ? lf->set(netp::E_IO_EVENT_LOOP_TERMINATED) : NETP_THROW("DO NOT LAUNCH AFTER TERMINATED, OR PASS A PROMISE TO OVERRIDE THIS ERRO");
			}
		}

		inline io_poller_type type() const { return (io_poller_type)m_type; }
		inline void aio_do(aio_action act, SOCKET fd, fn_aio_event_t const& fn) {
			NETP_ASSERT(fd != NETP_INVALID_SOCKET );
			NETP_ASSERT(in_event_loop());

			if ( ((u8_t(act)&u8_t(aio_action::BEGIN_READ_WRITE)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING) ) {
				m_acts.push_back({ act,fd,fn });
			} else {
				fn(netp::E_IO_EVENT_LOOP_TERMINATED);
			}
		}

		inline void aio_do(aio_action act, SOCKET fd, fn_aio_event_t&& fn) {
			NETP_ASSERT(fd != NETP_INVALID_SOCKET);
			NETP_ASSERT(in_event_loop());

			if (((u8_t(act)&u8_t(aio_action::BEGIN_READ_WRITE)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				m_acts.push_back({ act,fd, std::move(fn) });
			} else {
				fn(netp::E_IO_EVENT_LOOP_TERMINATED);
			}
		}

		__NETP_FORCE_INLINE NRP<netp::packet> const& channel_rcv_buf() const {
			return m_channel_rcv_buf;
		}

#ifdef NETP_IO_POLLER_IOCP
		virtual void do_iocp_call(iocp_action act, SOCKET fd, fn_overlapped_io_event const& fn_overlapped, fn_aio_event_t const& fn) {
			NETP_ASSERT(m_type == T_IOCP);
		}
		inline void iocp_call( iocp_action act, SOCKET fd, fn_overlapped_io_event const& fn_overlapped, fn_aio_event_t const& fn) {
			NETP_ASSERT(fd != NETP_INVALID_SOCKET );
			execute([L=NRP<io_event_loop>(this), act, fd, fn_overlapped, fn]() -> void {
				L->do_IOCP_call(act,fd, fn_overlapped,fn);
			});
		}
#endif

	protected:
	#define __LOOP_EXIT_WAITING__() (m_waiting.store(false, std::memory_order_release))

		virtual void _do_poller_init();
		virtual void _do_poller_deinit() ;
		virtual void _do_poller_interrupt_wait() ;

		virtual void _do_poll(long long wait_in_nano ) = 0;
		virtual int _do_watch(SOCKET, u8_t, NRP<watch_ctx> const&) = 0;
		virtual int _do_unwatch(SOCKET,u8_t, NRP<watch_ctx> const&) = 0;
	};

	class bye_event_loop :
		public netp::io_event_loop
	{
		public:
			bye_event_loop(io_poller_type t, poller_cfg const& cfg):
				io_event_loop(t,cfg)
			{}

		protected:
			void _do_poller_init() override {}
			void _do_poller_deinit() override {}
			void _do_poller_interrupt_wait() override { NETP_ASSERT(!in_event_loop());}

			void _do_poll(long long wait_in_nano)  override;
			int _do_watch(SOCKET fd, u8_t flag, NRP<watch_ctx> const& ctx)  override;
			int _do_unwatch(SOCKET fd, u8_t flag, NRP<watch_ctx> const& ctx) override;
	};

	class app;
	typedef std::vector<NRP<io_event_loop>> io_event_loop_vector;
	class io_event_loop_group:
		public netp::singleton<io_event_loop_group>
	{
		friend class netp::app;
		enum class bye_event_loop_state {
			S_IDLE,
			S_PREPARING,
			S_RUNNING,
			S_EXIT
		};

	private:
		netp::shared_mutex m_pollers_mtx[T_POLLER_MAX];
		std::atomic<u32_t> m_curr_poller_idx[T_POLLER_MAX];
		io_event_loop_vector m_pollers[T_POLLER_MAX];

		int m_bye_ref_count;
		std::atomic<bye_event_loop_state> m_bye_state;
		NRP<io_event_loop> m_bye_event_loop;


		void init( int count[io_poller_type::T_POLLER_MAX], poller_cfg cfgs[io_poller_type::T_POLLER_MAX]);
		void deinit();

	public:
		io_event_loop_group();
		~io_event_loop_group();

		void notify_terminating(io_poller_type t);
		void dealloc_remove_poller(io_poller_type t);
		void alloc_add_poller(io_poller_type t, int count, poller_cfg const& cfg, fn_poller_maker_t const& fn_maker = nullptr);
		io_poller_type query_available_custom_poller_type();
		netp::size_t size(io_poller_type t);
		NRP<io_event_loop> next(io_poller_type t, std::set<NRP<io_event_loop>> const& exclude_this_set_if_have_more);

		NRP<io_event_loop> next(io_poller_type t = NETP_DEFAULT_POLLER_TYPE);
		NRP<io_event_loop> internal_next(io_poller_type t = NETP_DEFAULT_POLLER_TYPE);

		void execute(fn_io_event_task_t&& f, io_poller_type = NETP_DEFAULT_POLLER_TYPE);
		void schedule(fn_io_event_task_t&& f, io_poller_type = NETP_DEFAULT_POLLER_TYPE);
		void launch(NRP<netp::timer> const& t, NRP<netp::promise<int>> const& lf = nullptr, io_poller_type = NETP_DEFAULT_POLLER_TYPE);
	};
}
#endif
