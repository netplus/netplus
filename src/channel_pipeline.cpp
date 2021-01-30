#include <netp/channel_pipeline.hpp>
#include <netp/channel.hpp>

namespace netp {

	channel_pipeline::channel_pipeline(NRP<channel> const& ch):
		m_loop(ch->L),
		m_ch(ch)
	{
		NETP_TRACE_CHANNEL("channel_pipeline::channel_pipeline()");
	}

	channel_pipeline::~channel_pipeline()
	{
		NETP_TRACE_CHANNEL("channel_pipeline::~channel_pipeline()");
	}

	void channel_pipeline::init()
	{
		NRP<channel_handler_head> h = netp::make_ref<channel_handler_head>();
		m_head = netp::make_ref<channel_handler_context>(m_ch, h);

		NRP<channel_handler_tail> t = netp::make_ref<channel_handler_tail>();
		m_tail = netp::make_ref<channel_handler_context>(m_ch, t);

		m_head->P = nullptr;
		m_head->N = m_tail;
		m_tail->P = m_head;
		m_tail->N = nullptr;
	}

	void channel_pipeline::deinit()
	{
		NRP<channel_handler_context> _hctx = m_tail;
		while (_hctx != nullptr ) {
			_hctx->H_FLAG |= CH_CTX_REMOVED; //clear all
			_hctx->N = nullptr;
			_hctx->H = nullptr;
			_hctx = _hctx->P;
		}
	}
}