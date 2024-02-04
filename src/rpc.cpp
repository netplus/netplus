#include <netp/socket.hpp>
#include <netp/handler/hlen.hpp>

#include <netp/rpc.hpp>

namespace netp {

	std::atomic<netp::u32_t> rpc_message::__rpc_message_id__{1};

	void rpc_message::encode(NRP<netp::packet>& outp) {
		u32_t outp_len = sizeof(u32_t) * 3 + 1;
		if (data != nullptr) {
			outp_len += data->len();
		}

		NRP<netp::packet> _outp = netp::make_ref<netp::packet>(outp_len);
		_outp->write<netp::u8_t>((u8_t)type & 0xFF);
		_outp->write<netp::u32_t>(id);
		_outp->write<netp::i32_t>(code);
		if (data != nullptr) {
			_outp->write<u32_t>(data->len()&0xFFFFFFFF);
			_outp->write(data->head(), data->len());
		}
		outp = _outp;
	}

	 int rpc_message::from_packet(NRP<netp::packet> const& inpack, NRP<rpc_message>& in_rpcm) {
		NETP_ASSERT(inpack != nullptr);
		if (inpack->len() < (sizeof(u8_t) + sizeof(u32_t) + sizeof(i32_t))) {
			return netp::E_RPC_MESSAGE_DECODE_FAILED;
		}
		netp::u8_t t = inpack->read<netp::u8_t>();
		netp::u32_t id = inpack->read<netp::u32_t>();
		int code = inpack->read<netp::i32_t>();
		u32_t plen = 0;
		if (inpack->len()) {
			if (inpack->len() < sizeof(netp::u32_t)) {
				return netp::E_RPC_MESSAGE_DECODE_FAILED;
			}
			plen = inpack->read<netp::u32_t>();
			if (inpack->len() < plen) {
				return netp::E_RPC_MESSAGE_DECODE_FAILED;
			}
		}

		in_rpcm = netp::make_ref<rpc_message>((rpc_message_type)t, id, code, netp::make_ref<netp::packet>(inpack->head(), plen));
		inpack->skip(plen);
		return netp::OK;
	}

	void rpc::_do_reply(NRP<netp::rpc_message> const& reply) {
		NETP_ASSERT(m_loop->in_event_loop());

		TRACE_RPC("[rpc]call done, id: %u, call rt: %d, data len: %u", reply->id, reply->code, reply->data == nullptr ? 0 : reply->data->len());
		if(m_wstate ==rpc_write_state::S_WRITE_CLOSED) {
			NETP_WARN("[rpc]call done, id: %u, call rt: %d, data len: %u, down stream closed", reply->id, reply->code, reply->data == nullptr ? 0 : reply->data->len());
			return;
		}

		m_reply_q.push_back(reply);
		_do_flush();
	}

	//for a one by one write call, we would not get E_CHANNEL_WRITE_BLOCK
	void rpc::_do_reply_done(int rt) {
		NETP_ASSERT(m_loop->in_event_loop());

		if (m_wstate == rpc_write_state::S_WRITE_CLOSED) { return; }

		NETP_ASSERT(m_reply_q.size());
		NRP<rpc_message>& _reply = m_reply_q.front();
		if (rt == netp::OK) {
			TRACE_RPC("[rpc]reply ok, write rt: %d, id: %d, call rt: %d, reply data len: %u", rt, _reply->id, _reply->code, _reply->data == nullptr ? 0 : _reply->data->len());
			m_wstate = rpc_write_state::S_WRITE_IDLE;
			m_reply_q.pop_front();
			if (m_reply_q.empty()) {
				rpc_message_reply_queue_t().swap(m_reply_q);
			}
			_do_flush();
		} else {
			NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
			NETP_ASSERT(m_ctx != nullptr);
			NETP_ERR("[rpc]reply failed, write rt: %d, id: %d, call rt: %d, reply data len: %u",rt, _reply->id, _reply->code, _reply->data == nullptr ? 0 : _reply->data->len() );
			m_ctx->close();
		}
	}

