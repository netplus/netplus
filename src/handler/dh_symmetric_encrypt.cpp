#include <netp/handler/dh_symmetric_encrypt.hpp>
#include <netp/string.hpp>
#include <netp/channel_handler_context.hpp>
#include <netp/channel.hpp>


namespace netp { namespace handler {

		const static char* H_DONE_MESSAGE = "EGASSEM!&88";

			/*
			* KEY_EXCHANGE frame
			*
			* any data except frame 1 arrived before KEY_GENERATED state would be taken as invalid

			enum symmetric_cipher_suite {
			CIPHER_XXTEA,
			CIPHER_AES_256
			};

			frame_handshake_hello
			{
			u64_t:random_id,
			symmetric_cipher_suite_list<CIPHER_ID1,CIPHER_ID2,...>,
			u8_t:compress_method,
			}
			frame_handshake_hello_resp
			{
			u64_t:random_id,
			uint8:symmetric_cipher_suite_id,
			u8_t:compress_method,
			}
			frame_handshake_ok
			{
				encrypted( "MESSAGE" ),
			}
			if the remote side decrypted success , it would get "MESSAGE", otherwise, remote side should close connection

			frame_handshake_error
			{
			i32_t:code
			}
			frame_data
			{
			}

			//below frame not implementeds right now
			frame_change_key_request
			{
			u64_t:random_id
			}
			frame_change_key_response
			{
			u64_t:random_id
			}
			*/

			/* KEY EXCHANGE & symmetric cipher choice machnism
			* A connect to B
			* A send hello_server message to B
			* B send hello_client message to A
			* B generate its own private key, setup compress method, send handshake_ok message to A
			* A generate its own private key, setup compress method, send handshake_ok message to B
			*
			* from now on, A <-> B can transfer data between them
			*/

		dh_symmetric_encrypt::dh_symmetric_encrypt() :
			channel_handler_abstract(CH_ACTIVITY_CONNECTED|CH_ACTIVITY_CLOSED|CH_ACTIVITY_READ_CLOSED|CH_ACTIVITY_WRITE_CLOSED|CH_INBOUND_READ|CH_OUTBOUND_WRITE),
			m_dhstate(DH_IDLE)
		{
			m_context.dhfactor.pub_key = 0;
			m_context.dhfactor.priv_key = 0;
			m_context.cipher_count = 2;
			m_context.ciphers[0] = CS_XXTEA;
			m_context.ciphers[1] = CS_NONE;
			m_context.compress_count = 1;
			m_context.compress[0] = C_NONE;
		}

		dh_symmetric_encrypt::~dh_symmetric_encrypt()
		{
		}

		void dh_symmetric_encrypt::handshake_assign_dh_factor(netp::security::DH_factor factor) {
			m_context.dhfactor = factor;
		}

		void dh_symmetric_encrypt::handshake_make_hello_packet(NRP<packet>& hello) {
			NETP_ASSERT(hello == nullptr);

			m_context.dhfactor = netp::security::dh_generate_factor();
			NETP_TRACE_DH("[dh_symmetric_encrypt]pub key: %llu,priv key: %llu", m_context.dhfactor.pub_key, m_context.dhfactor.priv_key);

			DH_frame_handshake_hello fhello;
			fhello.pub_key = m_context.dhfactor.pub_key;

			fhello.cipher_count = m_context.cipher_count;
			for (u8_t i = 0; i < m_context.cipher_count; ++i) {
				fhello.ciphers[i] = m_context.ciphers[i];
			}
			fhello.compress_count = m_context.compress_count;
			for (u8_t i = 0; i < m_context.compress_count; ++i) {
				fhello.compress[i] = m_context.compress[i];
			}

			NRP<packet> hello_packet = netp::make_ref<packet>();

#ifdef NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
			hello_packet->write<u32_t>(security::u8_u32_obfuscate(fhello.frame, netp::random_u32()));
#else
			hello_packet->write<u8_t>(fhello.frame);
#endif

			hello_packet->write<u64_t>(fhello.pub_key);

			hello_packet->write<u8_t>(fhello.cipher_count);
			for (u8_t i = 0; i < fhello.cipher_count; ++i) {
				hello_packet->write<u8_t>(fhello.ciphers[i] & 0xff);
			}
			hello_packet->write<u8_t>(fhello.compress_count);
			for (u8_t i = 0; i < fhello.compress_count; ++i) {
				hello_packet->write<u8_t>(fhello.compress[i] & 0xff);
			}

			hello = hello_packet;
		}

