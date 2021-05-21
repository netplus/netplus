#ifndef _NETP_CHANNEL_HANDLER_HPP
#define _NETP_CHANNEL_HANDLER_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/promise.hpp>

namespace netp {

	struct address;
	class channel_handler_context;

	enum channel_handler_api {
		CH_ACTIVITY_CONNECTED			= 1,
		CH_ACTIVITY_CLOSED					= 1<< 2,
		CH_ACTIVITY_ERROR					= 1 << 3,
		CH_ACTIVITY_READ_CLOSED		= 1 << 4,
		CH_ACTIVITY_WRITE_CLOSED		= 1 << 5,

		CH_INBOUND_READ					= 1 << 6,
		CH_INBOUND_READ_FROM		= 1 << 7,

		CH_OUTBOUND_WRITE				= 1<< 8,
		CH_OUTBOUND_FLUSH				= 1 << 9,
		CH_OUTBOUND_CLOSE				= 1 << 10,
		CH_OUTBOUND_CLOSE_READ	= 1 << 11,
		CH_OUTBOUND_CLOSE_WRITE	= 1 << 12,

		CH_OUTBOUND_WRITE_TO		= 1<< 13,

		CH_ACTIVITY = (CH_ACTIVITY_CONNECTED|CH_ACTIVITY_CLOSED | CH_ACTIVITY_ERROR | CH_ACTIVITY_READ_CLOSED | CH_ACTIVITY_WRITE_CLOSED ),
		CH_OUTBOUND = (CH_OUTBOUND_WRITE|CH_OUTBOUND_FLUSH | CH_OUTBOUND_CLOSE | CH_OUTBOUND_CLOSE_READ | CH_OUTBOUND_CLOSE_WRITE| CH_OUTBOUND_WRITE_TO),
		CH_INBOUND = (CH_INBOUND_READ|CH_INBOUND_READ_FROM),

		CH_CTX_DEATTACHED = 1<<14
	};

	class channel_handler_abstract :
		public ref_base
	{
		friend class channel_handler_context;
		friend class channel_pipeline;
		u16_t CH_H_FLAG;

	public:
		channel_handler_abstract(u16_t flag) : CH_H_FLAG(flag)
		{
		}

	protected:
		//for activity
		virtual void connected(NRP<channel_handler_context> const& ctx);
		virtual void closed(NRP<channel_handler_context> const& ctx);
		virtual void error(NRP<channel_handler_context> const& ctx, int err);
		virtual void read_closed(NRP<channel_handler_context> const& ctx);
		virtual void write_closed(NRP<channel_handler_context> const& ctx);

		//for inbound
		virtual void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income);

		virtual void readfrom(NRP<channel_handler_context> const& ctx, NRP<packet> const& income, NRP<address> const& from);

		//for outbound
		virtual void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet);
		virtual void flush(NRP<channel_handler_context> const& ctx);

		virtual void close(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx);
		virtual void close_read(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx);
		virtual void close_write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx);

		//for outbound_to
		virtual void write_to(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet, NRP<address> const& to);
	};

	class channel_handler_head :
		public channel_handler_abstract
	{
	public:
		channel_handler_head() :
			channel_handler_abstract(CH_OUTBOUND)
		{}
	protected:
		void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) ;
		void close(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx );
		void close_read(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx );
		void close_write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx);

		void write_to(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet, NRP<address> const& to );
	};

	class channel_handler_tail:
		public channel_handler_abstract
	{
	public:
		channel_handler_tail() :
			channel_handler_abstract(CH_ACTIVITY|CH_INBOUND)
		{}
	protected:
		void connected(NRP<channel_handler_context> const& ctx);
		void closed(NRP<channel_handler_context> const& ctx);
		void error(NRP<channel_handler_context> const& ctx, int err);
		void read_closed(NRP<channel_handler_context> const& ctx);
		void write_closed(NRP<channel_handler_context> const& ctx);

		void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) ;
		void readfrom(NRP<channel_handler_context> const& ctx, NRP<packet> const& income, NRP<address> const& from);
	};
}
#endif