	void rpc::_do_write_req_done(list_req_message* lrm, int rt) {
		NETP_ASSERT(m_loop->in_event_loop());

		if(m_wstate == rpc_write_state::S_WRITE_CLOSED) {return;}
		NETP_ASSERT(lrm->state == rpc_req_message_state::S_WRITING);

		TRACE_RPC("[rpc]write ok, write rt: %d, type: %d, id: %d, data len: %u", rt, _req->m->type, _req->m->id, _req->m->data == nullptr ? 0 : _req->m->data->len());
		if (lrm->m->type == rpc_message_type::T_REQ) {
			lrm->state = rpc_req_message_state::S_WAIT_RESPOND;
		} else {
			lrm->state = rpc_req_message_state::S_WRITE_DONE;
			NETP_ASSERT(lrm->m->type == rpc_message_type::T_DATA);
			lrm->pushp->set(rt);
			netp::allocator<list_req_message>::trash(lrm);
		}

		if (rt == netp::OK) {
			m_wstate = rpc_write_state::S_WRITE_IDLE;
			_do_flush();
		} else {
			NETP_ASSERT(m_ctx != nullptr);
			NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
			NETP_ERR("[rpc]write req failed, write rt: %d, id: %d, data len: %u", rt, lrm->m->id, lrm->m->data == nullptr ? 0 : lrm->m->data->len());
			m_ctx->close();
		}

	}

	void rpc::_do_flush() {
		NETP_ASSERT(m_loop->in_event_loop());
		NETP_ASSERT((m_wstate != rpc_write_state::S_WRITE_CLOSED) ) ;
		if (m_wstate != rpc_write_state::S_WRITE_IDLE ) {
			return;
		}

		NETP_ASSERT(m_ctx != nullptr);
		while (!m_reply_q.empty()) {
			m_wstate = rpc_write_state::S_WRITING;
			NRP<rpc_message>& reply_r = m_reply_q.front();
			NRP<netp::packet> outp;
			reply_r->encode(outp);

			NRP<netp::promise<int>> wp = netp::make_ref<netp::promise<int>>();
			wp->if_done([R = NRP<netp::rpc>(this)](int const& rt) {
				R->_do_reply_done(rt);
			});

			m_ctx->write(wp, outp);
			return;
		}

		while (!NETP_LIST_IS_EMPTY(&m_list_to_write)) {
			//the first one
			list_req_message* lrm = m_list_to_write.next;
			NETP_ASSERT(lrm->state == rpc_req_message_state::S_WAIT_WRITE);
			netp::list_delete(lrm);
			--m_list_to_write_count;

			lrm->state = rpc_req_message_state::S_WRITING;
			if (lrm->m->type == rpc_message_type::T_REQ) {
				netp::list_append(&m_list_wait_for_response[NETP_RPC_INFLIGHT_SLOT(lrm->m->id)], lrm);
				++m_list_wait_for_response_count;
			}

			NRP<netp::packet> outp;
			lrm->m->encode(outp);
			NRP<netp::promise<int>> wp = netp::make_ref<netp::promise<int>>();
			wp->if_done([R = NRP<netp::rpc>(this),lrm](int const& rt) {
				R->_do_write_req_done(lrm, rt);
			});
			m_wstate = rpc_write_state::S_WRITING;
			m_ctx->write(wp, outp);
			return;
		}
	}

	void rpc::_timer_timeout(NRP<netp::timer> const& t) {
		_do_timer_timeout();
		if (m_wstate != rpc_write_state::S_WRITE_CLOSED) {
			m_loop->launch(t, netp::make_ref<promise<int>>());
			return;
		}
	}

