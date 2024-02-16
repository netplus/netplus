#ifndef _NETP_IPV4_HPP
#define _NETP_IPV4_HPP

#include <vector>
#include <netp/memory.hpp>
#include <netp/eth.hpp>

namespace netp {
	typedef unsigned short port_t;
	static_assert(sizeof(port_t) == 2, "port_t size assert failed");

#pragma pack(push,1)
	//@comment
	//192.168.1.1
	//little endian, b1 == 1, big endian b1 == 192
	typedef union __ipv4_bits ipv4_t;
	union __ipv4_bits {
		u32_t u32;
		struct in_addr inaddr;
		struct _bit {
			u8_t b1;
			u8_t b2;
			u8_t b3;
			u8_t b4;
		} bits;
	};
	static_assert(sizeof(ipv4_t) == 4, "ipv4_t size assert failed");

	__NETP_FORCE_INLINE
	bool operator== (ipv4_t const& A, ipv4_t const& B) {
		return A.u32 == B.u32;
	}
	__NETP_FORCE_INLINE
	bool operator!= (ipv4_t const& A, ipv4_t const& B) {
		return A.u32 != B.u32;
	}

	template <>
    struct hash<netp::ipv4_t> {
        inline size_t operator()(const netp::ipv4_t& v4) const {
            return v4.u32;
        }
    };


#define __netp_l2_arp_ipv4_payload_len (28)
	typedef union __netp_l2_arp_ipv4_payload arp_ipv4_payload;
	union __netp_l2_arp_ipv4_payload {
		struct __arp_ipv4_payload__ {
			u16_t hardware_type;
			u16_t protocol_type;
			u8_t hardware_size;
			u8_t protocol_size;
			u16_t opcode;
			MAC sender_mac;
			ipv4_t sender_ip;
			MAC target_mac;
			ipv4_t target_ip;
		} data;
		u8_t payload[__netp_l2_arp_ipv4_payload_len];
	};

#define __netp_l2_arp_ipv4_frame_len (__netp_l2_eth_header_len + __netp_l2_arp_ipv4_payload_len)

	typedef union __netp_l2_arp_ipv4_frame arp_ipv4_frame;
	union __netp_l2_arp_ipv4_frame {
		struct __netp_l2_arp_ipv4_frame__ {
			eth_header eth_h;
			arp_ipv4_payload v4_payload;
		} arp_frame;
		u8_t payload[__netp_l2_arp_ipv4_frame_len];
	};

#define __netp_l2_ipv4_header_len 20

	typedef union __netp_l2_ipv4_header ipv4_header;

	union __netp_l2_ipv4_header {
		struct __ipv4_header__ {
		/*
            Internet Header Length is the length of the internet header in 32
            bit words, and thus points to the beginning of the data.  Note that
            the minimum value for a correct header is 5.
        */
#ifdef __L2_LITTLE_ENDIAN
			u8_t ihl : 4;
			u8_t ver : 4;
#else
			u8_t ver : 4;
			u8_t ihl : 4;
#endif

			u8_t tos;

			/*
			Total Length is the length of the datagram, measured in octets,including internet header and data
			*/
			u16_t total_len;
			u16_t id;
			u16_t flags_fragment_offset;
			u8_t ttl;

			/*
				1) refer to: https://datatracker.ietf.org/doc/html/rfc790
				2) #include <linux/in.h>
			*/
			u8_t protocol;
			u16_t sum;
			ipv4_t src;
			ipv4_t dst;
		} H;
		u8_t payload[__netp_l2_ipv4_header_len];
	};

#define __netp_l2_ipv4_tcp_header_len 20
	typedef union __netp_l2_ipv4_tcp_header ipv4_tcp_header;

	union __netp_l2_ipv4_tcp_header {
		struct __ipv4_tcp_header__ {
			port_t src;
			port_t dst;
			u32_t seq;
			u32_t ack_seq;

#ifdef __L2_LITTLE_ENDIAN
			u8_t res1 : 4;
			u8_t doff : 4;
			u8_t fin : 1;
			u8_t syn : 1;
			u8_t rst : 1;
			u8_t psh : 1;
			u8_t ack : 1;
			u8_t urg : 1;
			u8_t res2 : 2;
#else
			u8_t doff : 4;
			u8_t res1 : 4;
			u8_t res2 : 2;
			u8_t urg : 1;
			u8_t ack : 1;
			u8_t psh : 1;
			u8_t rst : 1;
			u8_t syn : 1;
			u8_t fin : 1;
#endif

			u16_t wnd;
			u16_t sum;
			u16_t urgent_pointer;
		} H;
		u8_t payload[__netp_l2_ipv4_tcp_header_len];
	};

#define __netp_l2_ipv4_tcp_header_option_len 12
	typedef u8_t ipv4_tcp_header_option[__netp_l2_ipv4_tcp_header_option_len];