		void dh_symmetric_encrypt::handshake_packet_arrive(NRP<channel_handler_context> const& ctx, NRP<packet> const& in) {
			NETP_ASSERT(m_dhstate != DH_DATA_TRANSFER, "[%s]current state: %u", ctx->ch->ch_info().c_str(), m_dhstate);

		__parse_packet:
#ifdef NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
			if(in->len() < sizeof(u32_t) ) {
				NETP_WARN("[dh_symmetric_encrypt][%s]invalid packet header, close ctx", ctx->ch->ch_info().c_str() );
				ctx->close();
				return ;
			}

			NETP_ASSERT(in->len() >= sizeof(u32_t), "in->len(): %u", in->len() );
			const u8_t dh_frame = security::u8_u32_deobfuscate(in->read<u32_t>());
#else
			NETP_RETURN_V_IF_NOT_MATCH(netp::E_TLP_INVALID_PACKET, in->len() > sizeof(u8_t));
			const u8_t dh_frame = in->read<u8_t>();
#endif
			switch (dh_frame) {
			case F_HANDSHAKE_HELLO:
			{
				if (in->len() < sizeof(u64_t)) {
					NETP_WARN("[dh_symmetric_encrypt][%s]invalid public key, close ctx", ctx->ch->ch_info().c_str());
					ctx->close();
					return;
				}

				//generate key , generate resp, generate MESSAGE
				NETP_ASSERT(m_dhstate == DH_IDLE, "[dh][%s]current state: %u", ctx->ch->ch_info().c_str(), m_dhstate);
				m_dhstate = DH_HANDSHAKE;

				DH_frame_handshake_hello hello;
				hello.pub_key = in->read<u64_t>();
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]received pub key: %llu", ctx->ch->ch_info().c_str(), hello.pub_key);

				if (in->len() < sizeof(u8_t)) {
					NETP_WARN("[dh_symmetric_encrypt][%s]invalid cipher count, close ctx", ctx->ch->ch_info().c_str());
					ctx->close();
					return;
				}
				hello.cipher_count = in->read<u8_t>();

				if (in->len() < (hello.cipher_count)) {
					NETP_WARN("[dh_symmetric_encrypt][%s]invalid cipher list, close ctx", ctx->ch->ch_info().c_str());
					ctx->close();
					return;
				}

				for (u8_t i = 0; i < hello.cipher_count; ++i) {
					hello.ciphers[i] = (DH_cipher_suite)in->read<u8_t>();
				}

				if (in->len() < sizeof(u8_t)) {
					NETP_WARN("[dh_symmetric_encrypt][%s]invalid compress count, close ctx", ctx->ch->ch_info().c_str());
					ctx->close();
					return;
				}
				hello.compress_count = in->read<u8_t>();
				if (in->len() < (hello.compress_count)) {
					NETP_WARN("[dh_symmetric_encrypt][%s]invalid compress list, close ctx", ctx->ch->ch_info().c_str());
					ctx->close();
					return;
				}

				for (u8_t i = 0; i < hello.compress_count; ++i) {
					hello.compress[i] = (DH_compress)in->read<u8_t>();
				}
				NETP_ASSERT(in->len() == 0);

				m_context.dhfactor = netp::security::dh_generate_factor();
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]pub key: %llu,priv key: %llu", ctx->ch->ch_info().c_str(), m_context.dhfactor.pub_key, m_context.dhfactor.priv_key);

				DH_frame_handshake_hello_reply dh_frameHello_reply;
				dh_frameHello_reply.pub_key = m_context.dhfactor.pub_key;
				dh_frameHello_reply.cipher = hello.ciphers[0];
				dh_frameHello_reply.compress = hello.compress[0];

				NSP<netp::security::cipher_abstract> cipher = make_cipher_suite(dh_frameHello_reply.cipher);
				NETP_ASSERT(cipher != nullptr);
				u64_t key = netp::security::dh_generate_key(hello.pub_key, m_context.dhfactor.priv_key);
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]generate key: %llu", ctx->ch->ch_info().c_str(), key);
				byte_t bkey[16] = { 0 };
				netp::bytes_helper::write_u64(key, (byte_t*)&bkey[0]);
				cipher->set_key(bkey, sizeof(u64_t));

				m_cipher = cipher;

