#ifndef _NETP_ETH_HPP
#define _NETP_ETH_HPP

#include <netp/core.hpp>
#include <netp/string.hpp>

namespace netp {

	//refer to https://en.wikipedia.org/wiki/EtherType
	enum ether_type {
		ETH_TYPE_IPV4 = 0x0800,//
		ETH_TYPE_ARP = 0x0806,//
		ETH_TYPE_WAKE_ON_LAN = 0x0842,//
		ETH_TYPE_AVTP = 0x22F0,//Audio Video Transport Protocol (AVTP)
		ETH_TYPE_IETF_TRILL_PROTOCOL = 0x22F3,//
		ETH_TYPE_STREAM_RESERVATION_PROTOCOL = 0x22EA,//	Stream Reservation Protocol
		ETH_TYPE_DEC_MOP_RC = 0x6002,//
		ETH_TYPE_DEC_NET_PHASE_IV = 0x6003,//
		ETH_TYPE_DEC_LAT = 0x6004,//
		ETH_TYPE_RARP = 0x8035,//Reverse Address Resolution Protocol (RARP)
		ETH_TYPE_APPLE_TALK = 0x809B,//AppleTalk (Ethertalk)
		ETH_TYPE_APPLE_TALK_ARP = 0x80F3,//AppleTalk Address Resolution Protocol (AARP)
		ETH_TYPE_VLAN_TAGGED_FRAME = 0x8100,//VLAN-tagged frame (IEEE 802.1Q) and Shortest Path Bridging IEEE 802.1aq with NNI compatibility[10]
		ETH_TYPE_SLPP = 0x8102,//
		ETH_TYPE_IPX = 0x0837,//
		ETH_TYPE_QNX_QNET = 0x8204,//
		ETH_TYPE_IPV6 = 0x86DD,//
		ETH_TYPE_ETHERNET_FLOW_CONTROL = 0x8808,//
		ETH_TYPE_ETHERNET_SLOW_PROTOCOLS = 0x8809,//Ethernet Slow Protocols[11] such as the Link Aggregation Control Protocol
		ETH_TYPE_COBRA_NET = 0x8819,//
		ETH_TYPE_MPLS_UNICAST = 0x8847,//
		ETH_TYPE_MPLS_MULTICAST = 0x8848,//
		ETH_TYPE_PPPOE_DISCOVERY_STAGE = 0x8863,//
		ETH_TYPE_PPPOE_SESSION_STAGE = 0x8864,//
		ETH_TYPE_INTEL_ADVANCED_NETWORKING_SERVICES = 0x886D,//
		ETH_TYPE_JUMBO_FRAMES = 0x8870,//
		ETH_TYPE_HOME_PLUG_1_0_MME = 0x887B,//
		ETH_TYPE_EAP_OVER_LAN = 0x888E,//
		ETH_TYPE_PROFINET_PROTOCOL = 0x8892,//
		ETH_TYPE_HYPER_SCSI_OVER_ETHERNET = 0x889A,//
		ETH_TYPE_ATA_OVER_ETHERNET = 0x88A2,//
		ETH_TYPE_ETHER_CAT_PROTOCOL = 0x88A4,//EtherCAT Protocol
		ETH_TYPE_PROVIDER_BRIDGING_802_1_AD_802_1_AQ = 0x88A8,//Provider Bridging (IEEE 802.1ad) & Shortest Path Bridging IEEE 802.1aq
		ETH_TYPE_ETHERNET_POWERLINK = 0x88AB,//Ethernet Powerlink
		ETH_TYPE_GOOSE = 0x88B8,//GOOSE (Generic Object Oriented Substation event)
		ETH_TYPE_GSE = 0x88B9, //	GSE(Generic Substation Events) Management Services
		ETH_TYPE_SAMPLED_VALU_TRANSMISSION = 0x88BA, //SV(Sampled Value Transmission)
		ETH_TYPE_LLDP = 0x88CC, //	Link Layer Discovery Protocol(LLDP)
		ETH_TYPE_SERCOS_III = 0x88CD,	//SERCOS III
		ETH_TYPE_WSMP = 0x88DC,//	WSMP, WAVE Short Message Protocol
		ETH_TYPE_HOME_PLUG_AV_MME = 0x88E1,//	HomePlug AV MME[citation needed]
		ETH_TYPE_MEDIA_REDUNDANCY_PROTOCOL = 0x88E3,//	Media Redundancy Protocol(IEC62439 - 2)
		ETH_TYPE_MAC_SECURITY = 0x88E5,//	MAC security(IEEE 802.1AE)
		ETH_TYPE_PROVIDER_BACKBONE_BRIDGES = 0x88E7,//	Provider Backbone Bridges(PBB) (IEEE 802.1ah)
		ETH_TYPE_PRECISION_TIME_PROTOCOL_OVER_ETHERNET = 0x88F7,//	Precision Time Protocol(PTP) over Ethernet(IEEE 1588)
		ETH_TYPE_NC_SI = 0x88F8,//	NC - SI
		ETH_TYPE_PARALLEL_REDUNDANCY_PROTOCOL = 0x88FB,//	Parallel Redundancy Protocol(PRP)
		ETH_TYPE_IEEE_802_1_AG = 0x8902,//	IEEE 802.1ag Connectivity Fault Management(CFM) Protocol / ITU - T Recommendation Y.1731 (OAM)
		ETH_TYPE_FCOE = 0x8906,//	Fibre Channel over Ethernet(FCoE)
		ETH_TYPE_FCOE_INITIALIZATION_PROTOCOL = 0x8914,//	FCoE Initialization Protocol
		ETH_TYPE_RDMA = 0x8915,//	RDMA over Converged Ethernet(RoCE)
		ETH_TYPE_TTE = 0x891D,//	TTEthernet Protocol Control Frame(TTE)
		ETH_TYPE_HSR = 0x892F,//	High - availability Seamless Redundancy(HSR)
		ETH_TYPE_ECTP = 0x9000,	//Ethernet Configuration Testing Protocol[13]
		ETH_TYPE_VLAN_TAGGED_IEEE_802_1Q = 0x9100,//	VLAN - tagged(IEEE 802.1Q) frame with double tagging
		ETH_TYPE_LLT = 0xCAFE,//	Veritas Technologies Low Latency Transport(LLT)
	};


