#ifndef _NETP_LIST_HPP
#define _NETP_LIST_HPP

#include <netp/core.hpp>

namespace netp {
	struct list_t {
		list_t* next;
		list_t *prev;
	};
	inline static void list_init(list_t* list) {
		list->next = list;
		list->prev = list;
	}
	inline static void __list_insert(list_t* prev, list_t* next, list_t* item) {
		item->next = next;
		item->prev = prev;
		next->prev = item;
		prev->next = item;
	}
	inline static void list_prepend(list_t* list, list_t* item) {
		__list_insert(list, list->next, item);
	}
	inline static void list_append(list_t* list, list_t* item) {
		__list_insert(list->prev, list, item);
	}
	inline static void list_delete(list_t* item) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->next = 0;
		item->prev = 0;
	}

#define NETP_LIST_IS_EMPTY(list) ( ((list) == (list)->next) && ((list)==(list)->prev) )
#define NETP_LIST_ITEM(ptr, type, member) ( (type*) (void*)((char*)(ptr) -offsetof(type,member) ) )
#define NETP_LIST_FOR(pos, list) for (pos = (list)->next; pos != (list); pos = pos->next)
#define NETP_LIST_SAFE_FOR(pos, n, list) for (pos = (list)->next, n = pos->next; pos != (list); pos = n, n = pos->next)

}

#endif