	//tcp,udp share the same phdr
#define __netp_l2_ipv4_pseudo_header_len 12
	typedef union __netp_l2_ipv4_pheader ipv4_pheader;
	union __netp_l2_ipv4_pheader {
		struct __ipv4_pheader__ {
			ipv4_t src;
			ipv4_t dst;
			u8_t zero;
			u8_t protocol;
			u16_t dlen;
		} PH;
		u8_t payload[__netp_l2_ipv4_pseudo_header_len];
	};

#define __netp_l2_ipv4_tcp_header_with_tcp_option_len (__netp_l2_ipv4_tcp_header_len+__netp_l2_ipv4_tcp_header_option_len)
#define __netp_l2_ipv4_pseudo_header_tcp_header_len	(__netp_l2_ipv4_pseudo_header_len+__netp_l2_ipv4_tcp_header_len)
#define __netp_l2_ipv4_pseudo_header_tcp_header_with_tcp_option_len	(__netp_l2_ipv4_pseudo_header_len+__netp_l2_ipv4_tcp_header_with_tcp_option_len)

	typedef union __ipv4_pheader_tcp_header ipv4_pheader_tcp_header;
	union __ipv4_pheader_tcp_header {
		struct __ipv4_pheader_tcp_header__ {
			ipv4_pheader P;
			ipv4_tcp_header tcp_h;
			ipv4_tcp_header_option tcp_option;
		}ptcp;
		u8_t payload[__netp_l2_ipv4_pseudo_header_tcp_header_with_tcp_option_len];
	};

	//static_assert(sizeof(ipv4_pheader_tcp_header) == __tap_ipv4_pseudo_header_tcp_header_with_tcp_option_len, "wrong ipv4_pheader_tcp_header size");
	//static_assert(sizeof(ipv4_header) == __tap_ipv4_header_len, "sizeof(ip_header) != 20");
	//static_assert(sizeof(ipv4_tcp_header) == __tap_ipv4_tcp_header_len, "sizeof(tcp_header) != 20");

#define __netp_l2_ipv4_iptcp_header_len (__netp_l2_ipv4_header_len+__netp_l2_ipv4_tcp_header_len)
#define __netp_l2_ipv4_iptcp_header_len_with_tcp_option (__netp_l2_ipv4_iptcp_header_len+__netp_l2_ipv4_tcp_header_option_len)
	typedef union __netp_l2_ipv4_iptcp_header ipv4_iptcp_header;
	union __netp_l2_ipv4_iptcp_header {
		struct __ipv4_iptcp_header__ {
			__netp_l2_ipv4_header ip_h;
			ipv4_tcp_header tcp_h;
			ipv4_tcp_header_option tcp_option;
		} iptcp;
		u8_t payload[__netp_l2_ipv4_iptcp_header_len_with_tcp_option];
	};
	//static_assert(sizeof(ipv4_iptcp_header) == __tap_ipv4_iptcp_header_len_with_tcp_option, "sizeof(iptcp_header) != 52");

#define __netp_l2_ipv4_udp_header_len 8
	typedef union __netp_l2_ipv4_udp_header ipv4_udp_header;
	union __netp_l2_ipv4_udp_header {
		struct __ipv4_udp_header__ {
			port_t src;
			port_t dst;
			port_t len;
			port_t sum;
		}H;
		u8_t payload[__netp_l2_ipv4_udp_header_len];
	};

#define __netp_l2_ipv4_pseudo_header_udp_header_len	(__netp_l2_ipv4_pseudo_header_len+__netp_l2_ipv4_udp_header_len)
	typedef union __netp_l2_ipv4_pheader_udp_header ipv4_pheader_udp_header;
	union __netp_l2_ipv4_pheader_udp_header {
		struct __ipv4_pheader_udp_header__ {
			ipv4_pheader P;
			ipv4_udp_header udp;
		} pudp;
		u8_t payload[__netp_l2_ipv4_pseudo_header_udp_header_len];
	};

	//static_assert(sizeof(ipv4_pheader_udp_header) == __netp_l2_ipv4_pseudo_header_udp_header_len, "wrong ipv4_pheader_udp_header size");
#define __netp_l2_ipv4_ipudp_header_len (__netp_l2_ipv4_header_len+__netp_l2_ipv4_udp_header_len)
	typedef union __netp_l2_ipv4_ipudp_header ipv4_ipudp_header;

	union __netp_l2_ipv4_ipudp_header {
		struct __ipv4_ipudp_header__ {
			__netp_l2_ipv4_header ip_h;
			ipv4_udp_header udp_h;
		}ipudp;
		u8_t payload[__netp_l2_ipv4_ipudp_header_len];
	};

#pragma pack(pop)
}

#endif