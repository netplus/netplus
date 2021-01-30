#include <netp/traffic/forwarder_detector.hpp>

#include <netp/traffic/forwarder_iptcp.hpp>
#include <netp/traffic/forwarder_iptcp_payload.hpp>
#include <netp/traffic/forwarder_ipudp.hpp>
#include <netp/traffic/forwarder_ipudp_payload.hpp>
#include <netp/traffic/forwarder_l2.hpp>

namespace netp { namespace traffic {

	NRP<channel_handler_abstract> forwarder_detector::_make_forwarder( forward_type t) {
		switch (t) {
			case forward_type::iptcp:
			{
				NETP_TODO("TOIMPL");
			}
			break;
			case forward_type::iptcp_payload:
			{
				return netp::make_ref<netp::traffic::forwarder_iptcp_payload>();
			}
			break;
			case forward_type::ipudp:
			{
				NETP_TODO("TOIMPL");
			}
			break;
			case forward_type::ipudp_payload:
			{
				NETP_TODO("TOIMPL");
			}
			break;
			case forward_type::l2:
			{
				NETP_TODO("TOIMPL");
			}
			break;
		}
	}


	void forwarder_detector::read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		NETP_ASSERT(income->len() > 0);
		forward_type t = (forward_type)income->read<u8_t>();
		if (t >= forward_type::forward_type_max) {
			NETP_ERR("[forwarder_detector][%s]invalid forward type,close session", ctx->ch->ch_info().c_str());
			ctx->close();
			return;
		}

		ctx->do_remove_from_pipeline(netp::make_ref<netp::promise<int>>());
		NRP<netp::add_handler_promise> addp = netp::make_ref<netp::add_handler_promise>();
		ctx->ch->pipeline()->do_add_last( _make_forwarder(t), addp);
		addp->wait();

		const std::tuple<int, NRP<channel_handler_context>>& addrt = addp->get();
		std::get<1>(addrt)->invoke_connected();
		ctx->invoke_read(income);
	}

}}