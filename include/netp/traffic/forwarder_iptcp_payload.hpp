#ifndef _NETP_TRAFFIC_FORWARDER_IPTCP_PAYLOAD_HPP
#define _NETP_TRAFFIC_FORWARDER_IPTCP_PAYLOAD_HPP

#include <deque>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/io_event_loop.hpp>
#include <netp/channel.hpp>
#include <netp/traffic/repeater.hpp>

namespace netp { namespace traffic {

	enum class address_type {
		T_DOMAIN, //domain
		T_IPV4,
		T_IPV6
	};

	enum class forwarder_iptcp_payload_state {
		READ_DST_PORT_AND_TYPE,
		READ_DST_ADDR,
		DIAL_DST,
		DST_CONNECTED,
		DIAL_FAILED,
		IPTCP_FORWARD_STATE_MAX
	};

	class forwarder_iptcp_payload;
	class iptcp_payload_dst_handler final :
		public netp::channel_handler_abstract
	{
		friend class forwarder_iptcp_payload;
		NRP<forwarder_iptcp_payload> m_forwarder;

	public:
		iptcp_payload_dst_handler(NRP<forwarder_iptcp_payload> const& forwarder) :
			channel_handler_abstract(netp::CH_ACTIVITY_CONNECTED | netp::CH_ACTIVITY_CLOSED | netp::CH_ACTIVITY_READ_CLOSED | netp::CH_INBOUND_READ),
			m_forwarder(forwarder)
		{}

		~iptcp_payload_dst_handler() {}

		void connected(NRP<netp::channel_handler_context> const& ctx) override;
		void closed(NRP<netp::channel_handler_context > const& ctx) override;
		void read_closed(NRP<netp::channel_handler_context > const& ctx) override;
		void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) override;
	};

	class forwarder_iptcp_payload final:
		public netp::channel_handler_abstract
	{
		friend class iptcp_payload_dst_handler;

		NRP<netp::io_event_loop> m_loop;

		NRP<netp::channel> m_src_ch;
		NRP<netp::channel> m_dst_ch;

		NRP<repeater<NRP<netp::channel_handler_context>>> m_repeater_dst_to_src;
		NRP<repeater<NRP<netp::channel_handler_context>>> m_repeater_src_to_dst;
		NRP<packet> m_first_packet;

		u32_t m_src_rcv_wnd;
		u32_t m_src_snd_wnd;

		netp::channel_id_t m_src_channel_id;
		forwarder_iptcp_payload_state m_forward_state;
		int m_dial_errno;
		bool m_src_read_closed;
		address_type m_dst_address_type;

		ipv4_t m_dst_ipv4;
		port_t m_dst_port;

		string_t		m_dst_domain;

		void _dst_connected(NRP<netp::channel_handler_context> const& ctx);
		void _dst_closed(NRP<netp::channel_handler_context> const& ctx);
		void _dst_read_closed(NRP<netp::channel_handler_context> const& ctx);
		void _dst_read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income);

		void _dial_dst();
	public:
		forwarder_iptcp_payload();

		void connected(NRP<netp::channel_handler_context> const& ctx) override;
		void closed(NRP<netp::channel_handler_context> const& ctx) override;
		void read_closed(NRP<netp::channel_handler_context> const& ctx) override;
		void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) override;
	};

}}
#endif