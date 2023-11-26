#ifndef _NETP_CHANNEL_HANDLER_CONTEXT_HPP
#define _NETP_CHANNEL_HANDLER_CONTEXT_HPP

#include <netp/event_loop.hpp>
#include <netp/channel_handler.hpp>
#include <netp/address.hpp>

//@todo: a new way to optimize handler's virtual function out && add NRP<netp::packet>&& support for fire_read, write
//1, set channel_handler_context::fire_read(NRP<netp::packet> const& pkt) -> template <class packet_t> channel_handler_context::fire_read(packet_t&& pkt)
//2, add a type hint for handler's type info (we'll use this info later)
//3, add template <class packet_t,int api_id> channel_handler_abstract::call(packet_t&& pkt) {
//		handler_real_t* rt = static_cast<handler_real_t*>(this);
//		...
// }
//4, each channel_handler impl it's own read|write cb by a specilized template function with a given api_id
// note: it would increase the learn curve greatly


#ifdef _NETP_DEBUG
	#define _NETP_HANDLER_CONTEXT_ASSERT NETP_ASSERT
#else
	#define _NETP_HANDLER_CONTEXT_ASSERT(...)
#endif
/*head,tail never removed by iterate*/
#define CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,DIR) \
__ctx_iterate_begin: \
	_NETP_HANDLER_CONTEXT_ASSERT(_ctx != nullptr); \
	if (NETP_UNLIKELY(_ctx->H_FLAG&CH_CTX_DEATTACHED)) { \
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


#define VOID_INVOKE(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx); \

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(NAME,HANDLER_FLAG) \
	inline void fire_##NAME() const { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = N; \
		VOID_INVOKE(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME() { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE(NAME,HANDLER_FLAG); \
	}

#define VOID_INVOKE_INT_1(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx,i); \

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_INT_1(NAME,HANDLER_FLAG) \
	inline void fire_##NAME( int i ) const { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = N; \
		VOID_INVOKE_INT_1(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME(int i) { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_INT_1(NAME,HANDLER_FLAG); \
	} \

#define VOID_INVOKE_PACKET(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx,pkt); \


#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(NAME,HANDLER_FLAG) \
	inline void fire_##NAME( NRP<packet> const& pkt ) const { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = N; \
		VOID_INVOKE_PACKET(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME( NRP<packet> const& pkt ) { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_PACKET(NAME,HANDLER_FLAG); \
	} \

#define VOID_INVOKE_PACKET_ADDR(NAME,HANDLER_FLAG) \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,N) \
	_ctx->H->NAME(_ctx,pkt,addr); \

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_ADDR(NAME,HANDLER_FLAG) \
	inline void fire_##NAME( NRP<packet> const& pkt, NRP<address> const& addr ) const { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = N; \
		VOID_INVOKE_PACKET_ADDR(NAME,HANDLER_FLAG); \
	} \
	inline void invoke_##NAME( NRP<packet> const& pkt, NRP<address> const& addr ) { \
		_NETP_HANDLER_CONTEXT_ASSERT(L->in_event_loop()); \
		NRP<channel_handler_context> _ctx = NRP<channel_handler_context>(this); \
		VOID_INVOKE_PACKET_ADDR(NAME,HANDLER_FLAG); \
	} \

//--T_TO_H--BEGIN
#define CH_PROMISE_INVOKE_PREV_PACKET_CH_PROMISE(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context> _ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(intp,_ctx,pkt); \

#define CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_CH_PROMISE(NAME,HANDLER_FLAG) \
private:\
	inline void __##NAME(NRP<promise<int>> const& intp, NRP<packet> const& pkt) { \
		if( NETP_UNLIKELY(H_FLAG&CH_CTX_DEATTACHED) ) {\
			intp->set(netp::E_CHANNEL_CONTEXT_DEATTACHED); \
			return; \
		} \
		CH_PROMISE_INVOKE_PREV_PACKET_CH_PROMISE(NAME,HANDLER_FLAG) \
	} \
public:\
	inline void NAME(NRP<promise<int>> const& intp, NRP<packet> const& pkt) { \
		if(L->in_event_loop()) { \
				/*a copy on ctx,intp, pkt might be saved*/ \
			__##NAME(intp, pkt); \
		} else {\
			L->schedule([ctx=NRP<channel_handler_context>(this),intp, pkt]() { \
				ctx->__##NAME(intp,pkt); \
			}); \
		}\
	} \
	inline NRP<promise<int>> NAME(NRP<packet> const& pkt) { \
		NRP<promise<int>> intp = netp::make_ref<promise<int>>();\
		NAME(intp,pkt); \
		return intp; \
	} \

#define CH_PROMISE_INVOKE_PREV_PACKET_ADDR_CH_PROMISE(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context> _ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(intp,_ctx,pkt,to); \

#define CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_ADDR_CH_PROMISE(NAME,HANDLER_FLAG) \
private:\
	inline void __##NAME(NRP<promise<int>> const& intp, NRP<packet> const& pkt, NRP<address> const& to) { \
		if( NETP_UNLIKELY(H_FLAG&CH_CTX_DEATTACHED) ) {\
			intp->set(netp::E_CHANNEL_CONTEXT_DEATTACHED); \
			return; \
		} \
		CH_PROMISE_INVOKE_PREV_PACKET_ADDR_CH_PROMISE(NAME,HANDLER_FLAG) \
	} \