	//refer to http://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml
	enum arp_operation_code {
		ARP_OP_RESERVED = 0, //Reserved[RFC5494]
		ARP_OP_REQUEST = 1,//	REQUEST[RFC826][RFC5227]
		ARP_OP_REPLY = 2,//	REPLY[RFC826][RFC5227]
		ARP_OP_REQUEST_REVERSE = 3,//	request Reverse[RFC903]
		ARP_OP_REPLY_REVERSE = 4,//	reply Reverse[RFC903]
		ARP_OP_DRARP = 5,//	DRARP - Request[RFC1931]
		ARP_OP_DRARP_REPLY = 6,//	DRARP - Reply[RFC1931]
		ARP_OP_DRARP_ERROR = 7,//	DRARP - Error[RFC1931]
		ARP_OP_INARP = 8,//	InARP - Request[RFC2390]
		ARP_OP_INARP_REPLY = 9,//	InARP - Reply[RFC2390]
		ARP_OP_ARP_NAK = 10,//	ARP - NAK[RFC1577]
		ARP_OP_MARS_REQUEST = 11,//	MARS - Request[Grenville_Armitage]
		ARP_OP_MARS_MULTI = 12,//	MARS - Multi[Grenville_Armitage]
		ARP_OP_MARS_MSERV = 13,//	MARS - MServ[Grenville_Armitage]
		ARP_OP_MARS_JOIN = 14,//	MARS - Join[Grenville_Armitage]
		ARP_OP_MARS_LEAVE = 15,//	MARS - Leave[Grenville_Armitage]
		ARP_OP_MARS_NAK = 16,//	MARS - NAK[Grenville_Armitage]
		ARP_OP_MARS_UNSERV = 17,//	MARS - Unserv[Grenville_Armitage]
		ARP_OP_MARS_SJOIN = 18,//	MARS - SJoin[Grenville_Armitage]
		ARP_OP_MARS_SLEAVE = 19,//	MARS - SLeave[Grenville_Armitage]
		ARP_OP_MARS_GROUP_LIST = 20,//	MARS - Grouplist - Request[Grenville_Armitage]
		ARP_OP_MARS_GROUP_LIST_REPLY = 21,//	MARS - Grouplist - Reply[Grenville_Armitage]
		ARP_OP_MARS_REDIRECT_MAP = 22,//	MARS - Redirect - Map[Grenville_Armitage]
		ARP_OP_MAPOS_UNARP = 23,//	MAPOS - UNARP[Mitsuru_Maruyama][RFC2176]
		ARP_OP_OP_EXP1 = 24,//	OP_EXP1[RFC5494]
		ARP_OP_OP_EXP2 = 25,//	OP_EXP2[RFC5494]
		//26 - 65534	Unassigned
	};

