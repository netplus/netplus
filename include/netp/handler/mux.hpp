#ifndef  _NETP_HANDLER_MUX_HPP
#define _NETP_HANDLER_MUX_HPP

#include <forward_list>
#include <map>
#include <algorithm>
#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel.hpp>

#include <netp/thread.hpp>
#include <netp/mutex.hpp>
#include <netp/event_broker.hpp>

namespace netp { namespace handler {

	#define NETP_MUX_STREAM_DEFAULT_CWND_FACTOR (5)

	#define NETP_MUX_STREAM_FRAME_DATA_DEFAULT_MIN_SIZE (u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE)>>1)
	#define NETP_MUX_STREAM_FRAME_DATA_DEFAULT_MAX_SIZE (0xffffU)

	//stream buf cfg is a reversed version of socket
	const channel_buf_cfg muxs_buf_cfg_default {
		(128*1024),
		(256*1024)
	};

#define NETP_MUX_STREAM_FRAME_HEADER_LEN (4+1+4+2)

	enum mux_stream_frame_flag {
		FRAME_SYN = 1,
		FRAME_DATA = 1 << 1,
		FRAME_UWND = 1 << 2, //force update WND
		FRAME_RST = 1 << 3,
		FRAME_FIN = 1 << 4,
		FRAME_MUX_STREAM_MESSAGE_TYPE_MAX = (FRAME_FIN + 1)
	};

	typedef u32_t mux_stream_id_t;
	typedef u8_t mux_stream_frame_flag_t;

#pragma pack(push,1)
	union mux_stream_frame_header {
		struct _mux_stream_frame_header_ {
			u32_t id;
			u8_t flag;
			u32_t wnd;
			u16_t dlen;
		} H;
		netp::byte_t payload[NETP_MUX_STREAM_FRAME_HEADER_LEN];
	};
#pragma pack(pop)

	const seconds_duration_t mux_stream_throughput_delimiter = seconds_duration_t(7);
	extern int mux_make_stream_id();

	static_assert( sizeof(mux_stream_frame_header) == NETP_MUX_STREAM_FRAME_HEADER_LEN, "mux_stream_frame_header_len assert failed");
	inline NRP<netp::packet> mux_stream_make_frame(mux_stream_id_t id, u8_t flag, u32_t wnd, NRP<packet> const & data ) {
		NRP<packet> p = data;
		if (p == nullptr) {
			p = netp::make_ref<packet>();
		}
		NETP_ASSERT(p->len() <= NETP_MUX_STREAM_FRAME_DATA_DEFAULT_MAX_SIZE);
		mux_stream_frame_header fh;
		fh.H.id = id;
		fh.H.flag = flag;
		fh.H.wnd = wnd;
		fh.H.dlen = u16_t(p->len());
		p->write_left(fh.payload, NETP_MUX_STREAM_FRAME_HEADER_LEN);
		return p;
	}

	struct mux_stream_outbound_entry {
		NRP<packet> data ;
		NRP<promise<int>> wp;
	};

	class mux;
	class mux_stream final:
		public netp::channel
	{
		friend class mux;

		mux_stream_id_t m_id;

		u32_t m_snd_wnd; //accumulate wnd
		u32_t m_rcv_wnd;
		u32_t m_rcv_data_inc; //rcv incred
		u32_t m_snd_dynamic;
		u32_t m_frame_data_size_max;

		NRP<netp::handler::mux> m_transport_mux;

		std::queue<mux_stream_outbound_entry> m_outlets_q;
		std::queue<NRP<netp::packet>> m_incomes_buffer_q;

		//steady_seconds_timepoint_t m_write_bytes_last_update;
		//u64_t m_write_nbytes_last_avgs; //every 5s
		bool m_fin_enqueue_done;

		void init(netp::channel_buf_cfg const& cfg ) {
			NETP_ASSERT(L->in_event_loop());
			_ch_set_read_buffer_size(cfg.rcvbuf_size);
			_ch_set_write_buffer_size(cfg.sndbuf_size);
			channel::ch_init();
		}

		void deinit() {
			channel::ch_deinit();
		}

		void __check_dial_ok() {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_transport_mux != nullptr);
			NETP_ASSERT(m_chflag&int(channel_flag::F_CONNECTING));
			ch_set_connected();
		}