public:\
	inline void NAME(NRP<promise<int>> const& intp, NRP<packet> const& pkt, NRP<address> const& to) { \
		if(L->in_event_loop()) { \
			__##NAME(intp,pkt,to); \
		} else { \
			L->schedule([ctx=NRP<channel_handler_context>(this),intp, pkt, to]() { \
				ctx->__##NAME(intp,pkt,to); \
			}); \
		}\
	} \
	inline NRP<promise<int>> NAME(NRP<packet> const& pkt, NRP<address> const& to) { \
		NRP<promise<int>> intp = netp::make_ref<promise<int>>();\
		NAME(intp,pkt,to); \
		return intp;\
	} \

#define CH_PROMISE_INVOKE_PREV_CH_PROMISE(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context> _ctx = P; \
	CHANNEL_HANDLER_CONTEXT_ITERATE_CTX(HANDLER_FLAG,P) \
	_ctx->H->NAME(intp,_ctx); \

#define CH_PROMISE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(NAME,HANDLER_FLAG) \
private:\
	inline void __##NAME(NRP<promise<int>> const& intp) { \
		if( NETP_UNLIKELY(H_FLAG&CH_CTX_DEATTACHED) ) {\
			intp->set(netp::E_CHANNEL_CONTEXT_DEATTACHED); \
			return; \
		} \
		CH_PROMISE_INVOKE_PREV_CH_PROMISE(NAME,HANDLER_FLAG) \
	} \
public:\
	inline void NAME(NRP<promise<int>> const& intp) { \
		if(L->in_event_loop()) 	{ \
			__##NAME(intp); \
		} else {\
			L->schedule([ctx = NRP<channel_handler_context>(this), intp]() { \
				ctx->__##NAME(intp); \
			}); \
		}\
	} \
	inline NRP<promise<int>> NAME() { \
		NRP<promise<int>> f = netp::make_ref<promise<int>>();\
		channel_handler_context::NAME(f); \
		return f;\
	} \

#define CH_PROMISE_INVOKE_PREV(NAME,HANDLER_FLAG) \
	NRP<channel_handler_context> _ctx = P; \
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
		NRP<event_loop> L;
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
			H_FLAG |= CH_CTX_DEATTACHED;

			p->set(netp::OK);
		}

		inline bool is_deattached() { return (H_FLAG & CH_CTX_DEATTACHED); }

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