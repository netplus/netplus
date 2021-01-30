#ifndef _NETP_HANDLER_FRAGMENT_HPP
#define _NETP_HANDLER_FRAGMENT_HPP
#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/channel_handler.hpp>

namespace netp { namespace handler {
class fragment final :
	public netp::channel_handler_abstract {
		u32_t m_fragment_maxium_size;
		public:
			fragment(u32_t maxium ):
				channel_handler_abstract(CH_INBOUND_READ),
				m_fragment_maxium_size(maxium) {}

			void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override {
				NETP_ASSERT(income != nullptr);

_check_begin:
				if (income->len() <= m_fragment_maxium_size) {
					ctx->fire_read(income);
					return ;
				}

				NRP<netp::packet> _in=netp::make_ref<netp::packet>(income->head(),m_fragment_maxium_size);
				income->skip(m_fragment_maxium_size);
				ctx->fire_read(_in);
				goto _check_begin;
			}
	};

}}
#endif