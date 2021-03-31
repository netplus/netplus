#ifndef _NETP_CHANNEL_PIPELINE_HPP
#define _NETP_CHANNEL_PIPELINE_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/address.hpp>

#include <netp/channel_handler_context.hpp>
#include <netp/io_event_loop.hpp>

#define PIPELINE_VOID_FIRE_VOID(NAME) \
	__NETP_FORCE_INLINE void fire_##NAME() const {\
		m_head->fire_##NAME(); \
	}\

#define PIPELINE_VOID_FIRE_INT_1(NAME) \
	__NETP_FORCE_INLINE void fire_##NAME(int i) const { \
		m_head->fire_##NAME(i); \
	}\

#define PIPELINE_VOID_FIRE_PACKET_1(NAME) \
	__NETP_FORCE_INLINE void fire_##NAME(NRP<packet> const& packet_) const {\
		m_head->fire_##NAME(packet_); \
	}\

#define PIPELINE_VOID_FIRE_PACKET_ADDR(NAME) \
	__NETP_FORCE_INLINE void fire_##NAME(NRP<packet> const& packet_, address const& from) const {\
		m_head->fire_##NAME(packet_,from); \
	}\

#define PIPELINE_ACTION_PACKET(NAME) \
	__NETP_FORCE_INLINE void NAME(NRP<promise<int>> const& intp,NRP<packet> const& packet_) const {\
		m_tail->NAME(intp,packet_); \
	}\
	NRP<promise<int>> NAME(NRP<packet> const& packet_) {\
		NRP<promise<int>> intp = netp::make_ref<promise<int>>(); \
		m_tail->NAME(intp,packet_); \
		return intp; \
	}\

#define PIPELINE_ACTION_PACKET_ADDR(NAME) \
	__NETP_FORCE_INLINE void NAME( NRP<promise<int>> const& intp, NRP<packet> const& packet_, address const& to) {\
		m_tail->NAME(intp, packet_,to); \
	}\
	NRP<promise<int>> NAME(NRP<packet> const& packet_, address const& to) {\
		NRP<promise<int>> intp = netp::make_ref<promise<int>>(); \
		m_tail->NAME(intp, packet_,to); \
		return intp; \
	}\

#define PIPELINE_CH_FUTURE_ACTION_VOID(NAME) \
	NRP<promise<int>> NAME() {\
		NRP<promise<int>> intp = netp::make_ref<promise<int>>(); \
		m_tail->NAME(intp); \
		return intp; \
	}\

#define PIPELINE_VOID_ACTION_CH_PROMISE_1(NAME) \
	__NETP_FORCE_INLINE void NAME( NRP<promise<int>> const& intp) {\
		m_tail->NAME(intp); \
	}\

namespace netp {

	typedef netp::promise<std::tuple<int, NRP<channel_handler_context>>> add_handler_promise;

	//class channel;
	class channel_pipeline final :
		public ref_base
	{
		friend class channel;

	private:
		NRP<io_event_loop> m_loop;
		NRP<channel> m_ch;
		//tail,head is boundary
		NRP<channel_handler_context> m_head;
		NRP<channel_handler_context> m_tail;

	public:
		channel_pipeline(NRP<channel> const& ch);
		virtual ~channel_pipeline();

		void init();
		void deinit();

		void do_add_last(NRP<channel_handler_abstract> const& h, NRP<netp::add_handler_promise> const& p ) {
			NETP_ASSERT(m_loop->in_event_loop());
			NETP_ASSERT(m_ch != nullptr);
			NRP<channel_handler_context> ctx = netp::make_ref<channel_handler_context>(m_ch, h);
			ctx->N = m_tail;
			ctx->P = m_tail->P;

			m_tail->P->N = ctx;
			m_tail->P = ctx;

			p->set(std::make_tuple(netp::OK, ctx));
		}

		NRP<netp::add_handler_promise> add_last(NRP<channel_handler_abstract> const& h) {
			NRP<netp::add_handler_promise> p = netp::make_ref<netp::add_handler_promise>();
			m_loop->execute([ppl = NRP<channel_pipeline>(this), h, p]() -> void {
				ppl->do_add_last(h,p);
			});
			return p;
		}

	protected:
		PIPELINE_VOID_FIRE_VOID(connected)
		PIPELINE_VOID_FIRE_VOID(closed)
		PIPELINE_VOID_FIRE_INT_1(error)
		PIPELINE_VOID_FIRE_VOID(read_closed)
		PIPELINE_VOID_FIRE_VOID(write_closed)
		PIPELINE_VOID_FIRE_PACKET_1(read)

		PIPELINE_VOID_FIRE_PACKET_ADDR(readfrom)

		PIPELINE_ACTION_PACKET(write)
		PIPELINE_ACTION_PACKET_ADDR(write_to)

		PIPELINE_CH_FUTURE_ACTION_VOID(close)
		PIPELINE_CH_FUTURE_ACTION_VOID(close_read)
		PIPELINE_CH_FUTURE_ACTION_VOID(close_write)

		PIPELINE_VOID_ACTION_CH_PROMISE_1(close)
		PIPELINE_VOID_ACTION_CH_PROMISE_1(close_read)
		PIPELINE_VOID_ACTION_CH_PROMISE_1(close_write)
	};
}
#endif