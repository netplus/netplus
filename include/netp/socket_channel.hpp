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

		socket_api* sockapi;

		address laddr;
		address raddr;

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
			sockapi((netp::socket_api*)&netp::default_socket_api),
			laddr(),
			raddr(),
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
		rt = so->ch_init(cfg->option, cfg->kvals, cfg->sock_buf);
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
		typedef std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>> socket_outbound_entry_t;

		template <class socket_channel_t>
		friend std::tuple<int, NRP<socket_channel_t>> accepted_create(NRP<io_event_loop> const& L, SOCKET nfd, address const& laddr, address const& raddr, NRP<socket_cfg> const& cfg);

		template <class socket_channel_t>
		friend std::tuple<int, NRP<socket_channel_t>> create(NRP<socket_cfg> const& cfg);

		template <class _Ref_ty, typename... _Args>
		friend ref_ptr<_Ref_ty> make_ref(_Args&&... args);
	protected:
		io_ctx* m_io_ctx;
		byte_t* m_rcv_buf_ptr;
		u32_t m_rcv_buf_size;

		u32_t m_noutbound_bytes;
		socket_outbound_entry_t m_outbound_entry_q;

		u32_t m_outbound_budget;
		u32_t m_outbound_limit; //in byte

		fn_io_event_t* m_fn_read;
		fn_io_event_t* m_fn_write;

		void _tmcb_BDL(NRP<timer> const& t);

		socket_channel(NRP<socket_cfg> const& cfg) :
			channel(cfg->L),
			socket_base(cfg->fd, cfg->family, cfg->type, cfg->proto, cfg->laddr, cfg->raddr, cfg->sockapi),
			m_io_ctx(0),
			m_rcv_buf_ptr(cfg->L->channel_rcv_buf()->head()),
			m_rcv_buf_size(u32_t(cfg->L->channel_rcv_buf()->left_right_capacity())),
			m_noutbound_bytes(0),
			m_outbound_budget(cfg->bdlimit),
			m_outbound_limit(cfg->bdlimit),
			m_fn_read(nullptr),
			m_fn_write(nullptr)
		{
			NETP_ASSERT(cfg->L != nullptr);
		}

		~socket_channel()
		{
		}

		int ch_init(u16_t opt, keep_alive_vals const& kvals, channel_buf_cfg const& cbc) {
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

		//url example: tcp://0.0.0.0:80, udp://127.0.0.1:80
		//@todo
		//tcp6://ipv6address
		void do_listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<promise<int>> const& chp, NRP<socket_cfg> const& ccfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
		//NRP<promise<int>> listen_on(address const& addr, fn_channel_initializer_t const& fn_accepted, NRP<socket_cfg> const& cfg, int backlog = NETP_DEFAULT_LISTEN_BACKLOG);

		void do_dial(address const& addr, fn_channel_initializer_t const& fn_initializer, NRP<promise<int>> const& dialp);
		//NRP<promise<int>> dial(address const& addr, fn_channel_initializer_t const& initializer);

		void _ch_do_close_read() {
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING)|int(channel_flag::F_READ_SHUTDOWN)) ) { return; }

			m_chflag |= int(channel_flag::F_READ_SHUTDOWNING);
			ch_io_end_read();
			//end_read and log might result in F_READ_SHUTDOWN state. (FOR net_logger)
			socket_base::shutdown(SHUT_RD);
			ch_fire_read_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_read end, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			m_chflag &= ~int(channel_flag::F_READ_SHUTDOWNING);
			m_chflag |= int(channel_flag::F_READ_SHUTDOWN);

			ch_rdwr_shutdown_check();
		}

		void _ch_do_close_write() {
			if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_SHUTDOWNING)) ) { return; }

			//boundary checking&set
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWNING);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
			ch_io_end_write();

			while (m_outbound_entry_q.size()) {
				NETP_ASSERT((ch_errno() != 0) && (m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_READ_ERROR) | int(channel_flag::F_IO_EVENT_LOOP_NOTIFY_TERMINATING))));
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				NETP_WARN("[socket][%s]cancel outbound, nbytes:%u, errno: %d", ch_info().c_str(), entry.data->len(), ch_errno());
				//hold a copy before we do pop it from queue
				NRP<promise<int>> wp = entry.write_promise;
				m_noutbound_bytes -= u32_t(entry.data->len());
				m_outbound_entry_q.pop_front();
				NETP_ASSERT(wp->is_idle());
				wp->set(ch_errno());
			}

			socket_base::shutdown(SHUT_WR);
			//unset boundary
			ch_fire_write_closed();
			NETP_TRACE_SOCKET("[socket][%s]ch_do_close_write end, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWNING);
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN);
			ch_rdwr_shutdown_check();
		}

		void _ch_do_dial_done_impl(fn_channel_initializer_t const& fn_initializer, NRP<promise<int>> const& dialf, int status, io_ctx* ctx);

		void __do_accept_fire(fn_channel_initializer_t const& ch_initializer) {
			ch_io_begin([ch=NRP<socket_channel>(this),ch_initializer](int status, io_ctx*) {
				if (status != netp::OK) {
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
					status = e.code();
				} catch (std::exception const& e) {
					status = netp_socket_get_last_errno();
					if (status == netp::OK) {
						status = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept failed: %d:%s", status, e.what());
				} catch (...) {
					status = netp_socket_get_last_errno();
					if (status == netp::OK) {
						status = netp::E_UNKNOWN;
					}
					NETP_ERR("[socket]accept failed, %d: unknown", status);
				}

				if (status != netp::OK) {
					ch->ch_errno() = status;
					ch->ch_flag() |= int(channel_flag::F_READ_ERROR);
					ch->ch_close_impl(nullptr);
					NETP_ERR("[socket][%s]accept failed: %d", ch->ch_info().c_str(), status);
					return;
				}

				ch->ch_set_connected();
				ch->ch_fire_connected();
				ch->ch_io_read();
			});
		}

		void __cb_io_accept_impl(NRP<socket_cfg> const& cfg, fn_channel_initializer_t const& fn_initializer, int status, io_ctx* ctx);

		__NETP_FORCE_INLINE void ___io_read_impl_done(int status) {
			switch (status) {
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
				NETP_ASSERT(status < 0);
				ch_io_end_read();
				m_chflag |= int(channel_flag::F_READ_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
				ch_errno() = (status);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]___io_read_impl_done, _ch_do_close_read_write, read error: %d, close, flag: %u", ch_info().c_str(), status, m_chflag);
			}
			}
		}

		void __cb_io_read_from_impl(int status, io_ctx* ctx);
		void __cb_io_read_impl(int status, io_ctx* ctx);

		inline void __handle_io_write_impl_done(const int status) {
			switch (status) {
			case netp::OK:
			{
				NETP_ASSERT((m_chflag & int(channel_flag::F_BDLIMIT)) == 0);
				NETP_ASSERT(m_outbound_entry_q.size() == 0);
				if (m_chflag & int(channel_flag::F_CLOSE_PENDING)) {
					_ch_do_close_read_write();
					NETP_TRACE_SOCKET("[socket][%s]IO_WRITE, end F_CLOSE_PENDING, _ch_do_close_read_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
				}
				else if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
					_ch_do_close_write();
					NETP_TRACE_SOCKET("[socket][%s]IO_WRITE, end F_WRITE_SHUTDOWN_PENDING, ch_close_write, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
				}
				else {
					std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>>().swap(m_outbound_entry_q);
					ch_io_end_write();
				}
			}
			break;
			case netp::E_SOCKET_WRITE_BLOCK:
			{
				NETP_ASSERT(m_outbound_entry_q.size() > 0);
#ifdef NETP_ENABLE_FAST_WRITE
				NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_BARRIER) | int(channel_flag::F_WATCH_WRITE)));
				ch_io_write();
#else
				NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_WRITE));
