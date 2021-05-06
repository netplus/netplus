#include <netp/handler/websocket.hpp>

#include <netp/logger_broker.hpp>
#include <netp/channel_handler_context.hpp>
#include <netp/channel.hpp>

#ifdef NETP_WITH_BOTAN

#include <botan/hash.h>
#include <botan/base64.h>

#define _H_Upgrade "Upgrade"
#define _H_Connection "Connection"
#define _H_SEC_WEBSOCKET_VERSION "Sec-WebSocket-Version"
#define _H_SEC_WEBSOCKET_KEY "Sec-WebSocket-Key"
#define _H_SEC_WEBSOCKET_ACCEPT "Sec-WebSocket-Accept"
#define _H_WEBSOCKET_SERVER "WebSocket-Server"

#define _WEBSOCKET_UUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

namespace netp { namespace handler {

	static const char* WEBSOCKET_UPGRADE_REPLY_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";

	void _sha1(const char* src, u32_t len, unsigned char out[20] ) {
		std::unique_ptr<Botan::HashFunction> hasher(Botan::HashFunction::create( std::string("SHA-1")));
		hasher->update((const uint8_t*)src, len);
		hasher->final(out);
	}

	void websocket::connected(NRP<channel_handler_context> const& ctx) {
		m_income_prev = netp::make_ref<packet>();
		m_tmp_frame = netp::make_shared<ws_frame>();
		m_tmp_frame->appdata = netp::make_ref<packet>();
		m_tmp_frame->extdata = netp::make_ref<packet>();

		m_tmp_message = netp::make_ref<packet>();

		if (m_type == websocket_type::T_SERVER) {
			m_state = state::S_WAIT_CLIENT_HANDSHAKE_REQ;
		} else {
			NETP_THROW("TOIM");
		}

		ctx->ch->ch_set_nodelay();
	}

	void websocket::closed(NRP<channel_handler_context> const& ctx) {
		if (m_http_parser != nullptr) {
			m_http_parser->reset();
			m_http_parser = nullptr;
		}
	}

