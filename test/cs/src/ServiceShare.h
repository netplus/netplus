#ifndef _SERVICES_COMMAND_H_
#define _SERVICES_COMMAND_H_

#include <netp.hpp>
namespace services {

	enum ServicesId {
		S_ECHO = 100
	};

	enum EchoCommand {
		C_ECHO_HELLO = 1,
		C_ECHO_STRING_REQUEST_TEST,
		C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES,
		C_ECHO_PINGPONG,
		C_ECHO_SEND_TEST
	};

	enum IncreaseMode {
		INC = 0,
		DRE
	};

	const netp::u32_t MAX_SND_LEN		= 64;
	netp::u32_t LAST_LEN				= 64;
	static netp::byte_t* bytes_buffer	= NULL ;

	void InitSndBytes() {
		static char s_char_table[] = {
			'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
			'0','1','2','3','4','5','6','7','8','9',
			'\'','!','@','#','$','%','^','&','*','(',')',',','_','+','|','}','{','?','>','<',
			'~','`','[',']',';','/','.',
			'-','='
		};
		netp::u32_t char_table_len = sizeof(s_char_table) / sizeof(s_char_table[0]);
		bytes_buffer = (netp::byte_t*)malloc(MAX_SND_LEN * sizeof(netp::byte_t));

		for (netp::u32_t i = 0; i<MAX_SND_LEN; i++) {
			bytes_buffer[i] = s_char_table[i%char_table_len];
		}
	}

	void DeinitSndBytes() {
		free(bytes_buffer);
	}

	class HelloProcessor {

	public:

		HelloProcessor()
		{
		}

		void HandleResp(NRP<netp::channel_handler_context> const& ctx,NRP<netp::packet> const& resp_pack ) {
			NRP<netp::packet> const& packet = resp_pack;
			netp::u32_t command = packet->read<netp::u32_t>();

			switch (command) {
			case services::C_ECHO_HELLO:
			case services::C_ECHO_STRING_REQUEST_TEST:
			case services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES:
			{
				netp::u32_t code = packet->read<netp::u32_t>();
				NETP_CONDITION_CHECK(code == netp::OK);

				netp::u32_t receive_hello_len = packet->read<netp::u32_t>();
				netp::byte_t* received_hello_str = NULL;

				if (receive_hello_len > 0) {
					received_hello_str = (netp::byte_t*)malloc(receive_hello_len * sizeof(netp::byte_t));
					NETP_CONDITION_CHECK(packet->len() == receive_hello_len);
					packet->read(received_hello_str, receive_hello_len);
					free(received_hello_str);
				}

				if (command == services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES) {
					return;
				}

				//resend
				//int cid = netp::client::service::Echo<ClientType>::C_TEST ;
				NRP<netp::packet> packet_o = netp::make_ref<netp::packet>(256);
				packet_o->write<netp::u32_t>(services::C_ECHO_STRING_REQUEST_TEST);
				netp::u32_t to_snd_len = (++LAST_LEN%MAX_SND_LEN);

				packet_o->write<netp::u32_t>(to_snd_len);
				packet_o->write((netp::byte_t*)bytes_buffer, to_snd_len);

				NRP<netp::packet> packet_t1 = netp::make_ref<netp::packet>(packet_o->len());
				packet_t1->write(packet_o->head(), packet_o->len());

				packet_t1->write_left < netp::u8_t >(services::S_ECHO);
				ctx->write(packet_t1);

//				NETP_CHECK_SOCKET_SEND_RETURN_V(rt);

#ifdef TEST_SEND
				WWSP<Packet> packet_ss(new Packet(*packet_o));
				packet_ss->WriteLeft<netp::ServiceIdT>(services::S_ECHO);

				WWSP<MyMessageT> message_ss(new MyMessageT(packet_ss));

				rt = client->Send(message_ss);
				NETP_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif

#ifdef TEST_PRESSES
				NRP<Packet> packet_press(new Packet(256));
				packet_press->Write<u32_t>(netp::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES);
				packet_press->Write<u32_t>(rand_len);
				packet_press->Write((byte_t*)m_test_to_send_buffer, rand_len);

				NRP<MyMessageT> message_press(new MyMessageT(netp::WSI_ECHO, packet_press));
				rt = client->Request(message_press);
				NETP_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif

#ifdef TEST_SEND
				//rt = client->Send( message_ss );
				//NETP_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif
			}
			break;
			case services::C_ECHO_PINGPONG:
			{
				netp::u32_t code = packet->read<netp::u32_t>();
				NETP_ASSERT(code == netp::OK);

				//int rt;
				//PEER_REQ_HELLO(peer, rt);

				SendHello(ctx);
			}
			break;
			default:
			{
				NETP_ASSERT( !"invalid respond command id");
			}
			break;
			}
		}

		static void SendHello(NRP<netp::channel_handler_context> const& ctx) {

			NRP<netp::packet> packet = netp::make_ref<netp::packet>(256);
			std::string hello_string("hello server");
			packet->write<netp::u32_t>(services::C_ECHO_HELLO);
			packet->write<netp::u32_t>(hello_string.length());
			packet->write((netp::byte_t const* const)hello_string.c_str(), hello_string.length());

			packet->write_left<netp::u8_t>(services::S_ECHO);
			ctx->write(packet);
		}
	};
}

#endif