				NRP<packet> OOO;
				NRP<packet> hello_reply = netp::make_ref<packet>();

#ifdef NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
				hello_reply->write<u32_t>(security::u8_u32_obfuscate(dh_frameHello_reply.frame, netp::random_u32()));
#else
				hello_reply->write<u8_t>(hello_resp.frame);
#endif
				hello_reply->write<u64_t>(dh_frameHello_reply.pub_key);
				hello_reply->write<u8_t>(dh_frameHello_reply.cipher & 0xff);
				hello_reply->write<u8_t>(dh_frameHello_reply.compress & 0xff);
				NRP<netp::promise<int>> reply_rf = ctx->write(hello_reply);

				reply_rf->if_done( [ctx,cipher]( int const& rt ) {
					if(rt != netp::OK) {
						NETP_WARN("[dh_symmetric_encrypt][%s]reply hello failed: %u, close ctx", ctx->ch->ch_info().c_str(), rt );
						ctx->close();
						return ;
					}

					NRP<packet> message = netp::make_ref<packet>();
					message->write((byte_t*)H_DONE_MESSAGE, (netp::u32_t)netp::strlen(H_DONE_MESSAGE));
					NRP<packet> encrypted_message;
					NRP<packet> hlength_encrypted_message;

					int encrt = cipher->encrypt(message, encrypted_message);
					NETP_ASSERT(encrt == netp::OK);

#ifdef NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
					encrypted_message->write_left<u32_t>(security::u8_u32_obfuscate(F_HANDSHAKE_OK, netp::random_u32()));
#else
					encrypted_message->write_left<u8_t>(F_HANDSHAKE_OK);
#endif

					NRP<netp::promise<int>> m_rf = ctx->write(encrypted_message);
					m_rf->if_done([ctx]( int const& rt ) {
						if(rt != netp::OK) {
							NETP_WARN("[dh_symmetric_encrypt][%s]write message failed: %u, close ctx", ctx->ch->ch_info().c_str(), rt);
							ctx->close();
						}
					});
 				});
			}
			break;
			case F_HANDSHAKE_HELLO_REPLY:
			{
				NETP_ASSERT(m_dhstate == DH_HANDSHAKE);
				NETP_ASSERT((in->len() >= sizeof(u64_t) + sizeof(u8_t) * 2));

				//generate key, send MESSAGE
				DH_frame_handshake_hello_reply dh_frameHello_reply;
				dh_frameHello_reply.pub_key = in->read<u64_t>();
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]received pub key: %llu", ctx->ch->ch_info().c_str(), dh_frameHello_reply.pub_key);

				dh_frameHello_reply.cipher = (DH_cipher_suite)in->read<u8_t>();
				dh_frameHello_reply.compress = (DH_compress)in->read<u8_t>();
				//NETP_ASSERT(in->len() == 0);

				NSP<netp::security::cipher_abstract> cipher = make_cipher_suite(dh_frameHello_reply.cipher);
				NETP_ASSERT(cipher != nullptr);
				NETP_ASSERT(m_context.dhfactor.priv_key != 0);
				u64_t key = netp::security::dh_generate_key(dh_frameHello_reply.pub_key, m_context.dhfactor.priv_key);
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]generate key: %llu", ctx->ch->ch_info().c_str(), key);

				byte_t bkey[16] = { 0 };
				netp::bytes_helper::write_u64(key, (byte_t*)&bkey[0]);
				cipher->set_key(bkey, sizeof(u64_t));

				m_cipher = cipher;

				NRP<packet> message = netp::make_ref<packet>();
				message->write((byte_t*)H_DONE_MESSAGE, (netp::u32_t)netp::strlen(H_DONE_MESSAGE));
				NRP<packet> encrypted_message;
				NRP<packet> hlength_encrypted_message;

				int encrt = cipher->encrypt(message, encrypted_message);

				NETP_ASSERT(encrt == netp::OK);
#ifdef NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
				encrypted_message->write_left<u32_t>(security::u8_u32_obfuscate(F_HANDSHAKE_OK, netp::random_u32()));
#else
				encrypted_message->write_left<u8_t>(fhello.frame);
