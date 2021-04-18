#ifndef _NETP_LIST_HPP
#define _NETP_LIST_HPP

#include <netp/core.hpp>

namespace netp {
	/*
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
	*/


	//embed next,prev in your own class to use these api
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
//#define NETP_IO_CTX_LIST_IS_EMPTY(list) ( ((list) == (list)->next) && ((list)==(list)->prev) )

#define NETP_LIST_IS_EMPTY(list) ( ((list) == (list)->next) && ((list)==(list)->prev) )

//by using of c++ template, we do not need this feature to checkout the struct addr, additionally,  offsetof is not safe in c++ any more
//@@@@BUT WE LOST A CONVIENT WAY TO PUT DIFFERENT STRUCT TYPE INTO THE SAME LIST
//#define NETP_LIST_ITEM(ptr, type, member) ( (type*) (void*)((char*)(ptr) -offsetof(type,member) ) )

#define NETP_LIST_FOR(cur, list) for (cur = (list)->next; cur != (list); cur = cur->next)

//safe for means it's safe to del node other than the right nxt one..if we del nxt, we get crashed while dereference nxt (1, assign nxt -> cur, 2, dereference cur)
#define NETP_LIST_SAFE_FOR(cur, nxt, list) for (cur = (list)->next, nxt = cur->next; cur != (list); cur = nxt, nxt = cur->next)

}

#endif