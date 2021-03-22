#include <netp/traffic/forwarder_iptcp_payload.hpp>
#include <netp/socket.hpp>

namespace netp { namespace traffic {

	const char* forwarder_iptcp_payload_state_str[(u8_t)forwarder_iptcp_payload_state::IPTCP_FORWARD_STATE_MAX] = {
		"read_dst_port_and_type",
		"read_dst_addr",
		"dial_dst",
		"dst_connected",
		"dial_failed"
	};

	void iptcp_payload_dst_handler::connected(NRP<netp::channel_handler_context> const& ctx) {
		NRP<netp::promise<int>> s = ctx->ch->ch_get_write_buffer_size();
		m_forwarder->m_loop->execute([F=m_forwarder,ctx]() {
			F->_dst_connected(ctx);
		});
	}
	void iptcp_payload_dst_handler::closed(NRP<netp::channel_handler_context > const& ctx) {
		m_forwarder->m_loop->execute([F = m_forwarder, ctx]() {
			F->_dst_closed(ctx);
		});
	}
	void iptcp_payload_dst_handler::read_closed(NRP<netp::channel_handler_context > const& ctx) {
		m_forwarder->m_loop->execute([F = m_forwarder, ctx]() {
			F->_dst_read_closed(ctx);
		});
	}

	void iptcp_payload_dst_handler::read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		m_forwarder->m_loop->execute([F = m_forwarder, ctx,income]() {
			F->_dst_read(ctx, income);
		});
	}

	void forwarder_iptcp_payload::_dst_connected(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_loop->in_event_loop());
		NETP_ASSERT(m_repeater_src_to_dst == nullptr);
		NETP_ASSERT(m_forward_state == forwarder_iptcp_payload_state::DIAL_DST);
		m_forward_state = forwarder_iptcp_payload_state::DST_CONNECTED;

		m_dst_ch = ctx->ch;
		m_repeater_src_to_dst = netp::make_ref<netp::traffic::repeater<NRP<channel_handler_context>>>(m_loop, ctx, (m_src_rcv_wnd<<1));

		m_repeater_src_to_dst->bind<netp::traffic::fn_repeater_event_t>(netp::traffic::repeater_event::e_finished, [dst_ch = ctx->ch]() {
			dst_ch->ch_close_write();
		});

		m_repeater_src_to_dst->bind<netp::traffic::fn_repeater_error_event_t>(netp::traffic::repeater_event::e_write_error, [src_ch_id=m_src_channel_id, src_ch = m_src_ch](int err) {
			NETP_INFO("[forwarder_iptcp_payload][s%u]src to dst error: %d", src_ch_id, err );
			src_ch->ch_close_read();
		});
		m_repeater_src_to_dst->bind<netp::traffic::fn_repeater_event_t>(netp::traffic::repeater_event::e_buffer_full, [src_ch = m_src_ch]() {
			src_ch->ch_aio_end_read();
		});
		m_repeater_src_to_dst->bind<netp::traffic::fn_repeater_event_t>(netp::traffic::repeater_event::e_buffer_empty, [src_ch = m_src_ch]() {
			src_ch->ch_aio_read();
		});

		m_repeater_dst_to_src->bind<netp::traffic::fn_repeater_error_event_t>(netp::traffic::repeater_event::e_write_error, [src_ch_id = m_src_channel_id, dst_ch = ctx->ch](int err) {
			NETP_WARN("[server][s%u]repeater_server_to_stream failed: %d", src_ch_id, err);
			dst_ch->ch_close_read();
		});

		m_repeater_dst_to_src->bind<netp::traffic::fn_repeater_event_t>(netp::traffic::repeater_event::e_buffer_full, [dst_ch = ctx->ch]() {
			dst_ch->ch_aio_end_read();
		});

		m_repeater_dst_to_src->bind<netp::traffic::fn_repeater_event_t>(netp::traffic::repeater_event::e_buffer_empty, [dst_ch = ctx->ch]() {
			dst_ch->ch_aio_read();
		});

		//relay dial ok
		NRP<packet> outp = netp::make_ref<packet>(64);
		outp->write<int32_t>(netp::OK);
		m_repeater_dst_to_src->relay(std::move(outp));

		if (m_first_packet->len()) {
			m_repeater_src_to_dst->relay(m_first_packet);
		}
		m_first_packet = nullptr;

