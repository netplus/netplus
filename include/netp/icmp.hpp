#ifndef _NETP_PROTOCOL_ICMP_HPP_
#define _NETP_PROTOCOL_ICMP_HPP_

#include <netp/core.hpp>
#include <netp/os/api_wrapper.hpp>
#include <netp/socket.hpp>

namespace netp {

	struct IPHeader {
		u8_t	VER_IHL;
		u8_t	TOS;
		u16_t	total_length;
		u16_t	id;
		u16_t	FlagFragoffset;
		u8_t	TTL;
		u8_t	protocol;
		u16_t	checksum;
		u32_t	src;
		u32_t	dst;
	};

	enum ICMPType {
		T_ECHO_REPLY = 0,
		T_DESTINATION_UNREACHABLE = 3,
		T_SOURCE_QUENCH = 4,
		T_REDIRECT = 5,
		T_ECHO = 8,
		T_TIME_EXCEEDED = 11,
		T_PARAMETER_PROBLEM = 12,

		T_TIMESTAMP = 13,
		T_TIMESTAMP_REPLY = 14,
		T_INFORMATION_REQUEST = 15,
		T_INFORMATION_REQUEST_REPLY = 16
	};

	enum ICMPCode {
		C_NET_UNREACHABLE = 0,
		C_HOST_UNREACHABLE,
		C_PROTOCOL_UNREACHABLE,
		C_PORT_UNREACHABLE,
		C_FRAGMENTATION_NEEDED_AND_DF_SET,
		C_SOURCE_ROUTE_FAILED
	};


	struct ICMP_Echo {

		netp::byte_t	type;
		netp::byte_t	code;
		netp::u16_t 	checksum;
		netp::u16_t	id;
		netp::u16_t	seq;

		netp::u64_t	ts;	//our data
	};


	struct PingReply {
		netp::u16_t seq;
		netp::u32_t RTT;
		netp::u32_t bytes;
		netp::u32_t TTL;
	};

	static std::atomic<netp::u16_t> __pingseq(1);
	static netp::u16_t ping_make_seq() {
		return netp::atomic_incre<netp::u16_t>(&__pingseq);
	}

	//@deprecated
	class ICMP {

	private:
		NRP<netp::socket_channel> m_so;
		netp::byte_t* icmp_data;

	public:
		ICMP() :
			m_so(nullptr)
		{
		}
		~ICMP() {}

		int ping( char const * ip, PingReply& reply, u32_t timeout)
		{

			(void)timeout;
			int initrt = _init_socket();
			NETP_RETURN_V_IF_NOT_MATCH(initrt, initrt == netp::OK);

			ICMP_Echo echo;

			echo.type = ICMPType::T_ECHO;
			echo.code = 0;
			echo.id = netp::os::get_process_id()&0xffff;
			echo.seq = ping_make_seq();
			echo.ts = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
			echo.checksum = 0;

			NRP<netp::packet> icmp_pack = netp::make_ref<netp::packet>();
			icmp_pack->write<u8_t>(echo.type);
			icmp_pack->write<u8_t>(echo.code);
			icmp_pack->write<u16_t>(0);
			icmp_pack->write<u16_t>(echo.id);
			icmp_pack->write<u16_t>(echo.seq);
			icmp_pack->write<u64_t>(echo.ts);

			echo.checksum = _calculate_checksum(icmp_pack->head(), icmp_pack->len());

			icmp_pack->reset();
			icmp_pack->write<u8_t>(echo.type);
			icmp_pack->write<u8_t>(echo.code);
			icmp_pack->write<u16_t>(echo.checksum);
			icmp_pack->write<u16_t>(echo.id);
			icmp_pack->write<u16_t>(echo.seq);
			icmp_pack->write<u64_t>(echo.ts);

			NRP<netp::address> addr = netp::make_ref<address>( ip, u16_t(0), NETP_AF_INET);

			int ec = netp::OK;
			u32_t snd_c = netp::sendto(m_so->ch_id(), icmp_pack->head(), (u32_t)icmp_pack->len(), addr, ec,0);
			NETP_RETURN_V_IF_NOT_MATCH(ec, ec == netp::OK);

			(void)snd_c;

			NRP<netp::address> recv_addr;
			byte_t recv_buffer[256] = { 0 };

			u32_t recv_c = netp::recvfrom(m_so->ch_id(), recv_buffer, 256, recv_addr, ec,0);
			netp::u64_t now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
			NETP_RETURN_V_IF_NOT_MATCH(ec, ec == netp::OK);
			NETP_ASSERT(recv_c > 0);

			u32_t read_idx = 0;
			u8_t ver_IHL = netp::bytes_helper::read_u8(recv_buffer + read_idx);
			read_idx++;

			u32_t IPHeaderLen = (ver_IHL & 0x0f) * 4;

			u32_t ICMPHeader_idx = IPHeaderLen;

			ICMP_Echo echo_reply;

			echo_reply.type = netp::bytes_helper::read_u8(recv_buffer+ ICMPHeader_idx);
			ICMPHeader_idx++;
			echo_reply.code = netp::bytes_helper::read_u8(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx++;

			echo_reply.checksum = netp::bytes_helper::read_u16(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += 2;

			echo_reply.id = netp::bytes_helper::read_u16(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += 2;

			echo_reply.seq = netp::bytes_helper::read_u16(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += 2;

			echo_reply.ts = netp::bytes_helper::read_u64(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += sizeof(netp::u64_t);

			if (echo_reply.id == echo.id	&&
				echo_reply.seq == echo.seq	&&
				echo_reply.type == T_ECHO_REPLY
			)
			{
				reply.seq	= echo.seq;
				reply.RTT	= (netp::u32_t)(now - echo.ts);
				reply.bytes = sizeof(echo.ts);
				reply.TTL = netp::bytes_helper::read_u8(recv_buffer+8);
			}

			return netp::OK;
		}

	private:

		int _init_socket()
		{
			if (m_so == nullptr) {
				int creatert;
				NRP<socket_cfg> cfg = netp::make_ref<socket_cfg>();
				cfg->type = NETP_SOCK_RAW;
				cfg->proto = NETP_PROTOCOL_ICMP;
				cfg->L = io_event_loop_group::instance()->next();
				std::tie(creatert, m_so) = netp::create_socket_channel(cfg);
				return creatert;
			}

			return netp::OK;
		}

		u16_t _calculate_checksum(byte_t* data, size_t length) {

			u32_t checksum = 0;
			size_t	_length = length;

			while (_length > 1) {
				checksum += netp::bytes_helper::read_u16(data + (length - _length));
				_length -= sizeof(u16_t);
			}

			if (_length) {
				checksum += netp::bytes_helper::read_u8(data+(length - _length));
			}

			checksum = (checksum >> 16) + (checksum & 0xffff);
			checksum += (checksum >> 16);
			return(u16_t)(~checksum);
		}
	};
}

#endif