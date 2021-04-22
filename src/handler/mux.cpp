#include <netp/handler/mux.hpp>

namespace netp { namespace handler {
	static std::atomic<int> __stream_id_{1};
	int mux_make_stream_id() {return __stream_id_.fetch_add(1, std::memory_order_relaxed) % 0x7FFFFFFF; }

	void mux_stream::_timer_updatewndforremote(NRP<netp::timer> const& t) {
		if (m_chflag&int(channel_flag::F_WRITING)) {
			NETP_ASSERT((m_chflag&int(channel_flag::F_BDLIMIT)) == 0);

			//if mux_stream rst by remote, we might get F_WRITING|F_WRITE_ERROR, zero m_outlets_q.size()
			NETP_ASSERT( m_outlets_q.size() );
			L->launch(t,netp::make_ref<netp::promise<int>>());
			return;
		}

		m_chflag &= ~int(channel_flag::F_TIMER_1);
		if (m_rcv_data_inc > 0) {
			//remote fin not send and local write not error, we have to report wnd to remote
			NRP<packet> ufp = mux_stream_make_frame(m_id, FRAME_UWND, m_rcv_data_inc, 0);
			m_rcv_data_inc = 0;//reset
			m_transport_mux->__do_mux_write(std::move(ufp), netp::make_ref<netp::promise<int>>());
		}
	}

	void mux_stream::_do_ch_zero_outlets_check() {
		NETP_ASSERT(m_outlets_q.size() == 0);

		if ((m_chflag & int(channel_flag::F_CLOSE_PENDING)) ) {
			_ch_do_close_read_write();
			NETP_TRACE_STREAM("[muxs][s%u]_ch_do_close_read_write by channel_flag::F_CLOSING", m_id);
		} else if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) {
			_ch_do_close_write();
			NETP_TRACE_STREAM("[muxs][s%u]_ch_do_close_write by channel_flag::F_WRITE_SHUTDOWNING", m_id);
		}
	}

