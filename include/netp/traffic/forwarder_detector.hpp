#ifndef _NETP_TRAFFIC_FORWARDER_DETECTOR_HPP
#define _NETP_TRAFFIC_FORWARDER_DETECTOR_HPP

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/packet.hpp>
#include <netp/channel.hpp>

namespace netp { namespace traffic {

	enum class forward_type {
		iptcp, //header by ip/tcp
		iptcp_payload, //only tcp payload
		ipudp, //header by ip/udp, for none connected udp session, we must always forward by this type
		ipudp_payload, //for connection oriented forward
		l2, //including ip,icmp,etc, all l2 kinds of layer frame
		forward_type_max
	};

	//cmd frame format
	//[cmd_type][forward_spec_frame_followed...]
	class forwarder_detector :
		public netp::channel_handler_abstract
	{
		NRP<channel_handler_abstract> _make_forwarder(forward_type t);
	public:
		forwarder_detector() :
			channel_handler_abstract(netp::CH_INBOUND_READ)
		{}
		void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) override;
	};
}}

#endif