#ifndef _NETP_POLLER_ABSTRACT_HPP
#define _NETP_POLLER_ABSTRACT_HPP

#include <netp/core.hpp>
#include <netp/io_monitor.hpp>

#define NETP_DEBUG_IO_CTX_

//@note: for a poll wait for timer >&& timer != infinite, in the following case , one more signal might be leave in the pipe/interrupt_fd
// 1, poll with timer >0 && timer != infinite
// 2, poll return with a timeout
// 3, add a signal into the pipe/interrupt_fd before we reset inwait flag
//in nano
//ENTER HAS A lock_gurard to sure the compiler would not reorder it
#define NETP_POLLER_WAIT_ENTER(W) ((W).store(true,std::memory_order_relaxed))
#define NETP_POLLER_WAIT_EXIT(wt_in_nano,W) ((u64_t(wt_in_nano)>0)) ? (W).store(false,std::memory_order_release) : (void)0;

namespace netp {

	enum io_poller_type {
		T_SELECT, //win&linux&android
		T_IOCP, //win
		T_EPOLL, //linux,epoll,et
		T_KQUEUE,//bsd
		T_POLLER_MAX,
		T_NONE
	};

	enum io_flag {
		IO_READ = 1,
		IO_WRITE = 1 << 1,
		IO_READ_HUP = 1<<2, //read closed by remote peer
		IO_ADD_PENDING = 1<<3, //USED BY SELECT ONLY,
		IO_EPOLL_NOET = 1<<4 //USED BY EPOLL ONLY
	};

	enum class io_action {
		READ = 1 << 0, //check read, sys io
		END_READ = 1 << 1,

		WRITE = 1 << 2, //check write, sys io
		END_WRITE = 1 << 3,

		NOTIFY_TERMINATING = 1 << 4,

		READ_WRITE = (READ|WRITE)
	};

	struct io_ctx
	{
		SOCKET fd;
		u8_t flag;
		NRP<io_monitor> iom;
		io_ctx* prev, *next;
	};
	typedef std::function<void(int status, io_ctx* ctx)> fn_io_event_t;
	inline static io_ctx* io_ctx_allocate(SOCKET fd, NRP<io_monitor> const& iom) {
		io_ctx* ctx = netp::allocator<io_ctx>::make();
		if (ctx != 0) {
			ctx->fd = fd;
			ctx->flag = 0;
			ctx->iom = iom;
		}
		return ctx;
	}

	inline static void io_ctx_deallocate(io_ctx* ctx) {
		NETP_ASSERT((ctx->iom != nullptr) && ((ctx->flag&(io_flag::IO_READ|io_flag::IO_WRITE))==0), "flag: %u", ctx->flag );
		ctx->iom = nullptr;
		netp::allocator<io_ctx>::trash(ctx);
	}

	class poller_abstract:
		public netp::ref_base
	{
	protected:
		io_poller_type m_type;
	public:
		poller_abstract(io_poller_type t):m_type(t) {}
		~poller_abstract() {}

		virtual void init() = 0;
		virtual void deinit() = 0;

		virtual void poll(i64_t wait_in_nano, std::atomic<bool>& waiting) = 0;

		virtual void interrupt_wait() = 0;
		virtual int io_do(io_action, io_ctx*) = 0;
		virtual io_ctx* io_begin(SOCKET,NRP<io_monitor> const& iom) = 0;
		virtual void io_end(io_ctx*) = 0;
		virtual int watch(u8_t,io_ctx*) = 0;
		virtual int unwatch(u8_t, io_ctx*) = 0;
	};
}
#endif