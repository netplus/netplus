#ifndef _NETP_POLLER_ABSTRACT_HPP
#define _NETP_POLLER_ABSTRACT_HPP

#include <netp/core.hpp>
#include <netp/io_monitor.hpp>

#ifdef _DEBUG
	#define NETP_DEBUG_IO_CTX_
#endif

//in nano
#define NETP_POLLER_WAIT_IGNORE_DUR (27)
namespace netp {

	template<class list_t>
	inline static void list_init(list_t* list) {
		list->next = list;
		list->prev = list;
	}
	template<class list_t>
	inline static void __list_insert(list_t* prev, list_t* next, list_t* item) {
		item->next = next;
		item->prev = prev;
		next->prev = item;
		prev->next = item;
	}
	template<class list_t>
	inline static void list_prepend(list_t* list, list_t* item) {
		__list_insert(list, list->next, item);
	}
	template<class list_t>
	inline static void list_append(list_t* list, list_t* item) {
		__list_insert(list->prev, list, item);
	}
	template<class list_t>
	inline static void list_delete(list_t* item) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->next = 0;
		item->prev = 0;
	}
#define NETP_IO_CTX_LIST_IS_EMPTY(list) ( ((list) == (list)->next) && ((list)==(list)->prev) )

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


	class poller_abstract:
		public netp::ref_base
	{
	protected:

	public:
		poller_abstract() {}
		~poller_abstract() {}

		virtual void init() = 0;
		virtual void deinit() = 0;

#define __LOOP_EXIT_WAITING__(_W_) (_W_.store(false, std::memory_order_release))
		virtual void poll(long long wait_in_nano, std::atomic<bool>& waiting) = 0;

		virtual void interrupt_wait() = 0;
		virtual int io_do(io_action, io_ctx*) = 0;
		virtual io_ctx* io_begin(SOCKET,NRP<io_monitor> const& iom) = 0;
		virtual void io_end(io_ctx*) = 0;
		virtual int watch(u8_t,io_ctx*) = 0;
		virtual int unwatch(u8_t, io_ctx*) = 0;
	};
}
#endif