	void websocket::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income)
	{
		if (income->len() == 0) {
			NETP_WARN("[websocket][#%u]empty message, return", ctx->ch->ch_id() );
			return;
		}

		if ( m_income_prev->len() ) {
			income->write_left(m_income_prev->head(), m_income_prev->len());
			m_income_prev->reset();
		}

		int ec = 0;
_CHECK:
		switch (m_state) {

		case state::S_WAIT_CLIENT_HANDSHAKE_REQ:
			{
				NETP_ASSERT( m_http_parser == nullptr );
				m_http_parser = netp::make_ref<netp::http::parser>();
				m_http_parser->init(netp::http::HPT_REQ);
				
				m_http_parser->on_headers_complete = std::bind(&websocket::http_on_headers_complete, NRP<websocket>(this), std::placeholders::_1, std::placeholders::_2);
				m_http_parser->on_body = std::bind(&websocket::http_on_body, NRP<websocket>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
				m_http_parser->on_message_complete = std::bind(&websocket::http_on_message_complete, NRP<websocket>(this), std::placeholders::_1);

				m_state = state::S_HANDSHAKING;
				goto _CHECK;
			}
		break;
		case state::S_HANDSHAKING:
			{
				size_t nparsed = m_http_parser->parse( (char*) income->head(), income->len(), ec );
				if(ec != netp::OK) {
					NETP_WARN("[websocket][#%u]handshake, parse handshake message failed: %d", ctx->ch->ch_id(), ec );
					ctx->close();
					return ;
				}

				//NETP_ASSERT(nparsed == income->len());
				income->skip(nparsed);
				goto _CHECK;
			}
			break;
		case state::S_UPGRADE_REQ_MESSAGE_DONE:
			{
				NETP_ASSERT(m_upgrade_req != nullptr);
				if (!m_upgrade_req->H->have(_H_Upgrade) || netp::strcmp("websocket", m_upgrade_req->H->get(_H_Upgrade).c_str() ) != 0 ) {
					NETP_WARN("[websocket]missing %s, or not websocket, force close", _H_Upgrade);
					NRP<packet> out = netp::make_ref<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, netp::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->close();
					return ;
				}

				if (!m_upgrade_req->H->have(_H_Connection) || netp::strpos(m_upgrade_req->H->get(_H_Connection).c_str(), "Upgrade" ) == -1 ) {
					NETP_WARN("[websocket]missing %s, or not Upgrade, force close", _H_Connection);
					NRP<packet> out = netp::make_ref<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, netp::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->close();
					return;
				}

				if (!m_upgrade_req->H->have(_H_SEC_WEBSOCKET_VERSION)) {
					NETP_WARN("[websocket]missing Sec-WebSocket-Key, force close");
					NRP<packet> out = netp::make_ref<packet>();
					out->write((byte_t*) WEBSOCKET_UPGRADE_REPLY_400, netp::strlen(WEBSOCKET_UPGRADE_REPLY_400) );
					ctx->write(out);
					ctx->close();
					return;
				}

				if (!m_upgrade_req->H->have(_H_SEC_WEBSOCKET_KEY)) {
					NETP_WARN("[websocket]missing Sec-WebSocket-Version, force close");
					NRP<packet> out = netp::make_ref<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, netp::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->close();
					return;
				}

				string_t skey = m_upgrade_req->H->get(_H_SEC_WEBSOCKET_KEY) + _WEBSOCKET_UUID;
				unsigned char sha1key[20];
				_sha1(skey.c_str(), (u32_t)skey.length(), sha1key);

				char xx[1024] = {0};
				::size_t input_l;
				::size_t nbytes = Botan::base64_encode(xx, sha1key, 20, input_l, true);

				NRP<netp::http::message> reply = netp::make_ref<netp::http::message>();
				reply->H = netp::make_ref<netp::http::header>();
				reply->type = netp::http::T_RESP;
				reply->code = 101;
				reply->status = "Switching Protocols";
				reply->ver = netp::http::version{ 1,1 };
				
				reply->H->set(_H_Upgrade, "websocket");
				reply->H->set(_H_Connection, "Upgrade");
				reply->H->set(_H_SEC_WEBSOCKET_ACCEPT, string_t(xx,nbytes) );
				reply->H->set(_H_SEC_WEBSOCKET_VERSION, "13" );
				reply->H->set(_H_WEBSOCKET_SERVER, __NETP_VER_STR );

				NRP<packet> outp;
				reply->encode(outp);
				NETP_INFO("reply H: \n%s", string_t((char*)outp->head(), outp->len()).c_str());
				ctx->write(outp);
				m_state = state::S_MESSAGE_BEGIN;
				ctx->fire_connected();
				goto _CHECK;
			}
			break;
		case state::S_MESSAGE_BEGIN:
			{
				NETP_ASSERT(m_tmp_message != nullptr);
				m_state = state::S_FRAME_BEGIN;
				m_fragmented_begin = false;
				goto _CHECK;
			}
			break;
		case state::S_FRAME_BEGIN:
			{
				m_state = state::S_FRAME_READ_H;
				goto _CHECK;
			}
		case state::S_FRAME_READ_H:
			{
				//NETP_INFO("<<< %u", income->len());
				NETP_ASSERT(sizeof(ws_frame::H) == 2, "%u", sizeof(ws_frame::H));
				if (income->len() >= sizeof(ws_frame::H)) {

					m_tmp_frame->H.B1.B = income->read<u8_t>();
					m_tmp_frame->H.B2.B = income->read<u8_t>();

					//@Note: refer to RFC6455
					//frame with a control opcode would not be fragmented
					if ( m_fragmented_begin == false ) {
						NETP_ASSERT( m_tmp_frame->H.B1.Bit.opcode != 0x0);
						m_message_opcode = m_tmp_frame->H.B1.Bit.opcode;
						if (m_tmp_frame->H.B1.Bit.fin == 0) {
							m_fragmented_begin = true;
						}
					}

					m_state = state::S_FRAME_READ_PAYLOAD_LEN;
					goto _CHECK;
				}
				m_income_prev = income;
			}
			break;
		case state::S_FRAME_READ_PAYLOAD_LEN:
			{
				//NETP_INFO("<<< %u", income->len());
				if (m_tmp_frame->H.B2.Bit.len == 126) {
					if (income->len() >= sizeof(u16_t)) {
						m_tmp_frame->payload_len = income->read<u16_t>();
						m_state = state::S_FRAME_READ_MASKING_KEY;
						goto _CHECK;
					}
				} else if (m_tmp_frame->H.B2.Bit.len == 127) {
					if (income->len() >= sizeof(u64_t)) {
						m_tmp_frame->payload_len = income->read<u64_t>();
						m_state = state::S_FRAME_READ_MASKING_KEY;
						goto _CHECK;
					}
				} else {
					m_tmp_frame->payload_len = m_tmp_frame->H.B2.Bit.len;
					m_state = state::S_FRAME_READ_MASKING_KEY;
					goto _CHECK;
				}
				m_income_prev = income;
			}
			break;
		case state::S_FRAME_READ_MASKING_KEY:
			{
				//@refer to https://tools.ietf.org/html/rfc6455#page-27
				//for message from server, mask would be 0

				if (m_type == websocket_type::T_SERVER) {
					if (m_tmp_frame->H.B2.Bit.mask == 0) {
						//invalid request
						close(make_ref<promise<int>>(),ctx);
						m_state = state::S_CLOSED;
						return;
					}

					if (income->len() >= sizeof(u32_t)) {
						const u32_t key = income->read<u32_t>();

						m_tmp_frame->masking_key_arr[0] = (key >> 24) & 0xff;
						m_tmp_frame->masking_key_arr[1] = (key >> 16) & 0xff;
						m_tmp_frame->masking_key_arr[2] = (key >> 8) & 0xff;
						m_tmp_frame->masking_key_arr[3] = (key) & 0xff;

						m_state = state::S_FRAME_READ_PAYLOAD;
						goto _CHECK;
					}
					m_income_prev = income;
				} else {
					m_state = state::S_FRAME_READ_PAYLOAD;
					goto _CHECK;
				}
			}
			break;
		case state::S_FRAME_READ_PAYLOAD:
			{
				const u64_t left_to_fill = m_tmp_frame->payload_len - m_tmp_frame->appdata->len();
				if (income->len() < left_to_fill) {
					m_income_prev = income;
					return;
				}

				if (m_tmp_frame->H.B2.Bit.mask == 0x1) {
					u32_t i = 0;
					while (i < left_to_fill) {
						const u8_t _byte = ( *((netp::u8_t*)income->head() + (i)) ^ m_tmp_frame->masking_key_arr[i % 4]);
						++i;
						m_tmp_frame->appdata->write<u8_t>(_byte);
					}
				} else {
					m_tmp_frame->appdata->write(income->head(), left_to_fill);
				}

				income->skip(left_to_fill);
				m_state = state::S_FRAME_END;
				goto _CHECK;
			}
			break;
		case state::S_FRAME_END:
			{
				//NO EXT SUPPORT NOW
				NETP_ASSERT(m_tmp_frame->H.B1.Bit.rsv1 == 0);
				NETP_ASSERT(m_tmp_frame->H.B1.Bit.rsv2 == 0);
				NETP_ASSERT(m_tmp_frame->H.B1.Bit.rsv3 == 0);

				switch (m_tmp_frame->H.B1.Bit.opcode) {
				case OP_CLOSE:
					{
						//reply a CLOSE, then do ctx->close();
						NETP_DEBUG("<<< op_close");
						m_state = state::S_FRAME_CLOSE_RECEIVED;
						close(make_ref<promise<int>>(),ctx);
					}
					break;
				case OP_PING:
					{
						//reply a PONG
						NSP<ws_frame> _PONG = netp::make_shared<ws_frame>();
						_PONG->H.B1.Bit.fin = 0x1;
						_PONG->H.B1.Bit.rsv1 = 0x0;
						_PONG->H.B1.Bit.rsv2 = 0x0;
						_PONG->H.B1.Bit.rsv3 = 0x0;
						_PONG->H.B1.Bit.opcode = OP_PONG;

						_PONG->H.B2.Bit.mask = 0x0;
						_PONG->H.B2.Bit.len = m_tmp_frame->appdata->len();
						_PONG->appdata = m_tmp_frame->appdata;

						NRP<packet> outp_PONG = netp::make_ref<packet>();
						outp_PONG->write<u8_t>( _PONG->H.B1.B );
						outp_PONG->write<u8_t>( _PONG->H.B2.B);

						outp_PONG->write(m_tmp_frame->appdata->head(), m_tmp_frame->appdata->len());
						m_tmp_frame->appdata->reset();

						ctx->write(outp_PONG);
						m_state = state::S_FRAME_BEGIN;
						goto _CHECK;
					}
					break;
				case OP_PONG:
					{
						//ignore right now
						m_state = state::S_FRAME_BEGIN;
						goto _CHECK;
					}
					break;
				case OP_TEXT:
				case OP_BINARY:
				case OP_CONTINUE:
					{
						NETP_ASSERT(m_tmp_frame->payload_len == m_tmp_frame->appdata->len());
						m_tmp_message->write(m_tmp_frame->appdata->head(), m_tmp_frame->appdata->len());
						m_tmp_frame->appdata->reset();

						if (m_tmp_frame->H.B1.Bit.fin == 0x1) {
							m_state = state::S_MESSAGE_END;
						} else {
							m_state = state::S_FRAME_BEGIN;
						}
						goto _CHECK;
					}
					break;
				case OP_X3:
				case OP_X4:
				case OP_X5:
				case OP_X6:
				case OP_X7:
				case OP_B:
				case OP_C:
				case OP_D:
				case OP_E:
				case OP_F:
					{
						//reply a CLOSE, then do ctx->close();
						NETP_WARN("<<< op_not_supported");
						close(make_ref<promise<int>>(), ctx );
					}
					break;
				default:
					{
						//reply a CLOSE, then do ctx->close();
						NETP_WARN("<<< op_unknown");
						close(make_ref<promise<int>>(),ctx);
					}
				}
			}
			break;
		case state::S_MESSAGE_END:
			{
				NETP_ASSERT(m_message_opcode == OP_BINARY || m_message_opcode == OP_TEXT);
				ctx->fire_read(m_tmp_message);
				m_tmp_message->reset();

				m_state = state::S_MESSAGE_BEGIN;
				goto _CHECK;
			}
			break;
		case state::S_FRAME_CLOSE_RECEIVED:
			{/*exit point*/}
			break;
		}
	}


	void websocket::write(NRP<promise<int>> const& chp,NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {

		NSP<ws_frame> _frame = netp::make_shared<ws_frame>();
		_frame->H.B1.Bit.fin = 0x1;
		_frame->H.B1.Bit.rsv1 = 0x0;
		_frame->H.B1.Bit.rsv2 = 0x0;
		_frame->H.B1.Bit.rsv3 = 0x0;
		_frame->H.B1.Bit.opcode = m_message_opcode;

		NETP_ASSERT(ctx->L->in_event_loop());

		if (m_type == websocket_type::T_SERVER) {
			_frame->H.B2.Bit.mask = 0x0;
		} else {
			NETP_TODO("client not supported right now");
		}

		if (outlet->len() > 0xFFFF) {
			_frame->H.B2.Bit.len = 0x7F;
			outlet->write_left<u64_t>(outlet->len());
			outlet->write_left<u8_t>( _frame->H.B2.B );
			outlet->write_left<u8_t>(_frame->H.B1.B);
		}
		else if (outlet->len() > 0x7D) {
			NETP_ASSERT(outlet->len() <= 0xFFFF);
			_frame->H.B2.Bit.len = 0x7E;
			outlet->write_left<u16_t>(outlet->len()&0xFFFF);
			outlet->write_left<u8_t>(_frame->H.B2.B);
			outlet->write_left<u8_t>(_frame->H.B1.B);
		}
		else {
			_frame->H.B2.Bit.len = outlet->len();
			outlet->write_left<u8_t>(_frame->H.B2.B);
			outlet->write_left<u8_t>(_frame->H.B1.B);
		}

		ctx->write(chp,outlet);
	}

	void websocket::close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) {

		if (m_close_sent == true) {
			ctx->close();
			chp->set(netp::E_CHANNEL_CLOSED);
			return;
		}

		NSP<ws_frame> _CLOSE = netp::make_shared<ws_frame>();
		_CLOSE->H.B1.Bit.fin = 0x1;
		_CLOSE->H.B1.Bit.rsv1 = 0x0;
		_CLOSE->H.B1.Bit.rsv2 = 0x0;
		_CLOSE->H.B1.Bit.rsv3 = 0x0;
		_CLOSE->H.B1.Bit.opcode = OP_CLOSE;

		_CLOSE->H.B2.Bit.mask = 0x0;
		_CLOSE->H.B2.Bit.len = 0x0;

		NRP<packet> outp_CLOSE = netp::make_ref<packet>();
		outp_CLOSE->write<u8_t>( _CLOSE->H.B1.B );
		outp_CLOSE->write<u8_t>( _CLOSE->H.B2.B );

		chp->if_done([ctx](int const& rt) {
			ctx->close();
		});
		ctx->write(chp, outp_CLOSE);
		m_close_sent = true;
	}

	int websocket::http_on_headers_complete(NRP<netp::http::parser> const& p, NRP<netp::http::message> const& m) {
		m_upgrade_req = m;
		(void)p;
		NETP_DEBUG(__FUNCTION__);
		return netp::OK;
	}

	int websocket::http_on_body(NRP<netp::http::parser> const& p, const char* data, u32_t len) {
		(void)p;
		(void)data;
		(void)len;
		NETP_DEBUG("[%s]<<< %s", __FUNCTION__, std::string(data, len).c_str());
		return netp::OK;
	}
	int websocket::http_on_message_complete(NRP<netp::http::parser> const& p) {
		(void)p;
		//@todo, we only support server side for tyhe currently impl
		NETP_ASSERT(m_type == websocket_type::T_SERVER);
		//m_upgrade_req->ver = {p->_p->http_major, p->_p->http_minor};
		NRP<packet> m;
		m_upgrade_req->encode(m);
		NETP_DEBUG("[%s], req: %s", __FUNCTION__, std::string( (char*) m->head(), m->len() ).c_str() );

		m_state = state::S_UPGRADE_REQ_MESSAGE_DONE;
		return netp::OK;
	}

	int websocket::http_on_chunk_header(NRP<netp::http::parser> const& p) {
		(void)p;
		NETP_DEBUG("[%s]", __FUNCTION__);
		return netp::OK;
	}

	int websocket::http_on_chunk_complete(NRP<netp::http::parser> const& p) {
		(void)p;
		NETP_DEBUG("[%s]", __FUNCTION__);
		return netp::OK;
	}
}}

#endif