	void mux_stream::_ch_flush_done(const int status, u16_t wt) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_chflag & int(channel_flag::F_WRITING));
		NETP_ASSERT( (m_chflag& int(channel_flag::F_CLOSED)) ==0 );

		switch (status) {
			case netp::OK:
			{
				NETP_ASSERT(m_outlets_q.size());
				const mux_stream_outbound_entry& f_entry = m_outlets_q.front();

				mux_stream_frame_header* fh = (mux_stream_frame_header*)f_entry.data->head();
				NETP_ASSERT(fh->H.dlen == wt); //test check
				NETP_ASSERT(m_snd_dynamic >= (fh->H.dlen));
				m_snd_dynamic -= wt;

				if ((fh->H.flag) & FRAME_FIN) {
					NETP_TRACE_STREAM("[muxs][s%u][fin]write done", m_id);
					NETP_ASSERT((m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) || (m_chflag & int(channel_flag::F_CLOSE_PENDING)));
				}

				if (f_entry.wp != nullptr) {
					f_entry.wp->set(netp::OK);
				}
				m_outlets_q.pop();
				m_chflag &= ~int(channel_flag::F_WRITING);

				if (m_outlets_q.empty()) {
					std::queue<mux_stream_outbound_entry>().swap(m_outlets_q);
					_do_ch_zero_outlets_check();
				} else {
					_do_ch_flush_impl();
				}
			}
			break;
			default:
			{
				NETP_ASSERT(status < 0 && status != netp::E_CHANNEL_WRITE_BLOCK);
				m_chflag |= int(channel_flag::F_WRITE_ERROR);
				m_chflag &= ~(int(channel_flag::F_WRITING)|int(channel_flag::F_CLOSE_PENDING)| int(channel_flag::F_BDLIMIT));
				ch_errno()=(status);
				ch_close_impl(nullptr);
			}
		}
	}

	void mux_stream::_do_ch_flush_impl() {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT((m_chflag & (int(channel_flag::F_WRITING)| int(channel_flag::F_BDLIMIT)) ) == 0);

		while (m_outlets_q.size()) {
			mux_stream_outbound_entry& f_entry = m_outlets_q.front();
			mux_stream_frame_header* fh = (mux_stream_frame_header*)f_entry.data->head();
			if (fh->H.dlen > m_snd_dynamic) {
				NETP_TRACE_STREAM("[muxs][s%u]stream write block, set block,m_snd_wnd: %u, queue bytes: %u, m_snd_dynamic: %u, incoming: %u", m_id, m_snd_wnd, m_outlets_q_nbytes, m_snd_dynamic, f.data->len());
				m_chflag |= int(channel_flag::F_BDLIMIT);
				return;
			}

			if (fh->H.flag == FRAME_UWND) {
				NETP_ASSERT(m_rcv_data_inc > 0);
				NETP_ASSERT(fh->H.dlen == 0);
			}

			if ((fh->H.flag) & FRAME_FIN) {
				NETP_TRACE_STREAM("[muxs][s%u][fin]writing", m_id);
				NETP_ASSERT((m_chflag & int(channel_flag::F_WRITE_SHUTDOWN_PENDING)) || (m_chflag & int(channel_flag::F_CLOSE_PENDING)) );
			}

			if (m_rcv_data_inc > 0) {
				fh->H.wnd = m_rcv_data_inc;
				m_rcv_data_inc = 0;
			}

			m_chflag |= int(channel_flag::F_WRITING);
			NRP<promise<int>> write_p = netp::make_ref<netp::promise<int>>();
			write_p->if_done([muxs = NRP<mux_stream>(this),wt=fh->H.dlen](int const& rt) {
				muxs->_ch_flush_done(rt,wt);
			});

			m_transport_mux->__do_mux_write(f_entry.data, write_p);
			break;
		}
	}

	void mux_stream::_do_dial(fn_channel_initializer_t const& initializer, NRP<promise<int>> const& dialp) {
		NETP_ASSERT(L->in_event_loop());

		ch_set_active();
		m_chflag |= int(channel_flag::F_CONNECTING);
		NRP<promise<int>> write_p = netp::make_ref<promise<int>>();
		write_p->if_done([s = NRP<mux_stream>(this), initializer, dialp](int const& rt) {
			s->L->execute([s, initializer, dialp, rt]() {
				if (rt != netp::OK) {
					dialp->set(rt);
					s->ch_errno() = rt;
					s->m_chflag |= int(channel_flag::F_WRITE_ERROR);
					s->ch_close_impl(nullptr);
					return;
				}

				s->__check_dial_ok();
				NETP_ASSERT(initializer != nullptr);
				initializer(s);
				dialp->set(netp::OK);
				NETP_ASSERT(s->m_snd_wnd >0, "info: %s", s->ch_info().c_str());
				NETP_ASSERT(s->m_rcv_wnd > 0, "info: %s", s->ch_info().c_str());
				s->ch_fire_connected();
				s->ch_io_read();
			});
		});

		NRP<packet> rcvsize_p = netp::make_ref<netp::packet>();
		rcvsize_p->write<u32_t>(m_snd_wnd);
		m_transport_mux->__do_mux_write(mux_stream_make_frame(m_id, FRAME_SYN, m_rcv_wnd, std::move(rcvsize_p)), write_p);
	}

	mux_stream::mux_stream(mux_stream_id_t id, NRP<netp::handler::mux> const& mux_, NRP<netp::io_event_loop> const& loop) :
		channel(loop),
		m_id(id),
		m_snd_wnd(0),
		m_rcv_wnd(0),
		m_rcv_data_inc(0),
		m_snd_dynamic(0),
		m_frame_data_size_max(u32_t(mux_->m_frame_data_size_max)),
		m_transport_mux(mux_),
		m_fin_enqueue_done(false)
	{
		NETP_TRACE_STREAM("[s%u]mux_stream::mux_stream()", m_id);
	}

	mux_stream::~mux_stream() {
		NETP_TRACE_STREAM("[s%u]mux_stream::~mux_stream()", m_id);
		//check F_TIMER_1 for timer launch failed case [loop exiting]
		NETP_ASSERT(((m_chflag&(int(channel_flag::F_WRITE_ERROR)|int(channel_flag::F_READ_ERROR)|int(channel_flag::F_TIMER_1)) ) == 0) ? m_rcv_data_inc == 0 : true);
	}

	void mux_stream::ch_close_impl(NRP<promise<int>> const& closep) {
		NETP_ASSERT(L->in_event_loop());
		int prt = netp::OK;
		if (m_chflag& int(channel_flag::F_CLOSED)) {
			prt = netp::E_CHANNEL_CLOSED;
		} else if (m_chflag& int(channel_flag::F_CONNECTING)) {
			_ch_do_close_connecting();
		} else if (m_chflag & (int(channel_flag::F_CLOSE_PENDING)|int(channel_flag::F_CLOSING)) ) {
			prt = (netp::E_OP_INPROCESS);
		} else if (m_chflag & (int(channel_flag::F_WRITING)|int(channel_flag::F_BDLIMIT))) {
			NETP_ASSERT(m_chflag& int(channel_flag::F_CONNECTED));
			NETP_ASSERT(m_outlets_q.size() != 0);
			m_chflag |= int(channel_flag::F_CLOSE_PENDING);
			prt = (netp::E_CHANNEL_CLOSING);
			NETP_TRACE_STREAM("[muxs][s%u][close]set closing by close, flag: %u", m_id, m_chflag);
		} else if (ch_errno() != netp::OK) {
			//there is no read error for mux_stream
			//NETP_ASSERT(m_chflag& int(channel_flag::F_CONNECTED));
			NETP_ASSERT(m_chflag & (int(channel_flag::F_WRITE_ERROR) | int(channel_flag::F_READ_ERROR)));
			_ch_do_close_read_write();
		} else if (m_fin_enqueue_done == false) {
			NETP_ASSERT((m_chflag&(int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_WRITE_SHUTDOWN_PENDING))) == 0);

			m_fin_enqueue_done = true;
			m_chflag |= int(channel_flag::F_CLOSE_PENDING);
			_write_frame(mux_stream_make_frame(m_id, FRAME_FIN, 0, 0), make_ref<promise<int>>());
			prt = (netp::E_CHANNEL_CLOSING);
			NETP_TRACE_STREAM("[muxs][s%u][fin][close]push_frame by close", m_id);
		} else {
			NETP_ASSERT(m_chflag& int(channel_flag::F_CONNECTED));
			NETP_ASSERT(m_fin_enqueue_done == true);
			NETP_ASSERT(m_outlets_q.size() == 0);
			_ch_do_close_read_write();
			NETP_TRACE_STREAM("[muxs][s%u][%u]ch_close_impl done", m_id, m_chflag);
		}

		if (closep) { closep->set(prt); }
	}

	void mux_stream::_frame_arrive(mux_stream_frame_flag_t flag, u32_t wndinc, NRP<netp::packet> const& data) {
		NETP_ASSERT(L->in_event_loop());
		switch (flag) {
		case mux_stream_frame_flag::FRAME_DATA:
		{
			if (m_chflag&(int(channel_flag::F_FIN_RECEIVED)|int(channel_flag::F_READ_SHUTDOWN))) {
				m_rcv_data_inc += u32_t(data->len());

				//this might be the last reply from local to remote
				m_transport_mux->__do_mux_write(mux_stream_make_frame(m_id, FRAME_RST, 0, 0), netp::make_ref<netp::promise<int>>());
				NETP_TRACE_STREAM("[muxs][s%u][data]nbytes, fin received already,ignore data, and reply a rst", m_id, data->len());
				break;
			}
			NETP_ASSERT((m_chflag & int(channel_flag::F_READ_SHUTDOWNING)) == 0);

		_BEGIN:
			if (!(m_chflag & int(channel_flag::F_WATCH_READ))) {
				m_incomes_buffer_q.push(data);
				NETP_TRACE_STREAM("[muxs][s%u]incoming: %u, buffer to q", m_id, data->len());
				break;
			}
			if (m_incomes_buffer_q.size()) {
				m_rcv_data_inc += u32_t(m_incomes_buffer_q.front()->len());
				ch_fire_read(m_incomes_buffer_q.front());
				m_incomes_buffer_q.pop();

				if (m_incomes_buffer_q.size() == 0) {
					std::queue<NRP<netp::packet>>().swap(m_incomes_buffer_q);
				}
				goto _BEGIN;
			}
			m_rcv_data_inc += u32_t(data->len());
			ch_fire_read(data);
		}
		break;
		case mux_stream_frame_flag::FRAME_FIN:
		{
			NETP_TRACE_STREAM("[muxs][s%u][fin]recv", ch_id());
			NETP_ASSERT(data->len() == 0);
			m_chflag |= int(channel_flag::F_FIN_RECEIVED);
			ch_close_read_impl(nullptr);
		}
		break;
		case mux_stream_frame_flag::FRAME_UWND:
		{
			NETP_ASSERT(data->len() == 0);
		}
		break;
		case mux_stream_frame_flag::FRAME_RST:
		{
			NETP_ASSERT(data->len() == 0);
			NETP_TRACE_STREAM("[muxs][s%u][rst]recv, force close", ch_id());
			m_chflag |= int(channel_flag::F_READ_ERROR);
			m_chflag &= ~(int(channel_flag::F_BDLIMIT)|int(channel_flag::F_CLOSE_PENDING));
			ch_errno()=(netp::E_MUX_STREAM_RST);
			ch_close_impl(nullptr);
		}
		break;
		}

		_check_snd_dynamic_inc(wndinc);
		_check_rcv_data_inc();
	}

	mux::mux(NRP<io_event_loop> const& L, channel_buf_cfg const& cfg ) :
		channel_handler_abstract(CH_ACTIVITY_CONNECTED|CH_ACTIVITY_WRITE_CLOSED|CH_ACTIVITY_CLOSED|CH_ACTIVITY_ERROR|CH_ACTIVITY_READ_CLOSED | CH_INBOUND_READ),
		m_loop(L),
		m_write_state(mux_transport_write_state::S_CLOSED),
		m_entry_count(0),
		m_frame_data_size_max(u32_t(NETP_MUX_STREAM_FRAME_DATA_DEFAULT_MAX_SIZE)),
		m_buf_cfg(cfg)
	{
		m_entries_writeit = m_entries.before_begin();
		m_entries_lastit_prev = m_entries.before_begin();
		m_entries_lastit = m_entries.begin();
	}

	mux::~mux() {
		//Iterator::_Mproxy should be set nullptr, otherwise we get read : read access violation .
		//forward_list::~forward_list() would result in the pointer of iterator::_Pproxy be deleted
		//reference: _Iterator_base12's impl
		m_entries_writeit = m_entries.end();
		m_entries_lastit_prev = m_entries.end();
		m_entries_lastit = m_entries.end();
	}

	void mux::__do_mux_flush_done( const int rt ) {
		if (m_write_state == mux_transport_write_state::S_CLOSED) {
			return;
		}

		NETP_ASSERT(m_transport_ctx != nullptr);
		m_transport_ctx->L->in_event_loop();
		NETP_ASSERT(m_write_state == mux_transport_write_state::S_WRITING);

		if (rt == netp::OK) {
			NETP_ASSERT(m_entries_lastit != m_entries.end());
			m_entries_lastit->wp->set(netp::OK);
			if (m_entries_lastit == m_entries_writeit) {
				m_entries_writeit = m_entries_lastit_prev;
			}
			m_entries_lastit = m_entries.erase_after(m_entries_lastit_prev);
			--m_entry_count;
			m_write_state = mux_transport_write_state::S_IDLE;
			if (m_entry_count==0) {
				return;
			}
			__do_mux_flush();
		} else {
			NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
			NETP_WARN("[mux][%s]mux write failed: %d, close mux", m_transport_ctx->ch->ch_info().c_str(), rt );
			m_transport_ctx->close();
		}
	}

	void mux::__do_mux_flush() {
		if ( m_write_state != mux_transport_write_state::S_IDLE || m_entries.empty() ) {
			return;
		}

		m_write_state = mux_transport_write_state::S_WRITING;
		NETP_ASSERT(m_transport_ctx->L->in_event_loop());
		NETP_ASSERT(m_transport_ctx != nullptr);
		do {
			if (m_entry_count == 1) {
				m_entries_lastit_prev = m_entries.before_begin();
				m_entries_lastit = m_entries.begin();
				goto _do_write;
			}

			if (m_entries_lastit == m_entries.end()) {
				m_entries_lastit_prev = m_entries.before_begin();
				m_entries_lastit = m_entries.begin();
			}

			while (m_entries_lastit != m_entries.end()) {
				mux_outlet_quota_entry& entry = *m_entries_lastit;
				entry.quota += (m_entry_count<8?8192:4096);
				if (entry.quota >= entry.data->len()) {
					goto _do_write;
				}
				++m_entries_lastit_prev;
				++m_entries_lastit;
			}
		} while (true);

	_do_write:
		NETP_ASSERT(m_entries_lastit != m_entries.end());
		NETP_ASSERT(m_entries_lastit->data != nullptr);
		NRP<netp::promise<int>> wp_ = netp::make_ref<netp::promise<int>>();
		wp_->if_done([mux = NRP<mux>(this), L = m_transport_ctx->L](int const& rt) {
			mux->__do_mux_flush_done(rt);
		});
		m_transport_ctx->write(std::move(wp_), m_entries_lastit->data);
	}

	void mux::__do_mux_write(NRP<netp::packet> const& outlet, NRP<netp::promise<int>> const& wp) {
		if (m_write_state == mux_transport_write_state::S_CLOSED) {
			wp->set(netp::E_CHANNEL_WRITE_CLOSED);
			return;
		}
		m_entries_writeit=m_entries.insert_after(m_entries_writeit,{ outlet,wp,0 });
		++m_entry_count;
		__do_mux_flush();
	}

	void mux::connected(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_transport_ctx == nullptr);
		NETP_ASSERT(m_write_state == mux_transport_write_state::S_CLOSED);
		m_transport_ctx = ctx;
		m_write_state = mux_transport_write_state::S_IDLE;
		NETP_ASSERT(ctx->L->in_event_loop());

		NRP<promise<int>> snd_buf_size_p = ctx->ch->ch_get_write_buffer_size();
		snd_buf_size_p->if_done([mux_=NRP<mux>(this),ctx](int const& size) {
			if (size > 0) {
				if (mux_->m_frame_data_size_max > u32_t(size >> 2)) {
					mux_->m_frame_data_size_max = u32_t(size >> 2);
				}

				mux_->m_frame_data_size_max = NETP_MAX(mux_->m_frame_data_size_max, NETP_MUX_STREAM_FRAME_DATA_DEFAULT_MIN_SIZE );

				mux_->invoke<fn_mux_evt_t>(E_MUX_CH_CONNECTED, mux_);
				//this is the last handler
				//ctx->fire_connected();
			}
		});
	}

	void mux::closed(NRP<netp::channel_handler_context> const& ctx) {
		NETP_INFO("[mux]mux closed: %s", ctx->ch->ch_info().c_str());
		NETP_ASSERT(m_write_state == mux_transport_write_state::S_CLOSED);
		m_transport_ctx = nullptr;
		//m_lastwp = nullptr;
		//cancel all outlet

		for (auto& it : m_entries) {
			it.wp->set(netp::E_MUX_STREAM_TRANSPORT_CLOSED);
		}

		m_entries.clear();
		m_entry_count = 0;
		m_entries_writeit = m_entries.before_begin();
		m_entries_lastit_prev = m_entries.before_begin();
		m_entries_lastit = m_entries.begin();

		stream_map_t::iterator&& streamit = m_stream_map.begin();
		while (streamit != m_stream_map.end()) {
			NRP<mux_stream> s = (streamit++)->second;
			s->m_chflag |= int(channel_flag::F_READ_ERROR);
			s->m_chflag &= ~(int(channel_flag::F_BDLIMIT) | int(channel_flag::F_CLOSE_PENDING));
			s->ch_errno()=(netp::E_MUX_STREAM_TRANSPORT_CLOSED);
			s->ch_close_impl(nullptr);
			NETP_TRACE_STREAM("[mux][s%u][mux_close]mux closed, close mux_stream", s->ch_id());
		}

		invoke<fn_mux_evt_t>(E_MUX_CH_CLOSED, NRP<mux>(this));
		ctx->fire_closed();
	}

	void mux::error(NRP<netp::channel_handler_context> const& ctx, int err) {
		m_write_state = mux_transport_write_state::S_CLOSED;

		invoke<fn_mux_evt_t>(E_MUX_CH_ERROR, NRP<mux>(this));
		ctx->fire_error(err);
	}

	void mux::read_closed(NRP<netp::channel_handler_context> const& ctx) {
		//TODO: flush all stream up and down, then do close
		ctx->fire_read_closed();
		ctx->close_write();
	}

	void mux::write_closed(NRP<netp::channel_handler_context> const& ctx) {
		m_write_state = mux_transport_write_state::S_CLOSED;
		(void)ctx;
	}

	void mux::read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {

		NETP_ASSERT(income != nullptr);
		NETP_ASSERT(ctx == m_transport_ctx);
		//NETP_INFO("[mux]dlen: %u", income->len());

		NETP_ASSERT(income->len() >= NETP_MUX_STREAM_FRAME_HEADER_LEN);
		mux_stream_frame_header* fh = (mux_stream_frame_header*)income->head();
		mux_stream_id_t id = fh->H.id;
		mux_stream_frame_flag_t flag = fh->H.flag;
		u32_t wndinc = fh->H.wnd;
		u32_t dlen = fh->H.dlen;

		if (flag >= mux_stream_frame_flag::FRAME_MUX_STREAM_MESSAGE_TYPE_MAX) {
			NETP_TRACE_STREAM("[muxs][s%u][rst]invalid stream message type, ignore", id);
			__do_mux_write(mux_stream_make_frame(id, FRAME_RST, 0, 0), netp::make_ref<netp::promise<int>>());
			return;
		}

		NETP_ASSERT(id > 0);
		NETP_ASSERT(dlen >= 0);
		income->skip(NETP_MUX_STREAM_FRAME_HEADER_LEN);
		//assert might get failed for multi-packet arrive at the same time
		//such as a fin followed by a rst
		NETP_ASSERT(dlen == income->len());

		NRP<mux_stream> s;
		if (flag == mux_stream_frame_flag::FRAME_SYN) {
			NETP_ASSERT(wndinc > 0);
			NETP_ASSERT(income->len() == sizeof(u32_t));
			u32_t sndwnd = income->read<u32_t>();
			NETP_ASSERT(sndwnd > 0);
			sndwnd = NETP_MIN(sndwnd, m_buf_cfg.sndbuf_size );

			u32_t rcvwnd = NETP_MIN(wndinc, m_buf_cfg.rcvbuf_size);

			int ec;
			std::tie(ec, s) = __do_open_stream(id, { sndwnd,rcvwnd });
			if (ec != netp::OK) {
				NETP_WARN("[mux][s%u][syn]accept stream failed: %d", id, ec);
				__do_mux_write(mux_stream_make_frame(id, FRAME_RST, 0, 0), netp::make_ref<netp::promise<int>>());
				return;
			}

			s->ch_set_connected();
			invoke<fn_mux_stream_accepted_t>(E_MUX_CH_STREAM_ACCEPTED, s);

			NETP_ASSERT(s->m_snd_wnd > 0, "info: %s", s->ch_info().c_str());
			NETP_ASSERT(s->m_rcv_wnd > 0, "info: %s", s->ch_info().c_str());
			s->ch_fire_connected();
			s->ch_io_read();
			return;
		}

		typename stream_map_t::iterator&& it = m_stream_map.find(id);
		if (it != m_stream_map.end()) {
			s = it->second;//keep a copy first, _frame_arrive might trigger map remove
			s->_frame_arrive(flag, wndinc, income);
			return;
		}

		if (flag == mux_stream_frame_flag::FRAME_RST) {
			NETP_TRACE_STREAM("[mux][s%u][rst]stream not found, ignore", id);
			return;
		}

		NETP_TRACE_STREAM("[mux][s%u][%u]stream not found, reply rst, len: %u", id, flag, income->len());
		__do_mux_write(mux_stream_make_frame(id, FRAME_RST, 0, 0), netp::make_ref<netp::promise<int>>());
	}
}}
