
#include <netp/app.hpp>
#include <netp/channel.hpp>
#include <netp/channel_handler.hpp>
#include <netp/channel_pipeline.hpp>
#include <netp/channel_handler_context.hpp>

#include <netp/address.hpp>

#define VOID_FIRE_HANDLER_DEFAULT_IMPL_0(NAME,FLAG,HANDLER_NAME) \
	void HANDLER_NAME::NAME(NRP<channel_handler_context> const& ctx) { \
		/*ctx->fire_##NAME();*/ \
		NETP_ASSERT(CH_H_FLAG & FLAG); \
		NETP_THROW(#FLAG "MUST IMPL ITS OWN "#NAME); \
		(void)ctx; \
	}

#define VOID_FIRE_HANDLER_DEFAULT_IMPL_INT_1(NAME,FLAG,HANDLER_NAME) \
	void HANDLER_NAME::NAME(NRP<channel_handler_context> const& ctx, int err) { \
		/*ctx->fire_##NAME(err); */\
		NETP_ASSERT(CH_H_FLAG & FLAG); \
		NETP_THROW(#FLAG"MUST IMPL ITS OWN "#NAME); \
		(void)ctx; \
		(void)err; \
	}

#define VOID_HANDLER_DEFAULT_IMPL_0(NAME,FLAG,HANDLER_NAME) \
	void HANDLER_NAME::NAME(NRP<channel_handler_context> const& ctx) { \
		/*ctx->NAME(); */\
		NETP_ASSERT(CH_H_FLAG & FLAG); \
		NETP_THROW(#FLAG"MUST IMPL ITS OWN "#NAME); \
		(void)ctx; \
	}

#define VOID_HANDLER_DEFAULT_IMPL_PROMISE(NAME,FLAG, HANDLER_NAME) \
	void HANDLER_NAME::NAME(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx) { \
		/*ctx->NAME(chp); */\
		NETP_ASSERT(CH_H_FLAG & FLAG); \
		NETP_THROW(#FLAG"MUST IMPL ITS OWN "#NAME); \
		(void)intp; \
		(void)ctx; \
	}

namespace netp {

	//default impl
	VOID_FIRE_HANDLER_DEFAULT_IMPL_0(connected, CH_ACTIVITY_CONNECTED, channel_handler_abstract)
	VOID_FIRE_HANDLER_DEFAULT_IMPL_0(closed, CH_ACTIVITY_CLOSED, channel_handler_abstract)
	VOID_FIRE_HANDLER_DEFAULT_IMPL_INT_1(error, CH_ACTIVITY_ERROR, channel_handler_abstract)
	VOID_FIRE_HANDLER_DEFAULT_IMPL_0(read_closed, CH_ACTIVITY_READ_CLOSED, channel_handler_abstract)
	VOID_FIRE_HANDLER_DEFAULT_IMPL_0(write_closed, CH_ACTIVITY_WRITE_CLOSED, channel_handler_abstract)
	
	//VOID_FIRE_HANDLER_DEFAULT_IMPL_0(write_block, CH_ACTIVITY_WRITE_BLOCK, channel_handler_abstract)
	//VOID_FIRE_HANDLER_DEFAULT_IMPL_0(write_unblock, CH_ACTIVITY_WRITE_UNBLOCK, channel_handler_abstract)

	void channel_handler_abstract::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT(CH_H_FLAG & CH_INBOUND_READ);
		NETP_THROW("CH_INBOUND_READ MUST IMPL ITS OWN read");
		(void)ctx;
		(void)income;
	}

	void channel_handler_abstract::readfrom(NRP<channel_handler_context> const& ctx, NRP<packet> const& income, NRP<address> const& from) {
		NETP_ASSERT(CH_H_FLAG & CH_INBOUND_READ_FROM);
		NETP_THROW("CH_INBOUND_READ_FROM MUST IMPL ITS OWN readfrom");
		(void)ctx;
		(void)income;
		(void)from;
	}
	
	void channel_handler_abstract::write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
		NETP_ASSERT(CH_H_FLAG & CH_OUTBOUND_WRITE);
		NETP_THROW("CH_OUTBOUND_WRITE MUST IMPL ITS OWN write");
		(void)intp;
		(void)ctx;
		(void)outlet;
	}

	VOID_HANDLER_DEFAULT_IMPL_0(flush, CH_OUTBOUND_FLUSH, channel_handler_abstract)
	VOID_HANDLER_DEFAULT_IMPL_PROMISE(close, CH_OUTBOUND_CLOSE,channel_handler_abstract)
	VOID_HANDLER_DEFAULT_IMPL_PROMISE(close_read, CH_OUTBOUND_CLOSE_READ,channel_handler_abstract)
	VOID_HANDLER_DEFAULT_IMPL_PROMISE(close_write, CH_OUTBOUND_CLOSE_WRITE, channel_handler_abstract)


	void channel_handler_abstract::write_to(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet, NRP<address> const& to) {
		NETP_ASSERT(CH_H_FLAG & CH_OUTBOUND_WRITE_TO);
		NETP_THROW("CH_OUTBOUND_WRITE_TO MUST IMPL ITS OWN write_to");
		(void)intp;
		(void)ctx;
		(void)outlet;
		(void)to;
	}

	void channel_handler_head::write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet ) {
		ctx->ch->ch_write_impl(intp,outlet);
	}

	void channel_handler_head::close(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx) {
		ctx->ch->ch_close_impl(intp);
	}

	void channel_handler_head::close_read(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx ) {
		ctx->ch->ch_close_read_impl(intp);
	}

	void channel_handler_head::close_write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx ) {
		ctx->ch->ch_close_write_impl(intp);
	}

	void channel_handler_head::write_to(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet, NRP<address> const& to) {
		ctx->ch->ch_write_to_impl(intp, outlet, to);
	}

	void channel_handler_tail::connected(NRP<channel_handler_context> const& ctx) {
		NETP_TRACE_CHANNEL("[#%s][tail]channel connected, no action", ctx->ch->ch_info().c_str() );
		(void)ctx;
	}
	void channel_handler_tail::closed(NRP<channel_handler_context > const& ctx) {
		//NETP_ASSERT(ctx->ch != nullptr);
		NETP_TRACE_CHANNEL("[#%s][tail]channel closed, no action", ctx->ch->ch_info().c_str() );
		(void)ctx;
	}
	void channel_handler_tail::error(NRP<channel_handler_context > const& ctx, int err) {
		//NETP_ASSERT(ctx->ch != nullptr);
		NETP_ERR("[#%s][tail]channel error, no action, err: %d", ctx->ch->ch_info().c_str(), err);
		(void)err;
		(void)ctx;
	}
	void channel_handler_tail::read_closed(NRP<channel_handler_context> const& ctx) {
		//NETP_ASSERT(ctx->ch != nullptr);
		NETP_TRACE_CHANNEL("[#%s][tail]channel read_closed, close write", ctx->ch->ch_info().c_str() );
		ctx->ch->ch_close_write();
		(void)ctx;
	}
	void channel_handler_tail::write_closed(NRP<channel_handler_context> const& ctx) {
		//NETP_ASSERT(ctx->ch != nullptr);
		NETP_TRACE_CHANNEL("[#%s][tail]channel write_closed, close read", ctx->ch->ch_info().c_str() );
		ctx->ch->ch_close_read();
		(void)ctx;
	}

	void channel_handler_tail::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		//NETP_ASSERT(ctx->ch != nullptr);
		NETP_ERR("[#%s][tail]channel read, we reach the end of the pipeline , please check your pipeline configure, no action", ctx->ch->ch_info().c_str() );
		(void)ctx;
		(void)income;
	}

	void channel_handler_tail::readfrom(NRP<channel_handler_context> const& ctx, NRP<packet> const& income, NRP<address> const& from) {
		//NETP_ASSERT(ctx->ch != nullptr);
		NETP_ERR("[#%s][tail]channel readfrom, we reach the end of the pipeline , please check your pipeline configure, no action, from: %s", ctx->ch->ch_info().c_str(), from->to_string().c_str() );
		(void)ctx;
		(void)income;
	}
}