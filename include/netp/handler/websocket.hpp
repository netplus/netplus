#ifndef _NETP_HANDLER_WEBSOCKET_HPP
#define _NETP_HANDLER_WEBSOCKET_HPP

#include <netp/core.hpp>

#include <netp/channel_handler.hpp>
#include <netp/http/message.hpp>
#include <netp/http/parser.hpp>

#ifdef NETP_WITH_BOTAN

namespace netp { namespace handler {

	enum class websocket_type {
		T_CLIENT,
		T_SERVER
	};

	enum class websocket_close_code {
		C_NORMAL_CLOSED = 1000,
		C_SERVER_GOING_DOWN = 1001,
		C_PROTOCOL_ERROR = 1002,
		C_RECEIVED_MESSAGE_DATA_NOT_SUPPORTED = 1003,
		C_RESERVED = 1004, //not used for now
		C_NO_CLOSE_CODE = 1005,
		C_ABNORMAL_CLOSED = 1006,//conn closed, but did not received any Close control frame
		C_RECEIVED_MESSAGE_DATA_NON_CONSISTENT = 1007,
		C_VIOLATES_POLICY = 1008,
		C_RECEIVED_MESSAGE_DATA_TOO_BIG = 1009,
		C_CLIENT_CLOSE_FOR_NEGOTIATE_EXT_FAILED = 1010,
		C_SERVER_CLOSE_CAN_NOT_FULLFILL_REQ = 1011,
		C_TLS_HANDSHAKE_FAILED = 1015
	};

	class websocket final:
		public netp::channel_handler_abstract
	{
		enum class state {
			S_IDLE,
			S_CLOSED,
			S_WAIT_CLIENT_HANDSHAKE_REQ,
			S_HANDSHAKING,
			S_UPGRADE_REQ_MESSAGE_DONE,
			S_MESSAGE_BEGIN,
			S_FRAME_BEGIN,
			S_FRAME_READ_H,
			S_FRAME_READ_PAYLOAD_LEN,
			S_FRAME_READ_MASKING_KEY,
			S_FRAME_READ_PAYLOAD,
			S_FRAME_END,
			S_MESSAGE_END,
			S_FRAME_CLOSE_RECEIVED,
			S_CLOSING
		};

		enum frame_opcode {
			OP_CONTINUE = 0x0,
			OP_TEXT = 0x1,
			OP_BINARY = 0x2,
			OP_X3 = 0x3,
			OP_X4 = 0x4,
			OP_X5 = 0x5,
			OP_X6 = 0x6,
			OP_X7 = 0x7,
			OP_CLOSE = 0x8,
			OP_PING = 0x9,
			OP_PONG = 0xA,
			OP_B = 0xB,
			OP_C = 0xC,
			OP_D = 0xD,
			OP_E = 0xE,
			OP_F = 0xF
		};

		struct ws_frame {
			struct _H {
				union _B1 {
					struct _Bit {
#ifdef __NETP_IS_LITTLE_ENDIAN
						u8_t opcode : 4;
						u8_t rsv3 : 1;
						u8_t rsv2 : 1;
						u8_t rsv1 : 1;
						u8_t fin : 1;
#else
						u8_t fin : 1;
						u8_t rsv1 : 1;
						u8_t rsv2 : 1;
						u8_t rsv3 : 1;
						u8_t opcode : 4;
#endif
					} Bit;
					u8_t B;
				} B1;
				union _B2 {
					struct _Bit {
#ifdef __NETP_IS_LITTLE_ENDIAN
						u8_t len : 7;
						u8_t mask : 1;
#else
						u8_t mask : 1;
						u8_t len : 7;
#endif
					} Bit;
					u8_t B;
				} B2;
			} H;

			u64_t payload_len;
			u8_t masking_key_arr[4];

			NRP<packet> extdata;
			NRP<packet> appdata;

			void reset() {
				H.B1.B = 0;
				H.B2.B = 0;

				payload_len = 0;
				::memset(masking_key_arr, 0, 4);
				extdata->reset();
				appdata->reset();
			}
		};
		
		std::string m_tmp_for_field;
		NRP<netp::http::message> m_upgrade_req;
		NRP<netp::http::parser> m_http_parser;
		NRP<netp::packet> m_income_prev;
		NSP<ws_frame> m_tmp_frame;

		NRP<packet> m_tmp_message; //for fragmented message
		websocket_type m_type;
		state m_state;
		u8_t m_message_opcode;
		u8_t m_fragmented_begin;
		bool m_close_sent;
		websocket_close_code m_close_code;
		std::string m_close_reason;

	protected:
			int http_on_headers_complete(NRP<netp::http::parser> const& p, NRP<netp::http::message> const& m);
			int http_on_body(NRP<netp::http::parser> const& p,const char* data, u32_t len);
			int http_on_message_complete(NRP<netp::http::parser> const& p);

			int http_on_chunk_header(NRP<netp::http::parser> const& p);
			int http_on_chunk_complete(NRP<netp::http::parser> const& p);

			public:
				websocket(websocket_type t) :
					channel_handler_abstract(CH_ACTIVITY_CONNECTED|CH_ACTIVITY_CLOSED | CH_INBOUND_READ | CH_OUTBOUND_WRITE|CH_OUTBOUND_CLOSE),
					m_http_parser(nullptr),
					m_type(t),
					m_state(state::S_IDLE),
					m_message_opcode(OP_TEXT),
					m_fragmented_begin(false),
					m_close_sent(false),
					m_close_code(websocket_close_code::C_NO_CLOSE_CODE)
				{}

				virtual ~websocket() {}
				void connected(NRP<channel_handler_context> const& ctx) override;
				void closed(NRP<channel_handler_context> const& ctx) override;
				void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override;
				void write(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet ) override;
				void close(NRP<promise<int>> const& chp, NRP<channel_handler_context> const& ctx) override;
		};
}}

#endif

#endif