		void _ch_do_close_read() {
			if (m_chflag& (int(channel_flag::F_READ_SHUTDOWN)|int(channel_flag::F_READ_SHUTDOWNING))) { return; }

			m_chflag |= int(channel_flag::F_READ_SHUTDOWNING);

			ch_io_end_read();
			//update wndinc to remote
			NETP_TRACE_STREAM("[muxs][s%u]_ch_do_close_read, left read packet: %u", ch_id(), m_incomes_buffer_q.size());
			while (m_incomes_buffer_q.size()) {
				m_rcv_data_inc += u32_t(m_incomes_buffer_q.front()->len());
				m_incomes_buffer_q.pop();
			}
			_check_rcv_data_inc();
			ch_fire_read_closed();

			m_chflag |= int(channel_flag::F_READ_SHUTDOWN);
			m_chflag &= ~int(channel_flag::F_READ_SHUTDOWNING);
			ch_rdwr_shutdown_check();
		}

		void _ch_do_close_write() {
			if (m_chflag&(int(channel_flag::F_WRITE_SHUTDOWN)|int(channel_flag::F_WRITE_SHUTDOWNING) )) { return; }
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWNING);

			m_chflag &= ~(int(channel_flag::F_WRITE_SHUTDOWN_PENDING)|int(channel_flag::F_WRITING));
			_ch_do_cancel_all_outlets();

			ch_fire_write_closed();
			NETP_TRACE_STREAM("[muxs][s%u]_ch_do_close_write", m_id);
			m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN);
			m_chflag &= ~int(channel_flag::F_WRITE_SHUTDOWNING);

