#ifndef _NETP_POLLER_ABSTRACT_HPP
#define _NETP_POLLER_ABSTRACT_HPP

#include <netp/core.hpp>
#include <netp/io_monitor.hpp>

#define NETP_DEBUG_IO_CTX_

//in nano
#define NETP_POLLER_WAIT_IGNORE_DUR (u64_t(27))
//ENTER HAS A lock_gurard to sure the compiler would not reorder it
#define NETP_POLLER_WAIT_ENTER(wt_in_nano,W) ( ((u64_t(wt_in_nano)>NETP_POLLER_WAIT_IGNORE_DUR)) ? (W).store(true,std::memory_order_relaxed): (void)0)
#define NETP_POLLER_WAIT_EXIT(wt_in_nano,W) std::atomic_signal_fence(std::memory_order_acq_rel);((u64_t(wt_in_nano)>NETP_POLLER_WAIT_IGNORE_DUR)) ? (W).store(false,std::memory_order_relaxed) : (void)0; std::atomic_signal_fence(std::memory_order_acq_rel);

namespace netp {

	enum io_poller_type {
		T_SELECT, //win&linux&android
		T_IOCP, //win
		T_EPOLL, //linux,epoll,et
		T_KQUEUE,//bsd
		T_POLLER_CUSTOM_1,
		T_POLLER_CUSTOM_2,
		T_POLLER_MAX,
		T_NONE
	};

	enum io_flag {
		IO_READ = 1,
		IO_WRITE = 1 << 1
	};

	enum class io_action {
		READ = 1 << 0, //check read, sys io
		END_READ = 1 << 1,

		WRITE = 1 << 2, //check write, sys io
		END_WRITE = 1 << 3,

		NOTIFY_TERMINATING = 1 << 4,

		READ_WRITE = (READ | WRITE)
	};

	struct io_ctx
	{
		io_ctx* prev, * next;
		SOCKET fd;
		u8_t flag;
		NRP<io_monitor> iom;
	};
	typedef std::function<void(int status, io_ctx* ctx)> fn_io_event_t;
	inline static io_ctx* io_ctx_allocate(SOCKET fd, NRP<io_monitor> const& iom) {
		io_ctx* ctx = netp::allocator<io_ctx>::make();
		ctx->fd = fd;
		ctx->flag = 0;
		ctx->iom = iom;
		return ctx;
	}

	inline static void io_ctx_deallocate(io_ctx* ctx) {
		NETP_ASSERT(ctx->iom != nullptr);
		ctx->iom = nullptr;
		netp::allocator<io_ctx>::trash(ctx);
	}


	class poller_abstract:
		public netp::ref_base
	{
	protected:

	public:
		poller_abstract() {}
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