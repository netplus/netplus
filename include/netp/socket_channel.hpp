#ifndef _NETP_SOCKET_CH_HPP_
#define _NETP_SOCKET_CH_HPP_

#include <queue>

#include <netp/smart_ptr.hpp>
#include <netp/string.hpp>
#include <netp/packet.hpp>
#include <netp/address.hpp>

#include <netp/socket_base.hpp>
#include <netp/channel.hpp>
#include <netp/dns_resolver.hpp>

//@NOTE: turn on this option would result in about 20% performance boost for EPOLL
#define NETP_ENABLE_FAST_WRITE

//in milliseconds
#define NETP_SOCKET_BDLIMIT_TIMER_DELAY_DUR (250)
#define NETP_DEFAULT_LISTEN_BACKLOG 256

namespace netp {

	class socket_cfg;
	typedef std::function<NRP<channel>(NRP<socket_cfg> const& cfg)> fn_channel_creator_t;
	class socket_cfg final :
		public ref_base
	{
	public:
		NRP<io_event_loop> L;
		SOCKET fd;
		u8_t family;
		u8_t type;
		u16_t proto;
		u16_t option;

		address laddr;
		address raddr;

		socket_api* sockapi;
		keep_alive_vals kvals;
		channel_buf_cfg sock_buf;
		u32_t bdlimit; //in bit (1kb == 1024b), 0 means no limit
		u32_t wsabuf_size;
		socket_cfg(NRP<io_event_loop> const& L = nullptr) :
			L(L),
			fd(SOCKET(NETP_INVALID_SOCKET)),
			family((NETP_AF_INET)),
			type(NETP_SOCK_STREAM),
			proto(NETP_PROTOCOL_TCP),
			option(default_socket_option),
			laddr(),
			raddr(),
			sockapi((netp::socket_api*)&netp::default_socket_api),
			kvals(default_tcp_keep_alive_vals),
			sock_buf({ 0 }),
			bdlimit(0),
			wsabuf_size(64*1024)
		{}
	};

	template <class socket_channel_t>
	std::tuple<int, NRP<socket_channel_t>> create(NRP<socket_cfg> const& cfg) {
		NETP_ASSERT(cfg->L != nullptr);
		NETP_ASSERT(cfg->L->in_event_loop());
		NETP_ASSERT(cfg->proto == NETP_PROTOCOL_USER ? cfg->L->poller_type() != NETP_DEFAULT_POLLER_TYPE : true);
		NRP<socket_channel_t> so = netp::make_ref<socket_channel_t>(cfg);
		int rt;
		if (cfg->fd == NETP_INVALID_SOCKET) {
			rt = so->open();
			if (rt != netp::OK) {
				NETP_WARN("[socket][%s]open failed: %d", so->ch_info().c_str(), rt);
				return std::make_tuple(rt,nullptr);
			}
		}
		rt = so->init(cfg->option, cfg->kvals, cfg->sock_buf);
		if (rt != netp::OK) {
			so->close();
			NETP_WARN("[socket][%s]init failed: %d", so->ch_info().c_str(), rt);
			return std::make_tuple(rt, nullptr);
		}
		return std::make_tuple(rt, so);
	}

	template <class socket_channel_t>
	std::tuple<int, NRP<socket_channel_t>> accepted_create(NRP<io_event_loop> const& L, SOCKET nfd, address const& laddr, address const& raddr, NRP<socket_cfg> const& cfg) {
		NETP_ASSERT(L->in_event_loop());
		NRP<socket_cfg> ccfg = netp::make_ref<socket_cfg>();
		ccfg->fd = nfd;
		ccfg->family = cfg->family;
		ccfg->type = cfg->type;
		ccfg->proto = cfg->proto;
		ccfg->laddr = laddr;
		ccfg->raddr = raddr;

		ccfg->L = L;
		ccfg->sockapi = cfg->sockapi;
		ccfg->option = cfg->option;
		ccfg->kvals = cfg->kvals;
		ccfg->sock_buf = cfg->sock_buf;
		ccfg->bdlimit = cfg->bdlimit;

		return create<socket_channel_t>(ccfg);
	}