#endif
				ctx->write(encrypted_message);

				if (in->len()) { 
					NETP_TRACE_DH("[dh_symmetric_encrypt][%s]after F_HANDSHAKE_HELLO_REPLY: we have in->len(): %u", ctx->ch->ch_info().c_str(), in->len() );
					goto __parse_packet; 
				}
			}
			break;
			case F_HANDSHAKE_OK:
			{
				NETP_ASSERT(m_dhstate == DH_HANDSHAKE);
				//check decrypted(message) == "MESSAGE"

				NETP_ASSERT(m_cipher != nullptr);
				NRP<packet> message;
				int decryptrt = m_cipher->decrypt(in, message);
				NETP_ASSERT(decryptrt == netp::OK);

				if (netp::strncmp((char*)message->head(), H_DONE_MESSAGE, netp::strlen(H_DONE_MESSAGE)) == 0) {
					//handshake done
					m_dhstate = DH_DATA_TRANSFER;
					ctx->fire_connected();
				} else {
					NETP_WARN("[dh_symmetric_encrypt][%s]MESSAGE CHECK failed", ctx->ch->ch_info().c_str() );
					ctx->close();
				}
			}
			break;
			default:
			{
				NETP_WARN("[dh_symmetric_encrypt][%s]invalid frame id, close", ctx->ch->ch_info().c_str());
				ctx->close();
			}
			break;
			}
		}

		void dh_symmetric_encrypt::connected(NRP<channel_handler_context> const& ctx) {

			if (ctx->ch->ch_is_active()) {
				NRP<packet> hello;
				handshake_make_hello_packet(hello);

				NRP<promise<int>> chf = ctx->write(hello);
				m_dhstate = DH_HANDSHAKE;
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]active connected, write hello", ctx->ch->ch_info().c_str());

				chf->if_done([DHSE = NRP<dh_symmetric_encrypt>(this), ctx](int const& rt) {
					if (rt != netp::OK) {
						NETP_ERR("[dh_symmetric_encrypt][%s]write handshake hello failed: %d, force close", ctx->ch->ch_info().c_str(), rt);
						DHSE->m_dhstate = netp::handler::dh_symmetric_encrypt::DH_ERROR;
						ctx->close();
					}
				});
			} else {
				NETP_TRACE_DH("[dh_symmetric_encrypt][%s]passive connected, wait hello", ctx->ch->ch_info().c_str());
			}
		}

		void dh_symmetric_encrypt::read_closed(NRP<channel_handler_context> const& ctx) {
			NETP_TRACE_DH("[dh_symmetric_encrypt][%s]ctx read closed, curr state: %u", ctx->ch->ch_info().c_str(), m_dhstate);
			if(m_dhstate == DH_DATA_TRANSFER) {
				ctx->fire_read_closed();
			} else {
				ctx->close();
			}
		}

		void dh_symmetric_encrypt::write_closed(NRP<channel_handler_context> const& ctx) {
			NETP_TRACE_DH("[dh_symmetric_encrypt][%s]ctx write closed, curr state: %u", ctx->ch->ch_info().c_str(), m_dhstate);
			if(m_dhstate == DH_DATA_TRANSFER) {
				ctx->fire_write_closed();
			} else { 
				ctx->close(); 
			}
		}

		void dh_symmetric_encrypt::closed(NRP<channel_handler_context> const& ctx) {
			NETP_TRACE_DH("[dh_symmetric_encrypt][%s]ctx closed, curr state: %u", ctx->ch->ch_info().c_str(), m_dhstate);

			if (m_dhstate == DH_DATA_TRANSFER) {
				ctx->fire_closed();
			} else {
				NETP_WARN("[dh_symmetric_encrypt][%s]handshake failed: %u", ctx->ch->ch_info().c_str(), m_dhstate);
				ctx->fire_error(netp::E_DH_HANDSHAKE_FAILED);
			}
		}

		void dh_symmetric_encrypt::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
			NETP_ASSERT(income != nullptr);

			switch (m_dhstate) {
			case DH_DATA_TRANSFER:
			{
				NRP<packet> decrypted;
				NETP_ASSERT(m_cipher != nullptr);
				int decrypt = m_cipher->decrypt(income, decrypted);
				NETP_ASSERT(decrypt == netp::OK);
				ctx->fire_read(decrypted);
			}
			break;
			case DH_IDLE:
			case DH_HANDSHAKE:
			{
				handshake_packet_arrive(ctx, income);
			}
			break;
			case DH_CHANGE_KEY:
			{//@to impl
			}
			break;
			case DH_ERROR:
			{//@to impl
			}
			break;
			}
		}

		void dh_symmetric_encrypt::write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
			NETP_ASSERT(m_dhstate == DH_DATA_TRANSFER);
			NETP_ASSERT(m_cipher != nullptr);
			NRP<packet> encrypted;
			int encrt = m_cipher->encrypt(outlet, encrypted);
			NETP_ASSERT(encrt == netp::OK);
			ctx->write(intp, encrypted);
		}
}}