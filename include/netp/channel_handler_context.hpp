#ifndef _NETP_CHANNEL_HANDLER_CONTEXT_HPP
#define _NETP_CHANNEL_HANDLER_CONTEXT_HPP

#include <netp/io_event_loop.hpp>
#include <netp/channel_handler.hpp>
#include <netp/address.hpp>

/*head,tail never removed by iterate*/
#define CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,DIR) \
__ctx_iterate_begin: \
	NETP_ASSERT(_ctx != nullptr); \
	if (NETP_UNLIKELY(_ctx->H_FLAG&CH_CTX_REMOVED)) { \
		_ctx->N->P = _ctx->P; \
		_ctx->P->N = _ctx->N; \
		_ctx = _ctx->DIR; \
		goto __ctx_iterate_begin; \
	} \
	if(!(_ctx->H_FLAG&HANDLER_FLAG)) \
	{ \
		_ctx = _ctx->DIR; \
		goto __ctx_iterate_begin; \
	} \


#define VOID_INVOKE_NEXT(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx); \

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(NAME,HANDLER_FLAG) \
	inline void fire_##NAME() const { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = N; \
		VOID_INVOKE_NEXT(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME() { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_NEXT(NAME,HANDLER_FLAG); \
	}

#define VOID_INVOKE_NEXT_INT_1(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx,i); \

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_INT_1(NAME,HANDLER_FLAG) \
	inline void fire_##NAME( int i ) const { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = N; \
		VOID_INVOKE_NEXT_INT_1(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME(int i) { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_NEXT_INT_1(NAME,HANDLER_FLAG); \
	} \

#define VOID_INVOKE_NEXT_PACKET(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx,p); \


#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(NAME,HANDLER_FLAG) \
	inline void fire_##NAME( NRP<packet> const& p ) const { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = N; \
		VOID_INVOKE_NEXT_PACKET(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME( NRP<packet> const& p ) { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_NEXT_PACKET(NAME,HANDLER_FLAG); \
	} \

#define VOID_INVOKE_NEXT_PACKET_ADDR(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx,p,addr); \

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_ADDR(NAME,HANDLER_FLAG) \
	inline void fire_##NAME( NRP<packet> const& p, address const& addr ) const { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = N; \
		VOID_INVOKE_NEXT_PACKET_ADDR(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME( NRP<packet> const& p, address const& addr ) { \
		NETP_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context>_ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_NEXT_PACKET_ADDR(NAME,HANDLER_FLAG); \
	} \

//--T_TO_H--BEGIN
#define CH_PROMISE_INVOKE_PREV_PACKET_CH_PROMISE(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context>_ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(intp,_ctx,p); \

#define CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_CH_PROMISE(NAME,HANDLER_FLAG) \
private:\
	inline void __##NAME(NRP<promise<int>> const& intp, NRP<packet> const& p) { \
		if( NETP_UNLIKELY(H_FLAG&CH_CTX_REMOVED) ) {\
			intp->set(netp::E_CHANNEL_CONTEXT_REMOVED); \
			return; \
		} \
		CH_PROMISE_INVOKE_PREV_PACKET_CH_PROMISE(NAME,HANDLER_FLAG) \
	} \
public:\
	inline void NAME(NRP<promise<int>> const& intp, NRP<packet> const& p) { \
		L->execute([ctx=NRP<channel_handler_context>(this),intp, p]() { \
			ctx->__##NAME(intp,p); \
		}); \
	} \
	inline NRP<promise<int>> NAME(NRP<packet> const& p) { \
		NRP<promise<int>> intp = netp::make_ref<promise<int>>();\
		NAME(intp,p); \
		return intp; \
	} \

#define CH_PROMISE_INVOKE_PREV_PACKET_ADDR_CH_PROMISE(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context>_ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(intp,_ctx,p,to); \

#define CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_ADDR_CH_PROMISE(NAME,HANDLER_FLAG) \
private:\
	inline void __##NAME(NRP<promise<int>> const& intp, NRP<packet> const& p, address const& to) { \
		if( NETP_UNLIKELY(H_FLAG&CH_CTX_REMOVED) ) {\
			intp->set(netp::E_CHANNEL_CONTEXT_REMOVED); \
			return; \
		} \
		CH_PROMISE_INVOKE_PREV_PACKET_ADDR_CH_PROMISE(NAME,HANDLER_FLAG) \
	} \
public:\
	inline void NAME(NRP<promise<int>> const& intp, NRP<packet> const& p, address const& to) { \
		L->execute([ctx=NRP<channel_handler_context>(this), p, to,intp]() { \
			ctx->__##NAME(intp,p,to); \
		}); \
	} \
	inline NRP<promise<int>> NAME(NRP<packet> const& p, address const& to) { \
		NRP<promise<int>> intp = netp::make_ref<promise<int>>();\
		NAME(intp,p,to); \
		return intp;\
	} \

#define CH_PROMISE_INVOKE_PREV_CH_PROMISE(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context>_ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(intp,_ctx); \

#define CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(NAME,HANDLER_FLAG) \
private:\
	inline void __##NAME(NRP<promise<int>> const& intp) { \
		if( NETP_UNLIKELY(H_FLAG&CH_CTX_REMOVED) ) {\
			intp->set(netp::E_CHANNEL_CONTEXT_REMOVED); \
			return; \
		} \
		CH_PROMISE_INVOKE_PREV_CH_PROMISE(NAME,HANDLER_FLAG) \
	} \
public:\
	inline void NAME(NRP<promise<int>> const& intp) { \
		L->execute([ctx=NRP<channel_handler_context>(this), intp]() { \
			ctx->__##NAME(intp); \
		}); \
	} \
	inline NRP<promise<int>> NAME() { \
		NRP<promise<int>> f = netp::make_ref<promise<int>>();\
		channel_handler_context::NAME(f); \
		return f;\
	} \

#define CH_PROMISE_INVOKE_PREV(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context>_ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(_ctx); \

//--T_TO_H--END

namespace netp {

	class channel;
	class channel_handler_context final:
		public ref_base
	{
	public:
		friend class channel_pipeline;
		NRP<io_event_loop> L;
		NRP<netp::channel> ch;
	private:
		u16_t H_FLAG;
		NRP<channel_handler_context> P;
		NRP<channel_handler_context> N;
		NRP<channel_handler_abstract> H;

	public:
		channel_handler_context(NRP<netp::channel> const& ch_, NRP<channel_handler_abstract> const& h);

		inline void do_remove_from_pipeline(NRP<netp::promise<int>> const& p) {
			NETP_ASSERT(L->in_event_loop());
			//HEAD,TAIL will never BE REMOVED from outside
			NETP_ASSERT(P != nullptr && N != nullptr );
			H_FLAG |= CH_CTX_REMOVED;
			p->set(netp::OK);
		}

		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(connected, CH_ACTIVITY_CONNECTED)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(closed, CH_ACTIVITY_CLOSED)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(read_closed, CH_ACTIVITY_READ_CLOSED)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(write_closed, CH_ACTIVITY_WRITE_CLOSED)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_INT_1(error, CH_ACTIVITY_ERROR)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(read, CH_INBOUND_READ)

		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_ADDR(readfrom, CH_INBOUND_READ_FROM)

		CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_CH_PROMISE(write, CH_OUTBOUND_WRITE)
		CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(close, CH_OUTBOUND_CLOSE)
		CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(close_read, CH_OUTBOUND_CLOSE_READ)
		CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(close_write, CH_OUTBOUND_CLOSE_WRITE)

		CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_ADDR_CH_PROMISE(write_to, CH_OUTBOUND_WRITE_TO);
	};
}
#endif