	void rpc::_do_timer_timeout() {
		NETP_ASSERT(m_loop->in_event_loop());

		const timer_timepoint_t now = timer_clock_t::now();
		list_req_message *cur, *nxt;
		for (size_t i = 0; i < sizeof(m_list_wait_for_response) / sizeof(m_list_wait_for_response[i]); ++i) {
			NETP_LIST_SAFE_FOR(cur, nxt, &m_list_wait_for_response[i]) {
				if (now > cur->tp_timeout) {
					NETP_ASSERT(cur->m != nullptr);
					NETP_ASSERT(cur->m->type == rpc_message_type::T_REQ);
					NETP_ASSERT(cur->state == rpc_req_message_state::S_WAIT_RESPOND);

					cur->state = rpc_req_message_state::S_TIMEOUT;
					cur->callp->set(std::make_tuple(netp::E_RPC_CALL_TIMEOUT, nullptr));
					NETP_WARN("[rpc]req timeout, id: %d, api code: %d, data len: %u", cur->m->id, cur->m->code, cur->m->data == nullptr ? 0 : cur->m->data->len());

					netp::list_delete(cur);
					--m_list_wait_for_response_count;
					netp::allocator<list_req_message>::trash(cur);
				}
			}
		}

		NETP_LIST_SAFE_FOR(cur, nxt, &m_list_to_write) {
			if (now > cur->tp_timeout) {
				NETP_ASSERT( cur->state == rpc_req_message_state::S_WAIT_WRITE);
				cur->state = rpc_req_message_state::S_TIMEOUT;
				if (cur->m->type == rpc_message_type::T_REQ) {
					cur->callp->set(std::make_tuple(netp::E_RPC_WRITE_TIMEOUT,nullptr));
				} else {
					NETP_ASSERT(cur->m->type == rpc_message_type::T_DATA);
					cur->pushp->set(netp::E_RPC_WRITE_TIMEOUT);
				}
				NETP_WARN("[rpc]write timeout, id: %d, data len: %u", cur->m->id, cur->m->data == nullptr ? 0 : cur->m->data->len() );
				netp::list_delete(cur);
				--m_list_to_write_count;
				netp::allocator<list_req_message>::trash(cur);
			}
		}
	}

	void rpc::_do_close(NRP<netp::promise<int>> const& tf) {
		NETP_ASSERT(m_loop->in_event_loop());

		NETP_ASSERT(m_ctx != nullptr);
		m_ctx->close(tf);
	}

	void rpc::_do_call(NRP<netp::rpc_call_promise> const& callp, int api_id, NRP<netp::packet> const& data, netp::timer_duration_t const& timeout) {

		NETP_ASSERT(m_loop->in_event_loop());
		if (m_wstate == rpc_write_state::S_WRITE_CLOSED) {
			callp->set(std::make_tuple(netp::E_RPC_NO_WRITE_CHANNEL, nullptr));
			return;
		}

		if ((m_list_to_write_count + m_list_wait_for_response_count) >= NETP_RPC_INFLIGHT_MAX) {
			callp->set(std::make_tuple(netp::E_CHANNEL_WRITE_BLOCK, nullptr));
			return;
		}

		NRP<netp::rpc_message> m = netp::make_ref<netp::rpc_message>(netp::rpc_message_type::T_REQ, api_id, data);
		list_req_message* lrm = netp::allocator<list_req_message>::make();
		lrm->state = netp::rpc_req_message_state::S_WAIT_WRITE;
		lrm->m = m;
		lrm->callp = callp;
		lrm->tp_timeout = (timer_clock_t::now() + timeout);

		netp::list_append(&m_list_to_write, lrm);
		++m_list_to_write_count;
		
		_do_flush();
	}

	void rpc::_do_push(NRP<netp::rpc_push_promise> const& pushp, NRP<netp::packet> const& data,  timer_duration_t const& timeout) {
		NETP_ASSERT(m_loop->in_event_loop());

		if (m_wstate == rpc_write_state::S_WRITE_CLOSED) {
			pushp->set(netp::E_RPC_NO_WRITE_CHANNEL);
			return;
		}

		if ((m_list_to_write_count + m_list_wait_for_response_count) >= NETP_RPC_INFLIGHT_MAX) {
			pushp->set(netp::E_CHANNEL_WRITE_BLOCK);
			return;
		}

		NETP_ASSERT(data->len());
		NRP<netp::rpc_message> m = netp::make_ref<netp::rpc_message>(netp::rpc_message_type::T_DATA, 0, data);
		list_req_message* lrm = netp::allocator<list_req_message>::make();
		lrm->state = netp::rpc_req_message_state::S_WAIT_WRITE;
		lrm->m = m;
		lrm->pushp = pushp;
		lrm->tp_timeout = (timer_clock_t::now() + timeout);
		netp::list_append(&m_list_to_write, lrm);
		++m_list_to_write_count;
		
		_do_flush();
	}

	void rpc::connected(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_loop->in_event_loop());
		m_ctx = ctx;

		NETP_ASSERT(m_wstate == rpc_write_state::S_WRITE_CLOSED);
		m_wstate = rpc_write_state::S_WRITE_IDLE;
		m_close_promise = netp::make_ref<promise<int>>();
		invoke<fn_rpc_activity_notify_t>(E_RPC_CONNECTED,NRP<rpc>(this) );
		unbind(E_RPC_CONNECTED);
		unbind(E_RPC_ERROR);