		//stream read close may happen before server connected
		if (m_src_read_closed) {
			m_repeater_src_to_dst->finish();
		}
	}
	void forwarder_iptcp_payload::_dst_closed(NRP<netp::channel_handler_context> const& ctx) {
		NETP_ASSERT(m_loop->in_event_loop());
		(void)ctx;
	}

	void forwarder_iptcp_payload::_dst_read_closed(NRP<netp::channel_handler_context> const& ctx) {
		(void)ctx;
		NETP_ASSERT(m_loop->in_event_loop());
		m_repeater_dst_to_src->finish();
	}

	void forwarder_iptcp_payload::_dst_read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		(void)ctx;
		NETP_ASSERT(m_loop->in_event_loop());
		m_repeater_dst_to_src->relay(income);
	}

	void forwarder_iptcp_payload::_dial_dst() {
		NETP_ASSERT(m_loop->in_event_loop());

		const string_t dialurl =
			(m_dst_address_type == address_type::T_IPV4) ?
			"tcp://" + ipv4todotip(m_dst_ipv4) + ":" + netp::to_string(m_dst_port) :
			"tcp://" + (m_dst_domain)+":" + netp::to_string(m_dst_port);

		NRP<netp::channel_dial_promise> dial_p = netp::make_ref<netp::channel_dial_promise>();
		dial_p->if_done([dialurl,L=m_loop, F=NRP<forwarder_iptcp_payload>(this)]( std::tuple<int, NRP<channel>> const& tupc) {
			L->execute([L, F, rt = std::get<0>(tupc), dialurl]() {
				if (rt != netp::OK) {
					F->m_forward_state = forwarder_iptcp_payload_state::DIAL_FAILED;
					F->m_dial_errno = rt;
					NETP_WARN("[forwarder_iptcp_payload][s%u--%s]dial failed: %d", F->m_src_channel_id, dialurl.c_str(), rt);
					NRP<packet> outp = netp::make_ref<packet>(64);
					outp->write<int32_t>(rt);
					F->m_repeater_dst_to_src->relay(outp);
					F->m_repeater_dst_to_src->finish();
				}
				else { F->m_dial_errno = -99999;/*debug flag*/ }
			});
		});

		NRP<socket_cfg> cfg = netp::make_ref<socket_cfg>();
		cfg->sock_buf = { m_src_snd_wnd, m_src_rcv_wnd };
		netp::do_dial(dialurl.c_str(), dialurl.length(), [F = NRP<forwarder_iptcp_payload>(this)](NRP<netp::channel> const& ch) {
			ch->pipeline()->add_last(netp::make_ref<iptcp_payload_dst_handler>(F));
		},dial_p, std::move(cfg) );
	}

	forwarder_iptcp_payload::forwarder_iptcp_payload() :
		channel_handler_abstract(netp::CH_ACTIVITY_CONNECTED | netp::CH_ACTIVITY_CLOSED | netp::CH_ACTIVITY_READ_CLOSED | netp::CH_INBOUND_READ)
	{}

	void forwarder_iptcp_payload::connected(NRP<netp::channel_handler_context> const& ctx) {
		m_loop = ctx->L;
		m_src_channel_id = ctx->ch->ch_id();

		m_src_ch = ctx->ch;
		m_forward_state = forwarder_iptcp_payload_state::READ_DST_PORT_AND_TYPE;
		m_dial_errno = 0;
		m_src_read_closed = false;

		m_first_packet = netp::make_ref<netp::packet>(16*1024);

		NRP<netp::promise<int>> src_read_wnd_s = ctx->ch->ch_get_read_buffer_size();
		m_src_rcv_wnd = src_read_wnd_s->get();
		NETP_ASSERT(m_src_rcv_wnd > 0);

		NRP<netp::promise<int>> src_write_wnd_s = ctx->ch->ch_get_write_buffer_size();
		m_src_snd_wnd = src_write_wnd_s->get();
		NETP_ASSERT(m_src_snd_wnd>0);

		m_repeater_dst_to_src = netp::make_ref<netp::traffic::repeater<NRP<netp::channel_handler_context>>>(m_loop, ctx, (m_src_snd_wnd<<1) );
		m_repeater_dst_to_src->bind<netp::traffic::fn_repeater_event_t>(netp::traffic::repeater_event::e_finished, [src_ch=ctx->ch]() {
			src_ch->ch_close_write();
		});
	}
	void forwarder_iptcp_payload::closed(NRP<netp::channel_handler_context> const& ctx) {
		(void)ctx;
		NETP_ASSERT(m_src_read_closed == true);
	}
	void forwarder_iptcp_payload::read_closed(NRP<netp::channel_handler_context> const& ctx) {
		(void)ctx;
		m_src_read_closed = true;
		if (m_forward_state < forwarder_iptcp_payload_state::DIAL_DST) {
			m_repeater_dst_to_src->finish();
			return;
		}
		if (m_repeater_src_to_dst != nullptr) {
			m_repeater_src_to_dst->finish();
		}
	}

	void forwarder_iptcp_payload::read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income ) { 

	__check_read_begin:
		switch (m_forward_state) {
		case forwarder_iptcp_payload_state::READ_DST_PORT_AND_TYPE:
			{
				m_first_packet->write(income->head(), income->len());
				income->reset();
				if (m_first_packet->len() < sizeof(u16_t) + sizeof(u8_t) ) {
					return;
				}
				m_dst_port = m_first_packet->read<u16_t>();
				m_dst_address_type = (address_type) m_first_packet->read<u8_t>();
				m_forward_state= forwarder_iptcp_payload_state::READ_DST_ADDR;
				goto __check_read_begin;
			}
			break;
		case forwarder_iptcp_payload_state::READ_DST_ADDR:
			{
				if (income->len()) {
					m_first_packet->write(income->head(), income->len());
					income->reset();
				}
				switch (m_dst_address_type) {
					case address_type::T_DOMAIN:
					{
						if (m_first_packet->len() == 0) { return; }
						const u8_t dlen = *(m_first_packet->head());

						//refer to https://stackoverflow.com/questions/32290167/what-is-the-maximum-length-of-a-dns-name
						if (dlen >= 255 /*DOMAIN MAX LEN*/) {
							NETP_ERR("[server][s%u]domain len exceed %d bytes, close stream, domain name: %s", ctx->ch->ch_id(), E_FORWARDER_DOMAIN_LEN_EXCEED, dlen );
							NRP<netp::packet> outp = netp::make_ref<netp::packet>();
							outp->write<int32_t>(netp::E_FORWARDER_DOMAIN_LEN_EXCEED);
							m_repeater_dst_to_src->relay(outp);
							m_repeater_dst_to_src->finish();
							return;
						}

						if (m_first_packet->len() < dlen) {
							return;
						}

						m_dst_domain = string_t((char*) (m_first_packet->head() + 1), dlen );
						m_first_packet->skip(1+dlen);
						m_forward_state = forwarder_iptcp_payload_state::DIAL_DST;
						_dial_dst();
					}
					break;
					case address_type::T_IPV4:
					{
						if (m_first_packet->len() < sizeof(ipv4_t)) { return; }
						ipv4_t ipv4 = m_first_packet->read<ipv4_t>();
						if (ipv4 == 0) {
							NETP_ERR("[forwarder_iptcp_payload][s%u]ipv4==0, close", m_src_channel_id );
							NRP<netp::packet> outp = netp::make_ref<netp::packet>();
							outp->write<int32_t>(netp::E_FORWARDER_INVALID_IPV4);
							m_repeater_dst_to_src->relay(outp);
							m_repeater_dst_to_src->finish();
							return;
						}

						m_dst_ipv4=(ipv4);
						m_forward_state = forwarder_iptcp_payload_state::DIAL_DST;
						_dial_dst();
					}
					break;
					case address_type::T_IPV6:
					{
						NETP_TODO("todo");
					}
					break;
					default:
					{
						NETP_ERR("[forwarder_iptcp_payload][s%u]invalid address type, close", m_src_channel_id );
						NRP<netp::packet> outp = netp::make_ref<netp::packet>();
						outp->write<int32_t>(-1);
						m_repeater_dst_to_src->relay(outp);
						m_repeater_dst_to_src->finish();
						return;
					}
				}
			}
			break;
		case forwarder_iptcp_payload_state::DIAL_DST:
			{
				m_first_packet->write(income->head(), income->len());
			}
			break;
		case forwarder_iptcp_payload_state::DST_CONNECTED:
			{
				NETP_ASSERT(m_repeater_src_to_dst != nullptr);
				m_repeater_src_to_dst->relay(income);
			}
			break;
		}
	}

}}