#endif
				//NETP_TRACE_SOCKET("[socket][%s]__cb_io_write_impl, write block", info().c_str());
			}
			break;
			case netp::E_CHANNEL_BDLIMIT:
			{
				m_chflag |= int(channel_flag::F_BDLIMIT);
				ch_io_end_write();
			}
			break;
			default:
			{
				ch_io_end_write();
				m_chflag |= int(channel_flag::F_WRITE_ERROR);
				m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_BDLIMIT));
				ch_errno() = (status);
				ch_close_impl(nullptr);
				NETP_WARN("[socket][%s]__cb_io_write_impl, call_ch_do_close_read_write, write error: %d, m_chflag: %u", ch_info().c_str(), status, m_chflag);
			}
			break;
			}
		}
		void __cb_io_write_impl(int status, io_ctx* ctx);

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

		virtual void __io_begin_done(io_ctx*) {
			m_chflag |= int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE);
		}

		void ch_write_impl(NRP<packet> const& outlet, NRP<promise<int>> const& chp) override;
		void ch_write_to_impl(NRP<packet> const& outlet, netp::address const& to, NRP<promise<int>> const& chp) override;

		void ch_close_read_impl(NRP<promise<int>> const& closep) override
		{
			NETP_ASSERT(L->in_event_loop());
			NETP_TRACE_SOCKET("[socket][%s]ch_close_read_impl, _ch_do_close_read, errno: %d, flag: %d", ch_info().c_str(), ch_errno(), m_chflag);
			int prt = netp::OK;
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING))) {
				prt = (netp::E_OP_INPROCESS);
			} else if ((m_chflag & int(channel_flag::F_READ_SHUTDOWN)) != 0) {
				prt = (netp::E_CHANNEL_WRITE_CLOSED);
			} else {
				_ch_do_close_read();
			}
			if (closep) { closep->set(prt); }
		}

		void ch_close_write_impl(NRP<promise<int>> const& chp) override;
		void ch_close_impl(NRP<promise<int>> const& chp) override;

		//for io monitor
		virtual void io_notify_terminating(int status, io_ctx*) override;
		virtual void io_notify_read(int status, io_ctx* ctx) override;
		virtual void io_notify_write(int status, io_ctx* ctx) override;
		
		virtual void __ch_clean();
	public:
		void ch_io_begin(fn_io_event_t const& fn_begin_done) override;
		void ch_io_end() override;

		void ch_io_accept(fn_io_event_t const& fn = nullptr) override;
		void ch_io_end_accept() override {
			ch_io_end_read();
		}

		void ch_io_read(fn_io_event_t const& fn_read = nullptr) override;
		void ch_io_end_read() override;
		void ch_io_write(fn_io_event_t const& fn_write = nullptr) override;
		void ch_io_end_write() override;

		void ch_io_connect(fn_io_event_t const& fn = nullptr) override {
			NETP_ASSERT(fn != nullptr);
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				return;
			}
			ch_io_write(fn);
		}

		void ch_io_end_connect() override {
			NETP_ASSERT(!ch_is_passive());
			ch_io_end_write();
		}

		public:
			/*for other purpose*/
			int bind(address const& addr);
			int listen(int backlog = NETP_DEFAULT_LISTEN_BACKLOG);
			SOCKET accept(address& raddr);
			int connect(address const& addr);

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
					chp->set(S->get_rcv_buffer_size());
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
					chp->set(S->get_snd_buffer_size());
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

			__NETP_FORCE_INLINE channel_id_t ch_id() const override { return m_fd; }
			std::string ch_info() const override {
				return socketinfo{ m_fd, (m_family),(m_type),(m_protocol),local_addr(), remote_addr() }.to_string();
			}
			void ch_set_bdlimit(netp::u32_t limit) override {
				L->execute([s = NRP<socket_channel>(this), limit]() {
					s->m_outbound_limit = limit;
					s->m_outbound_budget = s->m_outbound_limit;
				});
			};
	};
}
#endif