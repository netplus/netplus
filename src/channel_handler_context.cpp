#include <netp/channel.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel_handler_context.hpp>

namespace netp {

	channel_handler_context::channel_handler_context(NRP<netp::channel> const& ch_, NRP<channel_handler_abstract> const& h):
		L(ch_->L), ch(ch_), H_FLAG(h->CH_H_FLAG), P(nullptr), N(nullptr), H(h)
	{
	}
}