			ch_rdwr_shutdown_check();
		}

		//can not update value after connection established
		int _ch_set_read_buffer_size(int size) {
			if ( (m_chflag& int(channel_flag::F_CLOSED))== 0) {
				return netp::E_INVALID_STATE;
			}

			if (m_rcv_wnd == u32_t(size)) { return netp::OK; }

			if (size == 0) {
				m_rcv_wnd = muxs_buf_cfg_default.rcvbuf_size;
			} else if (size < int(channel_buf_range::CH_BUF_RCV_MIN_SIZE)) {
				m_rcv_wnd = u32_t(channel_buf_range::CH_BUF_RCV_MIN_SIZE);
			} else if (size > int(channel_buf_range::CH_BUF_RCV_MAX_SIZE)) {
				m_rcv_wnd = u32_t(channel_buf_range::CH_BUF_RCV_MAX_SIZE);
			} else {
				m_rcv_wnd = size;
			}

			return netp::OK;
		}

		int _ch_get_read_buffer_size() {
			return m_rcv_wnd;
		}

		int _ch_set_write_buffer_size(int size) {
			NETP_TRACE_STREAM("muxs[s%u]update write buffer from: %u to: %u", m_id, m_snd_wnd, size);
			if ((m_chflag& int(channel_flag::F_CLOSED)) ==0) {
				return netp::E_INVALID_STATE;
			}
			if (m_snd_wnd == u32_t(size)) { return netp::OK; }

			if (size == 0) {
				m_snd_wnd = muxs_buf_cfg_default.sndbuf_size;
			} else if (size < int(channel_buf_range::CH_BUF_SND_MIN_SIZE)) {
				m_snd_wnd = u32_t(channel_buf_range::CH_BUF_SND_MIN_SIZE);
			} else if (size > int(channel_buf_range::CH_BUF_SND_MAX_SIZE)) {
				m_snd_wnd = u32_t(channel_buf_range::CH_BUF_SND_MAX_SIZE);
			} else {
				m_snd_wnd = size;
			}

			NETP_ASSERT(NETP_MUX_STREAM_DEFAULT_CWND_FACTOR >= 1);
			m_snd_dynamic = m_snd_wnd * NETP_MUX_STREAM_DEFAULT_CWND_FACTOR;

			if (m_frame_data_size_max > (m_snd_dynamic >> 2)) {
				m_frame_data_size_max = (m_snd_dynamic >> 2);
			}

			m_frame_data_size_max = NETP_MAX(m_frame_data_size_max, NETP_MUX_STREAM_FRAME_DATA_DEFAULT_MIN_SIZE);
			return netp::OK;
		}

		int _ch_get_write_buffer_size() {
			return m_snd_wnd;
		}

		//
		inline void _write_data(NRP<packet> const& data, NRP<promise<int>> const& write_p) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(data != nullptr);

			netp::size_t left = data->len();
			netp::size_t wlen = 0;
		
			while (left>m_frame_data_size_max) {
				m_outlets_q.push({
					mux_stream_make_frame(m_id, FRAME_DATA, 0, netp::make_ref<packet>(data->head() + wlen, m_frame_data_size_max)),
					nullptr
				});
				wlen += m_frame_data_size_max;
				left -= m_frame_data_size_max;
			}

			m_outlets_q.push({
				mux_stream_make_frame(m_id, FRAME_DATA, 0, netp::make_ref<packet>(data->head() + wlen, left)),
				write_p
			});
			ch_flush_impl();
		}

		inline void _write_frame(NRP<packet> const& frame, NRP<promise<int>> const& write_p) {
			m_outlets_q.push({
				frame,
				write_p
			});
			ch_flush_impl();
		}

		void _do_ch_zero_outlets_check();
		void _ch_flush_done(const int status, u16_t wt);
		void _do_ch_flush_impl() ;

		void _ch_do_cancel_all_outlets() {
			NETP_ASSERT(L->in_event_loop());
			while (m_outlets_q.size()) {
				const mux_stream_outbound_entry& f_entry = m_outlets_q.front();
				const NRP<netp::promise<int>>& p = f_entry.wp;

				NETP_TRACE_STREAM("[muxs][s%u]write cancel, bytes: %u", m_id, f_entry.data->len() );
				if (p != nullptr) {
					f_entry.wp->set(ch_errno() == netp::OK ? netp::E_CHANNEL_ABORT : ch_errno());
				}
				m_outlets_q.pop();
			}
		}

		void _ch_do_close_connecting() {
			if( ch_errno() == netp::OK) {
				ch_errno()=(netp::E_ECONNABORTED);
			}
			m_chflag |=int(channel_flag::F_CLOSED);
			NETP_ASSERT(m_outlets_q.size() == 0);
			ch_fire_closed(netp::OK);
		}

		void _ch_do_close_read_write() {
			NETP_ASSERT(L->in_event_loop());
			if (m_chflag & (int(channel_flag::F_CLOSED) | int(channel_flag::F_CLOSING))) {
				return;
			}

			m_chflag |= int(channel_flag::F_CLOSING);
			m_chflag &= ~(int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_CONNECTED));

			NETP_TRACE_STREAM("[muxs][s%u]do_close mux_stream", m_id);
			_ch_do_close_read();
			_ch_do_close_write();
			m_chflag &= ~int(channel_flag::F_CLOSING);

			ch_rdwr_shutdown_check();
		}

		void __check_incomes_buffer() {
			NETP_ASSERT(L->in_event_loop());
			while ((m_chflag&int(channel_flag::F_WATCH_READ)) && m_incomes_buffer_q.size()) {
				m_rcv_data_inc += u32_t(m_incomes_buffer_q.front()->len());
				ch_fire_read(m_incomes_buffer_q.front());
				m_incomes_buffer_q.pop();
				if (m_incomes_buffer_q.size() == 0) {
					std::queue<NRP<netp::packet>>().swap(m_incomes_buffer_q);
				}
			}
			_check_rcv_data_inc();
		}


		void _do_dial(fn_channel_initializer_t const& initializer, NRP<promise<int>> const& chp);

		void dial(fn_channel_initializer_t const& initializer, NRP<promise<int>> const& chp) {
			L->execute([s = NRP<mux_stream>(this), initializer, chp]() -> void {
				s->_do_dial(initializer, chp);
			});
		}

		NRP<promise<int>> dial(fn_channel_initializer_t const& initializer) {
			NRP<promise<int>> ch_df = netp::make_ref<netp::promise<int>>();
			L->execute([s = NRP<mux_stream>(this), initializer, ch_df]() -> void {
				s->_do_dial(initializer, ch_df);
			});
			return ch_df;
		}

		inline void _check_snd_dynamic_inc(u32_t inc) {

			m_snd_dynamic += inc;
			NETP_ASSERT(m_snd_dynamic <= (m_snd_wnd * NETP_MUX_STREAM_DEFAULT_CWND_FACTOR) );
			if ((inc > 0) && (m_chflag&int(channel_flag::F_BDLIMIT))) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WRITING)) == 0);
				m_chflag &= ~int(channel_flag::F_BDLIMIT);
				ch_flush_impl();
			}
		}

		void _timer_updatewndforremote(NRP<netp::timer> const& t);

		void _check_rcv_data_inc() {
			if ((m_chflag&int(channel_flag::F_TIMER_1)) == 0 && m_rcv_data_inc > 0) {
				NRP<netp::timer> tm_UPDATEWND = netp::make_ref<netp::timer>(std::chrono::milliseconds(50),
					&mux_stream::_timer_updatewndforremote, NRP<mux_stream>(this), std::placeholders::_1
				);
				m_chflag |= int(channel_flag::F_TIMER_1);
				L->launch(std::move(tm_UPDATEWND), netp::make_ref<promise<int>>());
			}
		}

		void _frame_arrive(mux_stream_frame_flag_t flag, u32_t wnd, NRP<netp::packet> const& data);
	public:
		mux_stream(mux_stream_id_t id, NRP<netp::handler::mux> const& ctx, NRP<netp::io_event_loop> const& loop);
		~mux_stream();

		channel_id_t ch_id() const override { return (mux_stream_id_t)(m_id & 0xFFFFFFFF); }
		std::string ch_info() const override { return "s"+std::to_string(m_id); }

		NRP<promise<int>> ch_set_read_buffer_size(u32_t size) override {
			NRP<promise<int>> p = make_ref<promise<int>>();
			L->execute([muxs = NRP<mux_stream>(this), size, p]() {
				p->set(muxs->_ch_set_read_buffer_size(size));
			});
			return p;
		}

		NRP<promise<int>> ch_get_read_buffer_size() override {
			NRP<promise<int>> p = make_ref<promise<int>>();
			L->execute([muxs = NRP<mux_stream>(this), p]() {
				p->set(muxs->_ch_get_read_buffer_size());
			});
			return p;
		}

		NRP<promise<int>> ch_set_write_buffer_size(u32_t size) override {
			NRP<promise<int>> p = make_ref<promise<int>>();
			L->execute([S = NRP<mux_stream>(this), size, p]() {
				p->set(S->_ch_set_write_buffer_size(size));
			});
			return p;
		}
		NRP<promise<int>> ch_get_write_buffer_size() override {
			NRP<promise<int>> p = make_ref<promise<int>>();
			L->execute([S = NRP<mux_stream>(this), p]() {
				p->set(S->_ch_get_write_buffer_size());
			});
			return p;
		}

		NRP<promise<int>> ch_set_nodelay() override {
			NRP<promise<int>> p = make_ref<promise<int>>();
			L->execute([S = NRP<mux_stream>(this), p]() {
				p->set(netp::OK);
			});
			return p;
		}
		void ch_io_read(fn_io_event_t const& fn_read = nullptr) {
			if (!L->in_event_loop()) {
				L->schedule([ch = NRP<channel>(this)](){
					ch->ch_io_read();
				});
				return;
			}

			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWN)|int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_CLOSING)) ) {
				return;
			}
			NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_READ)) == 0);
			m_chflag |= int(channel_flag::F_WATCH_READ);
			__check_incomes_buffer();
			(void)fn_read;
		}

		void ch_io_end_read() {
			if (!L->in_event_loop()) {
				L->schedule([ch = NRP<channel>(this)](){
					ch->ch_io_end_read();
				});
				return;
			}
			m_chflag &= ~int(channel_flag::F_WATCH_READ);
		}

		//for channel compatible
		void ch_io_begin(fn_io_event_t const& ) override {};
		void ch_io_end() override {
			L->schedule([xs=NRP<mux_stream>(this)]() {
				xs->ch_fire_closed(netp::OK);
			});
		};

		void ch_io_accept(fn_io_event_t const& ) override {}
		void ch_io_end_accept() override {}

		//for channel compatible
		void ch_io_write(fn_io_event_t const& ) override { }
		void ch_io_end_write() override {}

		//for channel compatible
		void ch_io_connect(fn_io_event_t const& ) override {  }
		void ch_io_end_connect() override {}

		void ch_write_impl(NRP<packet> const& outlet, NRP<promise<int>> const& write_p) override {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(outlet != nullptr);
			NETP_ASSERT(write_p != nullptr);
			NETP_ASSERT(outlet->len() > 0);

			if (m_chflag &int(channel_flag::F_WRITE_SHUTDOWN)) {
				write_p->set(netp::E_CHANNEL_WRITE_CLOSED);
				return;
			} else if (m_chflag &(int(channel_flag::F_WRITE_SHUTDOWN_PENDING) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING))) {
				write_p->set(netp::E_CHANNEL_WRITE_SHUTDOWNING);
				return;
			} else {
				_write_data(outlet, write_p);
			}
		}

		inline void ch_flush_impl() {
			if (NETP_UNLIKELY((m_chflag & (int(channel_flag::F_WRITING)|int(channel_flag::F_BDLIMIT))) == 0)) {
				_do_ch_flush_impl();
			}
		}

		void ch_close_impl(NRP<promise<int>> const& closep) override;
		void ch_close_read_impl(NRP<promise<int>> const& closep) override {
			NETP_ASSERT(L->in_event_loop());
			int prt = netp::OK;
			if (m_chflag&(int(channel_flag::F_READ_SHUTDOWNING)|int(channel_flag::F_CLOSING)| int(channel_flag::F_CLOSE_PENDING))) {
				prt=(E_OP_INPROCESS);
			} else if (m_chflag&int(channel_flag::F_READ_SHUTDOWN)) {
				prt=(E_CHANNEL_READ_CLOSED);
			} else {
				_ch_do_close_read();
			}

			if (closep) { closep->set(prt); }
		}
		void ch_close_write_impl(NRP<promise<int>> const& closep) override {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT((m_chflag& int(channel_flag::F_CONNECTING)) == 0);
			int prt = netp::OK;
			if (m_chflag&int(channel_flag::F_WRITE_SHUTDOWN)) {
				prt=(E_CHANNEL_WRITE_CLOSED);
			} else if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWNING)|int(channel_flag::F_CLOSING) | int(channel_flag::F_CLOSE_PENDING))) {
				prt=(netp::E_OP_INPROCESS);
			} else if (m_chflag & (int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) ) {
				prt=(netp::E_CHANNEL_WRITE_SHUTDOWNING);
			} else if (m_chflag&int(channel_flag::F_WRITING)) {
				m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
				prt=(netp::E_CHANNEL_WRITE_SHUTDOWNING);
			} else if (ch_errno() != netp::OK) {
				NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_ERROR)|int(channel_flag::F_READ_ERROR) ) );
				_ch_do_close_write();
			} else {
				m_chflag |= int(channel_flag::F_WRITE_SHUTDOWN_PENDING);
				if (m_fin_enqueue_done == false) {
					m_fin_enqueue_done = true;
					_write_frame(mux_stream_make_frame(m_id, FRAME_FIN, 0, 0), make_ref<promise<int>>());
					NETP_TRACE_STREAM("[muxs][s%u][fin]push_frame by close_write", m_id);
				}
				prt=(netp::E_CHANNEL_WRITE_SHUTDOWNING);
			}
			if (closep) { closep->set(prt); }
		}
	};

	enum mux_ch_event {
		E_MUX_CH_CONNECTED,
		E_MUX_CH_CLOSED,
		E_MUX_CH_STREAM_ACCEPTED,
		E_MUX_CH_ERROR
	};

	class mux;
	typedef std::function<void(NRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(NRP<mux_stream> const& s)> fn_mux_stream_accepted_t;

	#define MUX_OUTLET_QUEUE_SIZE 32
	class mux final :
		public channel_handler_abstract,
		public event_broker_any
	{
		friend class mux_stream;
		typedef std::unordered_map<mux_stream_id_t, NRP<mux_stream>, std::hash<mux_stream_id_t>,std::equal_to<mux_stream_id_t>, netp::allocator<std::pair<const mux_stream_id_t, NRP<mux_stream>>> > stream_map_t;
		typedef std::pair<mux_stream_id_t, NRP<mux_stream> > stream_pair_t;

		struct mux_outlet_quota_entry {
			NRP<netp::packet> data;
			NRP<netp::promise<int>> wp;
			netp::size_t quota;
		};

		typedef std::forward_list<mux_outlet_quota_entry> mux_outlet_quota_entry_list_t;
		enum mux_transport_write_state {
			S_IDLE,
			S_WRITING,
			S_CLOSED
		};

		NRP<netp::io_event_loop> m_loop;
		NRP<netp::channel_handler_context> m_transport_ctx;

		stream_map_t m_stream_map;
		mux_transport_write_state m_write_state;

		std::size_t m_entry_count;
		mux_outlet_quota_entry_list_t::iterator m_entries_writeit;
		mux_outlet_quota_entry_list_t::iterator m_entries_lastit_prev;
		mux_outlet_quota_entry_list_t::iterator m_entries_lastit;
		mux_outlet_quota_entry_list_t m_entries;

		u32_t m_frame_data_size_max;
		channel_buf_cfg m_buf_cfg;
	private:
		void __do_mux_flush_done(const int rt);
		void __do_mux_flush();

		void __do_mux_write(NRP<netp::packet> const& outlet, NRP<netp::promise<int>> const& wp);

		std::tuple<int, NRP<mux_stream>> __do_open_stream(mux_stream_id_t id, channel_buf_cfg const& bcfg ) {
			NETP_ASSERT(m_loop->in_event_loop());
			if (m_transport_ctx == nullptr) {
				return std::make_tuple(netp::E_CHANNEL_CLOSED, nullptr);
			}
			stream_map_t::iterator&& it = m_stream_map.find(id);
			if (it != m_stream_map.end()) {
				return std::make_tuple(netp::E_CHANNEL_EXISTS, nullptr);
			}

			NRP<mux_stream> muxs = netp::make_ref<mux_stream>(id, NRP<mux>(this), m_transport_ctx->L);
			muxs->init(bcfg);
			m_stream_map.insert({ id, muxs });
			NETP_TRACE_STREAM("[mux][s%u]insert stream", id);
			muxs->ch_close_promise()->if_done([x = NRP<mux>(this), muxs](int const&) {
				muxs->deinit();
				::size_t c = x->m_stream_map.erase(u32_t(muxs->ch_id()));
				NETP_ASSERT(c == 1);
			});

			return std::make_tuple(netp::OK, muxs );
		}

	public:
		void do_dial(mux_stream_id_t id, fn_channel_initializer_t const& fn_initializer, NRP<channel_dial_promise> const& chdial_p, netp::channel_buf_cfg const& bcfg) {

			if (!m_loop->in_event_loop()) {
				m_loop->schedule([x=NRP<mux>(this), id, fn_initializer, chdial_p,bcfg]() {
					x->do_dial(id,fn_initializer, chdial_p,bcfg);
				});
				return;
			}

			int rt;
			NRP<mux_stream> s;
			std::tie(rt, s) = __do_open_stream(id,bcfg);
			if (rt != netp::OK) {
				chdial_p->set(std::make_tuple(rt,nullptr));
				return;
			}

			NRP<promise<int>> dial_p = netp::make_ref<promise<int>>();
			dial_p->if_done([chdial_p, s](int const& rt) {
				chdial_p->set(std::make_tuple(rt, s));
			});

			s->dial(fn_initializer, dial_p);
		}

		NRP<channel_dial_promise> dial(mux_stream_id_t id, fn_channel_initializer_t const& fn_initializer, netp::channel_buf_cfg const& bcfg = {128,128} ) {
			NETP_ASSERT(m_loop != nullptr);
			NRP<channel_dial_promise> ch_openp = netp::make_ref<channel_dial_promise>();
			m_loop->execute([id, ch_openp, x=NRP<mux>(this), fn_initializer,bcfg]() {
				x->do_dial(id,fn_initializer, ch_openp,bcfg);
			});
			return ch_openp;
		}

	public:
		mux(NRP<io_event_loop> const& L, channel_buf_cfg const& cfg = { channel_buf_range::CH_BUF_RCV_MAX_SIZE, channel_buf_range::CH_BUF_SND_MAX_SIZE });
		virtual ~mux();

		void connected(NRP<netp::channel_handler_context> const& ctx) override;
		void closed(NRP<netp::channel_handler_context> const& ctx) override;
		void error(NRP<netp::channel_handler_context> const& ctx, int err) override;
		void read_closed(NRP<netp::channel_handler_context> const& ctx) override;
		void write_closed(NRP<netp::channel_handler_context> const& ctx) override;

		void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) override;
	};
}}
#endif