	enum hardware_type {
		RESERVED = 0,//	Reserved[RFC5494]
		ETHERNET10 = 1,	//Ethernet(10Mb)[Jon_Postel]
		EXPERIMENT_ETHERNET = 2,//	Experimental Ethernet(3Mb)[Jon_Postel]
		AMATEUR_RADIO_AX_25 = 3,//	Amateur Radio AX.25[Philip_Koch]
		PROTEON_PRONET_TOKEN_RING = 4,//	Proteon ProNET Token Ring[Avri_Doria]
		CHAOS = 5,//	Chaos[Gill_Pratt]
		IEEE_802_NETWORKS = 6,//	IEEE 802 Networks[Jon_Postel]
		ARCNET = 7,//	ARCNET[RFC1201]
		HYPER_CHANNEL = 8,//	Hyperchannel[Jon_Postel]
		LANSTAR = 9,//	Lanstar[Tom_Unger]
		AUTONET_SHORT_ADDRESS = 10,//	Autonet Short Address[Mike_Burrows]
		LOCAL_TALK = 11,//	LocalTalk[Joyce_K_Reynolds]
		LOCAL_NET = 12,//	LocalNet(IBM PCNet or SYTEK LocalNET)[Joseph Murdock]
		ULTRA_LINK = 13,//	Ultra link[Rajiv_Dhingra]
		SMDS = 14,//	SMDS[George_Clapp]
		FRAME_REPLY = 15,//	Frame Relay[Andy_Malis]
		ASYNCHRONOUS_TRANSMISSION_MODE_JXB2 = 16,//	Asynchronous Transmission Mode(ATM) [[JXB2]]
		HDLC = 17,//	HDLC[Jon_Postel]
		FIBRE_CHANNEL = 18,//	Fibre Channel[RFC4338]
		ASYNCHRONOUS_TRANSMISSION_MODE_RFC2225 = 19,//	Asynchronous Transmission Mode(ATM)[RFC2225]
		SERIAL_LINE = 20,//	Serial Line[Jon_Postel]
		ASYNCHRONOUS_TRANSMISSION_MODE_MIKE_BURROWS = 21,//	Asynchronous Transmission Mode(ATM)[Mike_Burrows]
		MIL_STD = 22,//	MIL - STD - 188 - 220[Herb_Jensen]
		METRICOM = 23,	//Metricom[Jonathan_Stone]
		IEEE_1394 = 24,//	IEEE 1394.1995[Myron_Hattig]
		MAPOS = 25,//	MAPOS[Mitsuru_Maruyama][RFC2176]
		TWINAXIA = 26,//	Twinaxial[Marion_Pitts]
		EUI = 27,//	EUI - 64[Kenji_Fujisawa]
		HIPARP = 28,//	HIPARP[Jean_Michel_Pittet]
		IP_AND_ARP_OVER_ISO_7816 = 29,//	IP and ARP over ISO 7816 - 3[Scott_Guthery]
		ARP_SEC = 30,//	ARPSec[Jerome_Etienne]
		IPSEC_TUNNEL = 31,//	IPsec tunnel[RFC3456]
		INFINI_BAND = 32,//	InfiniBand(TM)[RFC4391]
		TIA_102_PROJECT_25_COMMON_AIR_INTERFACE = 33,//	TIA - 102 Project 25 Common Air Interface(CAI)[Jeff Anderson, Telecommunications Industry of America(TIA) TR - 8.5 Formulating Group, <cja015 & motorola.com>, June 2004]
		WIEGAND_INTERFACE = 34,//	Wiegand Interface[Scott_Guthery_2]
		PURE_IP = 35,//	Pure IP[Inaky_Perez - Gonzalez]
		HW_EXP1 = 36,//	HW_EXP1[RFC5494]
		HFI = 37,//	HFI[Tseng - Hui_Lin]
		//38 - 255	Unassigned
		HW_EXP2 = 256,//	HW_EXP2[RFC5494]
		AETHERNET = 257,//	AEthernet[Geoffroy_Gramaize]
		//258 - 65534	Unassigned
		RESERVED_RFC5494 = 65535,//	Reserved[RFC5494]
	};

#pragma pack(push,1)
	typedef union {
		struct __l2_mac__ {
			u8_t b1;
			u8_t b2;
			u8_t b3;
			u8_t b4;
			u8_t b5;
			u8_t b6;
		} B6;
		u8_t payload[6];
	} MAC;

#define __l2_eth_header_len 14
	typedef union {
		struct __eth_header__ {
			MAC dst;
			MAC src;
			u16_t type;
		}H;
		u8_t payload[__l2_eth_header_len];
	} eth_header;

#define __arp_hardware_type_offset_from_eth_header__ (__l2_eth_header_len)
#define __arp_protocol_type_offset_from_eth_header__ (__l2_eth_header_len+sizeof(dd_u16))

#define __arp_hardware_type(eth_header) (NETP_NTOHS( *((u16_t*) (eth_header->payload+__arp_hardware_type_offset_from_eth_header__))) )
#define __arp_protocol_type(eth_header) (NETP_NTOHS( *((u16_t*) (eth_header->payload+__arp_protocol_type_offset_from_eth_header__))) )

#pragma pack(pop)

	inline string_t m6tostring(MAC const& m6_) {
		char tmp[18] = { 0 };
		snprintf(tmp, 18, "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x", m6_.B6.b1, m6_.B6.b2, m6_.B6.b3, m6_.B6.b4, m6_.B6.b5, m6_.B6.b6);
		return string_t(tmp, 17);
	}
	inline bool operator==(MAC const& left, MAC const& right) {
		return std::memcmp((char*)left.payload, (char*)right.payload, 6) == 0;
	}
	inline bool operator!=(MAC const& left, MAC const& right) {
		return std::memcmp((char*)left.payload, (char*)right.payload, 6) != 0;
	}
}

#endif