		NRP<netp::timer> tm_TIMEOUT = netp::make_ref<netp::timer>(std::chrono::seconds(1), &rpc::_timer_timeout, NRP<rpc>(this), std::placeholders::_1 ) ;
		m_loop->launch(tm_TIMEOUT,netp::make_ref<promise<int>>());
	}

	void rpc::closed(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_loop->in_event_loop());
		TRACE_RPC("[rpc][#%u]rpc closed", ctx->ch->ch_id());
		(void)ctx;

		NETP_ASSERT(m_wstate == netp::rpc_write_state::S_WRITE_CLOSED);

		while (m_reply_q.size()) {
			NRP<netp::rpc_message>& reply = m_reply_q.front();
			NETP_WARN("[rpc]cancel reply, id: %d, code: %d, nbytes: %d", reply->id, reply->code, reply->data->len());
			m_reply_q.pop_front();
		}

		list_req_message *cur, *nxt;
		NETP_LIST_SAFE_FOR(cur, nxt, &m_list_to_write) {
			if (cur->m->type == rpc_message_type::T_REQ) {
				cur->callp->set(std::make_tuple(netp::E_RPC_CALL_CANCEL, nullptr));
			} else {
				NETP_ASSERT(cur->m->type == rpc_message_type::T_DATA);
				cur->pushp->set(netp::E_RPC_CALL_CANCEL);
			}
			--m_list_to_write_count;
			netp::list_delete(cur);
			netp::allocator<list_req_message>::trash(cur);
		}

		for (size_t i = 0; i < sizeof(m_list_wait_for_response) / sizeof(m_list_wait_for_response[0]); ++i) {
			NETP_LIST_SAFE_FOR(cur, nxt, &m_list_wait_for_response[i]) {
				cur->callp->set(std::make_tuple(netp::E_RPC_CALL_TIMEOUT, nullptr));
				--m_list_wait_for_response_count;
				netp::list_delete(cur);
				netp::allocator<list_req_message>::trash(cur);
			}
		}

		m_fn_on_push = nullptr;
		m_ctx = nullptr;
		m_close_promise->set(netp::OK);
	}

	void rpc::error(NRP<netp::channel_handler_context> const& ctx, int err) {
		NETP_ASSERT(m_loop->in_event_loop());
		NETP_ERR("[rpc][#%u]rpc error: %d", ctx->ch->ch_id(), err );
		NETP_ASSERT(m_close_promise == nullptr);
		invoke<fn_rpc_activity_notify_error_t>(E_RPC_ERROR, NRP<rpc>(this), err );
		unbind(E_RPC_CONNECTED);
		unbind(E_RPC_ERROR);
		ctx->close();
	}

	void rpc::read_closed(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_loop->in_event_loop());
		TRACE_RPC("[rpc][#%u]read closed, close ch", ctx->ch->ch_id());
		ctx->close();
	}
	void rpc::write_closed(NRP<netp::channel_handler_context> const& ctx) {
		TRACE_RPC("[rpc][#%u]write closed, close ch", ctx->ch->ch_id());
		NETP_ASSERT(m_wstate != netp::rpc_write_state::S_WRITE_CLOSED);
		m_wstate = netp::rpc_write_state::S_WRITE_CLOSED;
		ctx->close();
	}

	void rpc::read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		NETP_ASSERT(m_loop->in_event_loop());
		NRP<rpc_message> in;
		int rt = rpc_message::from_packet(income,in);

		if (NETP_UNLIKELY(rt != netp::OK)) {
			NETP_WARN("[rpc][%s]invalid rpc message parsed, close rpc", ctx->ch->ch_info().c_str() );
			ctx->close();
			return;
		}

		NETP_ASSERT(in != nullptr);

		switch (in->type) {
		case rpc_message_type::T_REQ:
		{
			TRACE_RPC("[rpc]call in, id: %u, api code: %d, data len: %u", in->id, in->code, in->data == nullptr ? 0 : in->data->len());
			NRP<rpc_message> r = netp::make_ref<rpc_message>(netp::rpc_message_type::T_RESP, in->id);
			try {
				NRP<netp::rpc_call_promise> f = netp::make_ref<netp::rpc_call_promise>();
				f->if_done([r, rpc_=(this)]( std::tuple<int, NRP<packet>> const& tupp) {
					r->code = std::get<0>(tupp);
					r->data = std::get<1>(tupp);
					rpc_->_do_reply(r);
				});
				invoke<fn_rpc_call_t>(in->code, NRP<netp::rpc>(this), in->data, f );
				return;
			} catch (netp::exception& e) {
				r->code = e.code();
				NETP_ERR("[rpc]call in, netp::exception: [%d]%s\n%s(%d) %s\n%s", e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			} catch (std::exception& e) {
				r->code = netp::E_RPC_CALL_ERROR_UNKNOWN;
				NETP_ERR("[rpc]call in, std::exception: %s", e.what() );
			} catch (...) {
				NETP_ERR("[rpc]call in, unknown exception");
				r->code = netp::E_RPC_CALL_ERROR_UNKNOWN;
			}
			_do_reply(r);
		}
		break;
		case rpc_message_type::T_RESP:
		{
			list_req_message* lrm_slot = &m_list_wait_for_response[NETP_RPC_INFLIGHT_SLOT(in->id)];
			list_req_message* lrm_waiting_reply = 0;
			NETP_LIST_FOR(lrm_waiting_reply, lrm_slot) {
				if (lrm_waiting_reply->m->id == in->id) {
					break;
				}
			}
			if (lrm_waiting_reply == 0 ) {
				NETP_INFO("[rpc]unknown resp in, id: %u, api code: %d, data len: %u", in->id, in->code, in->data == nullptr ? 0 : in->data->len());
				return;
			}
			netp::list_delete(lrm_waiting_reply);
			--m_list_wait_for_response_count;

			NETP_ASSERT(lrm_waiting_reply->state == rpc_req_message_state::S_WAIT_RESPOND);
			TRACE_RPC("[rpc]reply in, id: %u, call rt: %d, data len: %u", in->id, in->code, in->data == nullptr ? 0 : in->data->len());
			lrm_waiting_reply->state = rpc_req_message_state::S_RESPOND;
			try {
				lrm_waiting_reply->callp->set(std::make_tuple(in->code, in->data));
			} catch (netp::exception& e) {
				NETP_ERR("[rpc]on_reply, netp::exception: [%d]%s\n%s(%d) %s\n%s",e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			} catch (std::exception& e) {
				NETP_INFO("[rpc]reply in, id: %u, call rt: %d, data len: %u, std::exception: %s", in->id, in->code, in->data == nullptr ? 0 : in->data->len(), e.what() );
			} catch (...) {
				NETP_INFO("[rpc]reply in, id: %u, call rt: %d, data len: %u, unknown exception", in->id, in->code, in->data == nullptr ? 0 : in->data->len());
			}

			netp::allocator<list_req_message>::trash(lrm_waiting_reply);
		}
		break;
		case rpc_message_type::T_DATA:
		{
			TRACE_RPC("[rpc]data in, id: %u, data len: %u", in->id, in->data == nullptr ? 0 : in->data->len());
			if (m_fn_on_push == nullptr) {
				NETP_WARN("[rpc]data in, id: %u, data len : %u, no on_read cb", in->id, in->data == nullptr ? 0 : in->data->len());
				return ;
			}

			try {
				m_fn_on_push(NRP<netp::rpc>(this), in->data);
			} catch (netp::exception& e) {
				NETP_ERR("[rpc]data in, netp::exception: [%d]%s\n%s(%d) %s\n%s", e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			} catch (std::exception& e) {
				NETP_ERR("[rpc]data in, id: %u, call rt: %d, data len: %u, std::exception: %s", in->id, in->code, in->data == nullptr ? 0 : in->data->len(), e.what());
			} catch (...) {
				NETP_ERR("[rpc]data in, id: %u, call rt: %d, data len: %u, unknown exception", in->id, in->code, in->data == nullptr ? 0 : in->data->len());
			}
		}
		break;
		default:
		{
			NETP_WARN("[rpc]unknown rpc_message_type: %d, data len: %u", in->type, in->data == nullptr ? 0 : in->data->len() );
		}
		break;
		}
	}

	rpc::rpc(NRP<netp::event_loop> const& L) :
		channel_handler_abstract(netp::CH_ACTIVITY | netp::CH_INBOUND_READ),
		m_loop(L),
		m_wstate(rpc_write_state::S_WRITE_CLOSED),
		m_fn_on_push(nullptr),
		m_list_to_write_count(0),
		m_list_wait_for_response_count(0)
	{
		netp::list_init(&m_list_to_write);
		for (size_t i = 0; i < sizeof(m_list_wait_for_response) / sizeof(m_list_wait_for_response[0]); ++i) {
			netp::list_init(&m_list_wait_for_response[i]);
		}
	}

	rpc::~rpc()
	{
		NETP_ASSERT(m_list_to_write_count == 0 && m_list_wait_for_response_count == 0);
	}

	void rpc::on_push(fn_on_push_t const& fn) {
		NETP_ASSERT(m_fn_on_push == nullptr);
		m_fn_on_push = fn;
	}

	void rpc::operator>>(fn_on_push_t const& fn) {
		on_push(fn);
	}

	NRP<netp::promise<int>> rpc::close() {
		NRP<netp::promise<int>> tf = netp::make_ref<netp::promise<int>>();
		m_loop->execute([R = NRP<netp::rpc>(this), tf](){
			R->_do_close(tf);
		});
		return tf;
	}

	netp::fn_channel_initializer_t rpc::__decorate_initializer(fn_rpc_activity_notify_t const& fn_notify_connected, fn_rpc_activity_notify_error_t const& fn_notify_err, netp::fn_channel_initializer_t const& fn_ch_initializer ) {

		return [fn_notify_connected, fn_notify_err, fn_ch_initializer](NRP<netp::channel> const& ch) {
			if (fn_ch_initializer != nullptr) {
				fn_ch_initializer(ch);
			}

			NRP<netp::channel_handler_abstract> h_hlen = netp::make_ref<netp::handler::hlen>();
			ch->pipeline()->add_last(h_hlen);

			NRP<netp::rpc> rpc = netp::make_ref<netp::rpc>(ch->L);
			if (fn_notify_err != nullptr) {
				rpc->bind<fn_rpc_activity_notify_error_t>(E_RPC_ERROR, fn_notify_err);
			}
			rpc->bind<fn_rpc_activity_notify_t>(E_RPC_CONNECTED,fn_notify_connected);
			ch->pipeline()->add_last(rpc);
		};
	}

	//example:
	//tcp://127.0.0.1:31000
	NRP<rpc_dial_promise> rpc::dial(const char* host, size_t len, netp::fn_channel_initializer_t const& fn_ch_initializer , NRP<socket_cfg> const& cfg ) {
		NRP<netp::rpc_dial_promise> rdf = netp::make_ref<netp::rpc_dial_promise>();
		NRP<netp::channel_dial_promise> ch_df = netp::make_ref<netp::channel_dial_promise>();
		ch_df->if_done([rdf](std::tuple<int, NRP<netp::channel>> const& tupc) {
			if ( std::get<0>(tupc) != netp::OK) {
				rdf->set(std::make_tuple(std::get<0>(tupc),nullptr));
			}
		});

		auto fn_notify_connected = [rdf](NRP<netp::rpc> const& rpc_) {
			if (rdf != nullptr) {
				NETP_ASSERT(rpc_ != nullptr);
				rdf->set(std::make_tuple(netp::OK,rpc_));
			}
		};

		fn_rpc_activity_notify_error_t fn_notify_err = [rdf](NRP<netp::rpc> const&, int err ) {
			rdf->set(std::make_tuple(err, nullptr));
		};

		netp::do_dial(ch_df, host, len, __decorate_initializer(fn_notify_connected, fn_notify_err, fn_ch_initializer), cfg);
		return rdf;
	}

	NRP<rpc_dial_promise> rpc::dial(std::string const& host, netp::fn_channel_initializer_t const& fn_ch_initializer, NRP<socket_cfg> const& cfg) {
		return dial(host.c_str(), host.length(), fn_ch_initializer, cfg);
	}

	NRP<rpc_listen_promise> rpc::listen(std::string const& host, fn_rpc_activity_notify_t const& fn_accepted, netp::fn_channel_initializer_t const& fn_ch_initializer, NRP<socket_cfg> const& cfg ) {
		return netp::listen_on(host.c_str(), host.length(), __decorate_initializer(fn_accepted, nullptr, fn_ch_initializer), cfg);
	}
}