	struct socket_outbound_entry final {
		NRP<packet> data;
		NRP<promise<int>> write_promise;
		address to;
	};

	class socket_channel:
		public channel,
		public socket_base
	{
		friend void do_dial(address const& addr, fn_channel_initializer_t const& initializer, NRP<channel_dial_promise> const& ch_dialf, NRP<socket_cfg> const& cfg);
		friend void do_listen_on(NRP<channel_listen_promise> const& listenp, address const& laddr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog);

	protected:
		aio_ctx* m_aio_ctx;
		byte_t* m_rcv_buf_ptr;
		u32_t m_rcv_buf_size;
		NRP<socket_cfg> m_listen_cfg;

		typedef std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>> socket_outbound_entry_t;
		socket_outbound_entry_t m_outbound_entry_q;
		netp::size_t m_noutbound_bytes;

		netp::size_t m_outbound_budget;
		netp::size_t m_outbound_limit; //in byte

		void _tmcb_BDL(NRP<timer> const& t);
	public:
		socket_channel(NRP<socket_cfg> const& cfg) :
			channel(cfg->L),
			socket_base(cfg->fd, cfg->family, cfg->type, cfg->proto, cfg->laddr, cfg->raddr, cfg->sockapi),
			m_aio_ctx(0),
			m_rcv_buf_ptr(cfg->L->channel_rcv_buf()->head()),
			m_rcv_buf_size(u32_t(cfg->L->channel_rcv_buf()->left_right_capacity())),

			m_noutbound_bytes(0),
			m_outbound_budget(cfg->bdlimit),
			m_outbound_limit(cfg->bdlimit)
		{
			NETP_ASSERT(cfg->L != nullptr);
		}

		~socket_channel()
		{
		}

	public:
		int init(u16_t opt, keep_alive_vals const& kvals, channel_buf_cfg const& cbc) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_chflag&int(channel_flag::F_CLOSED));
			channel::ch_init();
			int rt= socket_base::init(opt,kvals,cbc);
			if (rt != netp::OK) {
				ch_errno() = rt;
				m_chflag |= int(channel_flag::F_READ_ERROR);
				return rt;
			}
			m_chflag &= ~int(channel_flag::F_CLOSED);
			return rt;
		}
		int bind(address const& addr);
		int listen(int backlog = NETP_DEFAULT_LISTEN_BACKLOG);

		SOCKET accept(address& raddr);
		int connect(address const& addr);
		void do_async_connect(address const& addr, NRP<promise<int>> const& p);

	protected:
		//url example: tcp://0.0.0.0:80, udp://127.0.0.1:80
		//@todo
		//tcp6://ipv6address
		void do_listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<promise<int>> const& chp, NRP<socket_cfg> const& ccfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
		//NRP<promise<int>> listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<socket_cfg> const& cfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);

		void do_dial(address const& addr, fn_channel_initializer_t const& initializer, NRP<promise<int>> const& chp);
		//NRP<promise<int>> dial(address const& addr, fn_channel_initializer_t const& initializer);

		void _ch_do_close_read() {
			if ((m_chflag & int(channel_flag::F_READ_SHUTDOWN))) { return; }

			NETP_ASSERT((m_chflag & int(channel_flag::F_READ_SHUTDOWNING)) == 0);
			m_chflag |= int(channel_flag::F_READ_SHUTDOWNING);

			ch_aio_end_read();
			//end_read and log might result in F_READ_SHUTDOWN state. (FOR net_logger)

			socket_base::shutdown(SHUT_RD);
			m_chflag |= int(channel_flag::F_READ_SHUTDOWN);
			m_chflag &= ~int(channel_flag::F_READ_SHUTDOWNING);
			ch_fire_read_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read end, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
			ch_rdwr_shutdown_check();
		}

		void _ch_do_close_write() {
			if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN)) { return; }

			//boundary checking&set
			NETP_ASSERT((m_chflag & int(channel_flag::F_WRITE_SHUTDOWNING)) == 0);
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWNING);

			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
			ch_aio_end_write();

			while (m_outbound_entry_q.size()) {
				NETP_ASSERT((ch_errno() != 0) && (m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_READ_ERROR) | int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING))));
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				NETP_WARN("[socket][%s]cancel outbound, nbytes:%u, errno: %d", ch_info().c_str(), entry.data->len(), ch_errno());
				//hold a copy before we do pop it from queue
				NRP<promise<int>> wp = entry.write_promise;
				m_noutbound_bytes -= entry.data->len();
				m_outbound_entry_q.pop_front();
				NETP_ASSERT(wp->is_idle());
				wp->set(ch_errno());
			}

			socket_base::shutdown(SHUT_WR);
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN);
			//unset boundary
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWNING);
			ch_fire_write_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_write end, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
			ch_rdwr_shutdown_check();
		}

		void _do_dial_done_impl(int code, fn_channel_initializer_t const& initializer, NRP<promise<int>> const& chf);

		void __do_accept_fire(fn_channel_initializer_t const& ch_initializer) {
			ch_aio_begin([ch=NRP<socket_channel>(this),ch_initializer](int status, aio_ctx*) {
				int aiort = status;
				if (aiort != netp::OK) {
					//begin failed
					NETP_ASSERT(ch->ch_flag() & int(channel_flag::F_CLOSED));
					return;
				}
				try {
					if (NETP_LIKELY(ch_initializer != nullptr)) {
						ch_initializer(ch);
					}
				} catch (netp::exception const& e) {
					NETP_ASSERT(e.code() != netp::OK);
					aiort = e.code();
				} catch (std::exception const& e) {
					aiort = netp_socket_get_last_errno();
					if (aiort == netp::OK) {
						aiort = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept failed: %d:%s", aiort, e.what());
				} catch (...) {
					aiort = netp_socket_get_last_errno();
					if (aiort == netp::OK) {
						aiort = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept failed, %d: unknown", aiort);
				}

				if (aiort != netp::OK) {
					ch->ch_errno() = aiort;
					ch->ch_flag() |= int(channel_flag::F_READ_ERROR);
					ch->ch_close_impl(nullptr);
					NETP_ERR("[socket][%s]accept failed: %d", ch->ch_info().c_str(), aiort);
					return;
				}

				ch->ch_set_connected();
				ch->ch_fire_connected();
				ch->ch_aio_read();
			});
		}

		void __cb_aio_accept_impl(fn_channel_initializer_t const& fn_initializer, int status, aio_ctx* ctx);

		__NETP_FORCE_INLINE void ___aio_read_impl_done(const int aiort) {
			switch (aiort) {
			case netp::OK:
			case netp::E_SOCKET_READ_BLOCK:
			{}
			break;
			case netp::E_SOCKET_GRACE_CLOSE:
			{
				NETP_ASSERT(m_protocol != u8_t(NETP_PROTOCOL_UDP));
				m_chflag |= int(channel_flag::F_FIN_RECEIVED);
				ch_close_read_impl(nullptr);
			}
			break;
			default:
			{
				NETP_ASSERT(aiort < 0);
				ch_aio_end_read();
				m_chflag |= int(channel_flag::F_READ_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
				ch_errno() = (aiort);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]___aio_read_impl_done, _ch_do_close_read_write, read error: %d, close, flag: %u", ch_info().c_str(), aiort, m_chflag);
			}
			}
		}

		void __cb_aio_read_from_impl(int status, aio_ctx* ctx);
		void __cb_aio_read_impl(int status, aio_ctx* ctx);

		inline void __handle_aio_write_impl_done(const int aiort) {
			switch (aiort) {
			case netp::OK:
			{
				NETP_ASSERT((m_chflag & int(channel_flag::F_BDLIMIT)) == 0);
				NETP_ASSERT(m_outbound_entry_q.size() == 0);
				if (m_chflag & int(channel_flag::F_CLOSE_PENDING)) {
					_ch_do_close_read_write();
					NETP_TRACE_SOCKET("[socket][%s]aio_write, end F_CLOSE_PENDING, _ch_do_close_read_write, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
				}
				else if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
					_ch_do_close_write();
					NETP_TRACE_SOCKET("[socket][%s]aio_write, end F_WRITE_SHUTDOWN_PENDING, ch_close_write, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
				}
				else {
					std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>>().swap(m_outbound_entry_q);
					ch_aio_end_write();
				}
			}
			break;
			case netp::E_SOCKET_WRITE_BLOCK:
			{
				NETP_ASSERT(m_outbound_entry_q.size() > 0);
#ifdef NETP_ENABLE_FAST_WRITE
				NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER) | int(channel_flag::F_WATCH_WRITE)));
				ch_aio_write();
#else
				NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_WRITE));
#endif
				//NETP_TRACE_SOCKET("[socket][%s]__cb_aio_write_impl, write block", info().c_str());
			}
			break;
			case netp::E_CHANNEL_BDLIMIT:
			{
				m_chflag |= int(channel_flag::F_BDLIMIT);
				ch_aio_end_write();
			}
			break;
			default:
			{
				ch_aio_end_write();
				m_chflag |= int(channel_flag::F_WRITE_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
				ch_errno() = (aiort);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]__cb_aio_write_impl, call_ch_do_close_read_write, write error: %d, m_chflag: %u", ch_info().c_str(), aiort, m_chflag);
			}
			break;
			}
		}
		void __cb_aio_write_impl(int status, aio_ctx* ctx);

		//@note, we need simulate a async write, so for write operation, we'll flush outbound buffer in the next loop
		//flush until error
		//<0, is_error == (errno != E_CHANNEL_WRITING)
		//==0, flush done
		//this api would be called right after a check of writeable of the current socket
		int _do_ch_write_impl();
		int _do_ch_write_to_impl();

		//for connected socket type
		void _ch_do_close_listener();
		void _ch_do_close_read_write();

		void __aio_begin_done(aio_ctx*) {
			m_chflag |= int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE);
		}

		void __aio_notify_terminating(int status, aio_ctx*) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(status == netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING);
			//terminating notify, treat as a error
			NETP_ASSERT(m_chflag & int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE));
			m_chflag |= (int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING));
			m_cherrno = netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING;
			m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
			ch_close_impl(nullptr);
		}

	public:
		NRP<promise<int>> ch_set_read_buffer_size(u32_t size) override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), size, chp]() {
				chp->set(S->set_rcv_buffer_size(size));
			});
			return chp;
		}

		NRP<promise<int>> ch_get_read_buffer_size() override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), chp]() {
				chp->set(S->m_sock_buf.rcvbuf_size);
			});
			return chp;
		}

		NRP<promise<int>> ch_set_write_buffer_size(u32_t size) override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), size, chp]() {
				chp->set(S->set_snd_buffer_size(size));
			});
			return chp;
		}

		NRP<promise<int>> ch_get_write_buffer_size() override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([S = NRP<socket_channel>(this), chp]() {
				chp->set(S->m_sock_buf.sndbuf_size);
			});
			return chp;
		}

		NRP<promise<int>> ch_set_nodelay() override {
			NRP<promise<int>> chp = make_ref<promise<int>>();
			L->execute([s = NRP<socket_channel>(this), chp]() {
				chp->set(s->turnon_nodelay());
			});
			return chp;
		}

		void ch_aio_begin(fn_aio_event_t const& fn_begin_done) {
			NETP_ASSERT(is_nonblocking());

			if (!L->in_event_loop()) {
				L->schedule([s = NRP<socket_channel>(this), fn_begin_done]() {
					s->ch_aio_begin(fn_begin_done);
				});
				return;
			}

			NETP_ASSERT((m_chflag & (int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE))) == 0);

			m_aio_ctx = L->aio_begin(m_fd);
			if (m_aio_ctx == 0) {
				ch_errno() = netp::E_AIO_BEGIN_FAILED;
				m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
				fn_begin_done(netp::E_AIO_BEGIN_FAILED, 0);
				return;
			}
			m_aio_ctx->fn_notify = std::bind(&socket_channel::__aio_notify_terminating, NRP<socket_channel>(this), std::placeholders::_1, std::placeholders::_2);
			__aio_begin_done(m_aio_ctx);
			fn_begin_done(netp::OK, m_aio_ctx);
		}

		void ch_aio_end() {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_outbound_entry_q.size() == 0);
			NETP_ASSERT(m_noutbound_bytes == 0);
			NETP_ASSERT(m_chflag & int(channel_flag::F_CLOSED));
			NETP_ASSERT((m_chflag & (int(channel_flag::F_WATCH_READ) | int(channel_flag::F_WATCH_WRITE))) == 0);
			NETP_TRACE_SOCKET("[socket][%s]aio_action::END, flag: %d", info().c_str(), m_chflag);

			//****NOTE ON WINDOWS&IOCP****//
			//Any pending overlapped sendand receive operations(WSASend / WSASendTo / WSARecv / WSARecvFrom with an overlapped socket) issued by any thread in this process are also canceled.Any event, completion routine, or completion port action specified for these overlapped operations is performed.The pending overlapped operations fail with the error status WSA_OPERATION_ABORTED.
			//Refer to: https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket
			ch_fire_closed(close());
			if (m_chflag & int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE)) {
				L->schedule([so = NRP<socket_channel>(this)]() {
					so->m_aio_ctx->fn_notify = nullptr;
					so->L->aio_end(so->m_aio_ctx);
				});
			}
		}

	private:
		void ch_aio_accept(fn_channel_initializer_t const& fn_accepted_initializer) override {
			if (!L->in_event_loop()) {
				L->schedule([ch=NRP<channel>(this), fn_accepted_initializer ]() {
					ch->ch_aio_accept();
				});
				return;
			}
			NETP_ASSERT(L->in_event_loop());

			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]F_WATCH_READ state already", ch_info().c_str());
				return;
			}

			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]cancel for rd closed already", ch_info().c_str());
				return;
			}
			NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]watch AIO_READ", ch_info().c_str());

			//@TODO: provide custome accept feature
			//const fn_aio_event_t _fn = cb_accepted == nullptr ? std::bind(&socket::__cb_async_accept_impl, NRP<socket>(this), std::placeholders::_1) : cb_accepted;
			int rt = L->aio_do(aio_action::READ, m_aio_ctx);
			if (rt == netp::OK) {
				m_chflag |= int(channel_flag::F_WATCH_READ);
				m_aio_ctx->fn_read = std::bind(&socket_channel::__cb_aio_accept_impl, NRP<socket_channel>(this), fn_accepted_initializer, std::placeholders::_1, std::placeholders::_2);
			}
		}

		void ch_aio_end_accept() {
			ch_aio_end_read();
		}

	public:
		__NETP_FORCE_INLINE channel_id_t ch_id() const override { return m_fd; }
		std::string ch_info() const override { 
			return socketinfo{ m_fd, (m_family),(m_type),(m_protocol),local_addr(), remote_addr() }.to_string();
		}

		void ch_set_bdlimit(u32_t limit) override {
			L->execute([s = NRP<socket_channel>(this), limit]() {
				s->m_outbound_limit = limit;
				s->m_outbound_budget = s->m_outbound_limit;
			});
		};

		void ch_write_impl(NRP<packet> const& outlet, NRP<promise<int>> const& chp) override;
		void ch_write_to_impl(NRP<packet> const& outlet, netp::address const& to, NRP<promise<int>> const& chp) override;

		void ch_close_read_impl(NRP<promise<int>> const& closep) override
		{
			NETP_ASSERT(L->in_event_loop());
			NETP_TRACE_SOCKET("[socket][%s]ch_close_read_impl, _ch_do_close_read, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
			int prt = netp::OK;
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING))) {
				prt = (netp::E_OP_INPROCESS);
			}
			else if ((m_chflag & int(channel_flag::F_READ_SHUTDOWN)) != 0) {
				prt = (netp::E_CHANNEL_WRITE_CLOSED);
			}
			else {
				_ch_do_close_read();
			}

			if (closep) { closep->set(prt); }
		}

		void ch_close_write_impl(NRP<promise<int>> const& chp) override;
		void ch_close_impl(NRP<promise<int>> const& chp) override;

		void ch_aio_read(fn_aio_event_t const& fn_read = nullptr) {
			if (!L->in_event_loop()) {
				L->schedule([s = NRP<socket_channel>(this), fn_read]()->void {
					s->ch_aio_read(fn_read);
				});
				return;
			}
			NETP_ASSERT((m_chflag & int(channel_flag::F_READ_SHUTDOWNING)) == 0);
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s]aio_action::READ, ignore, flag: %d", info().c_str(), m_chflag);
				return;
			}

			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_READ)) == 0);
				if (fn_read != nullptr) {
					fn_read(netp::E_CHANNEL_READ_CLOSED, nullptr);
				}
				return;
			}
			int rt = L->aio_do(aio_action::READ, m_aio_ctx);
			if (rt == netp::OK) {
				m_chflag |= int(channel_flag::F_WATCH_READ);
				m_aio_ctx->fn_read = fn_read != nullptr ? fn_read :
					m_protocol == NETP_PROTOCOL_TCP ? std::bind(&socket_channel::__cb_aio_read_impl, NRP<socket_channel>(this), std::placeholders::_1, std::placeholders::_2) :
					std::bind(&socket_channel::__cb_aio_read_from_impl, NRP<socket_channel>(this), std::placeholders::_1, std::placeholders::_2);
			}
			NETP_TRACE_IOE("[socket][%s]aio_action::READ", info().c_str());
		}

		void ch_aio_end_read() {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this)]()->void {
					_so->ch_aio_end_read();
				});
				return;
			}

			if ((m_chflag & int(channel_flag::F_WATCH_READ))) {
				m_chflag &= ~int(channel_flag::F_WATCH_READ);
				L->aio_do(aio_action::END_READ, m_aio_ctx);
				m_aio_ctx->fn_read = nullptr;
				NETP_TRACE_IOE("[socket][%s]aio_action::END_READ", info().c_str());
			}
		}

		void ch_aio_write(fn_aio_event_t const& fn_write = nullptr) override {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this), fn_write]()->void {
					_so->ch_aio_write(fn_write);
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				NETP_ASSERT(m_chflag & int(channel_flag::F_CONNECTED));
				if (fn_write != nullptr) {
					fn_write(netp::E_SOCKET_OP_ALREADY, 0);
				}
				return;
			}

			if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_WRITE)) == 0);
				NETP_TRACE_SOCKET("[socket][%s]aio_action::WRITE, cancel for wr closed already", info().c_str());
				if (fn_write != nullptr) {
					fn_write(netp::E_CHANNEL_WRITE_CLOSED, 0);
				}
				return;
			}

			int rt = L->aio_do(aio_action::WRITE, m_aio_ctx);
			if (rt == netp::OK) {
				m_chflag |= int(channel_flag::F_WATCH_WRITE);
				m_aio_ctx->fn_write = fn_write != nullptr ? fn_write :
					std::bind(&socket_channel::__cb_aio_write_impl, NRP<socket_channel>(this), std::placeholders::_1, std::placeholders::_2);
			}
		}

		void ch_aio_end_write() override {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel>(this)]()->void {
					_so->ch_aio_end_write();
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				m_chflag &= ~int(channel_flag::F_WATCH_WRITE);

				L->aio_do(aio_action::END_WRITE, m_aio_ctx);
				m_aio_ctx->fn_write = nullptr;
				NETP_TRACE_IOE("[socket][%s]aio_action::END_WRITE", info().c_str());
			}
		}

		void ch_aio_connect(fn_aio_event_t const& fn = nullptr) override {
			NETP_ASSERT(fn != nullptr);
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				return;
			}
			ch_aio_write(fn);
		}

		void ch_aio_end_connect() override {
			NETP_ASSERT(!ch_is_passive());
			ch_aio_end_write();
		}
	};
}
#endif