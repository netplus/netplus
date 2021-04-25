#ifndef _NETP_HEAP_HPP
#define _NETP_HEAP_HPP
#include <netp/core.hpp>

#define BHEAP_P(i) ((i-1)>>1)
#define BHEAP_L(i) ((i<<1)+1)
#define BHEAP_R(i) ((i<<1)+2)
//#define DEFAULT_BHEAP_CAPACITY 1024
namespace netp {
	template <class T, class Fx_compare = less<T>, netp::size_t initial_capacity = 1000 >
	class binary_heap final
	{
		Fx_compare __fn_cmp__;
		T* m_arr;
		netp::size_t m_size;
		netp::size_t m_capacity;
		inline void __realloc__(netp::size_t count ) {
			NETP_ASSERT(count > m_size);
			T* arr = netp::allocator<T>::make_array(count);

			NETP_ALLOC_CHECK(arr, count * sizeof(T));
			for (u32_t _i = 0; _i < m_size; ++_i) {
				arr[_i] = std::move(m_arr[_i]);
			}
			netp::allocator<T>::trash_array(m_arr, m_capacity);
			m_arr=arr;
			m_capacity = count;
		}

	public:
		binary_heap() :
			m_arr(nullptr),
			m_size(0),
			m_capacity(initial_capacity) 
		{
			m_arr = netp::allocator<T>::make_array(m_capacity);
			NETP_ALLOC_CHECK(m_arr, m_capacity * sizeof(T));
		}

		~binary_heap() { 
			netp::allocator<T>::trash_array(m_arr,m_capacity);
		}
		inline netp::size_t size() const { return m_size; }
		inline bool empty() const { return m_size == 0; }

		inline void reserve(netp::size_t count ) {
			if ( (count == m_capacity) || count < ((m_size>>1) + m_size) ) {
				return;
			}
			if (count < initial_capacity) {
				count = initial_capacity;
			}
			__realloc__(count);
		}

		void push(T&& t) {
			if ( NETP_UNLIKELY(m_capacity==m_size)) {
				__realloc__((m_size + (m_size >> 1)));
			}

			netp::size_t i = m_size++;
			netp::size_t p = BHEAP_P(i);

			while ((i != 0) && (__fn_cmp__(std::forward<T>(t), m_arr[p]))) {
				m_arr[i] = std::move(m_arr[p]);
				i = p;
				p = BHEAP_P(i);
			}

			m_arr[i] = std::forward<T>(t);
		}

		inline T& front() {
			NETP_ASSERT(m_size > 0);
			return m_arr[0];
		}

		inline T const& front() const {
			NETP_ASSERT(m_size > 0);
			return m_arr[0];
		}

		void pop() {
			NETP_ASSERT(m_size>0);
			--m_size;

			netp::size_t i_ = 0;
			while ( true ) {
				const netp::size_t r = BHEAP_R(i_);
				netp::size_t l = r-1;

				if (NETP_LIKELY(r<m_size)) {
					if (__fn_cmp__(m_arr[r], m_arr[l])) {
						l = r;
					}
				} else if (l<m_size) {
				} else {
					break;
				}
				if (__fn_cmp__( m_arr[m_size], m_arr[l])) {
					break;
				}
				m_arr[i_] = std::move(m_arr[l]);
				i_ = l;
			}
			m_arr[i_] = std::move(m_arr[m_size]);
		}
	};

	template <class T>
	struct binomial_node {
		binomial_node* parent;
		binomial_node* child;
		binomial_node* sibling;
		u32_t degree;
		T data;

		binomial_node() : parent(nullptr), child(nullptr), sibling(nullptr),degree(0),data()	{}
		~binomial_node(){}
	};

	template <class T, class Fx_compare = less<T> >
	class binomial_heap final
	{
		typedef binomial_heap<T, Fx_compare> binomial_heap_t;
		typedef binomial_node<T> binomial_node_t;
		Fx_compare __fn_cmp__;

		u32_t m_size;
		binomial_node_t* m_head;

		inline binomial_node<T>* __lookup_front(binomial_node_t** prev) {
			assert(!empty());
			binomial_node_t* ret = m_head;
			binomial_node_t* curr = m_head;
			binomial_node_t* t_prev = nullptr;
			while (curr != nullptr) {
				if (__fn_cmp__(curr->data, ret->data)) {
					*prev = t_prev;
					ret = curr;
				}
				t_prev = curr;
				curr = curr->sibling;
			}
			return ret;
		}

		inline static binomial_node_t* __reverse_node_and_set_parent_null(binomial_node_t* const n) {
			binomial_node_t* prev = nullptr;
			binomial_node_t* curr = n;
			while (curr != nullptr) {
				binomial_node_t* next = curr->sibling;
				curr->sibling = prev;
				curr->parent = nullptr;
				prev = curr;
				curr = next;
			}
			return prev;
		}

		inline static binomial_node_t* __merge(binomial_heap_t* const l, binomial_heap_t* const r) {
			binomial_node_t* h = nullptr;
			binomial_node_t** curr = &h;

			binomial_node_t* ln = l->m_head;
			binomial_node_t* rn = r->m_head;

			while (ln != nullptr && rn != nullptr) {
				if (ln->degree <= rn->degree) {
					*curr = ln;
					ln = ln->sibling;
				} else {
					*curr = rn;
					rn = rn->sibling;
				}

				curr = &(*curr)->sibling;
			}

			if (ln != nullptr) {
				*curr = ln;
			} else if (rn != nullptr) {
				*curr = rn;
			} else {
				//assert(!"what");
			}
			return h;
		}

		//link l -> r
		inline static void __link(binomial_node_t* c, binomial_node_t* p) {
			c->parent = p;
			++p->degree;
			c->sibling = p->child;
			p->child = c;
		}

		template<class Fx_cmp>
		static binomial_node_t* __union(binomial_heap_t* l, binomial_heap_t* r, Fx_cmp& fn_cmp) {
			binomial_node_t* nhead = __merge(l, r);

			if (nhead == nullptr) return nullptr;

			binomial_node_t* prev = nullptr;
			binomial_node_t* curr = nhead;
			binomial_node_t* next = nhead->sibling;

			while (next != nullptr) {
				if (curr->degree != next->degree ||
					((next->sibling != nullptr) && next->sibling->degree == next->degree)) {
					prev = curr;
					curr = next;
				} else if (fn_cmp(curr->data, next->data)) {
					curr->sibling = next->sibling;
					__link(next, curr);
				} else {
					if (prev == nullptr) {
						nhead = next;
					} else {
						prev->sibling = next;
					}

					__link(curr, next);
					curr = next;
				}
				next = curr->sibling;
			}
			return nhead;
		}
	public:
		binomial_heap() :m_head(nullptr),m_size(0)	{}
		~binomial_heap() {}

		inline u32_t size() const { return m_size; }
		inline bool empty() const { return m_size == 0; }

		void push(binomial_node_t* node) {
			if (m_head == nullptr) {
				m_head = node;
			} else {
				binomial_heap_t _h;
				_h.m_head = node;
				m_head = __union(this, &_h, __fn_cmp__);
			}
			++m_size;
		}

		inline binomial_node<T>* front() {
			//lookup from m_head till a sibling -> nil
			assert(!empty());
			binomial_node_t* p = nullptr;
			return __lookup_front(&p);
		}

		inline binomial_node<T> const* front() const {
			binomial_node_t* p = nullptr;
			return __lookup_front(&p);
		}

		void pop() {
			binomial_node_t* p = nullptr;
			binomial_node_t* ret = __lookup_front(&p);
			if (ret == nullptr) return;

			if (p == nullptr) {
				assert(ret == m_head);
				m_head = m_head->sibling;
			} else {
				p->sibling = ret->sibling;
			}

			binomial_node_t* new_h = __reverse_node_and_set_parent_null(ret->child);
			binomial_heap_t _h;
			_h.m_head = new_h;

			m_head = __union(this, &_h, __fn_cmp__);
			--m_size;
		}
	};

	template <class T>
	struct fibonacci_node:
		public netp::ref_base
	{
		fibonacci_node* parent;
		fibonacci_node* child;
		fibonacci_node* left;
		fibonacci_node* right;
		T data;
		unsigned int degree;
		bool marked;
		
		fibonacci_node() :parent(nullptr),child(nullptr),left(nullptr),right(nullptr),degree(0),marked(false) {}
		~fibonacci_node(){}
	};

	template <class T, class Fx_compare = less<T> >
	class fibonacci_heap final
	{
		typedef fibonacci_heap<T, Fx_compare> heap_t;
		typedef fibonacci_node<T> node_t;

		Fx_compare __fn_cmp__;
		u32_t m_size;
		u32_t m_nheap;
		node_t* m_min;

		inline static void __insert_before(node_t* r, node_t* n) {
			assert(r != nullptr);
			assert(n != nullptr);
			assert(r != n);

			n->right = r;
			n->left = r->left;

			r->left->right = n;
			r->left = n;
		}
		inline static void __remove_node(node_t* n) {
			n->left->right = n->right;
			n->right->left = n->left;
		}

		inline static void __consolidate(heap_t* H) {
			assert(H != nullptr);
			assert(H->m_min != nullptr);

			node_t* node_arr[64] = { nullptr };
			node_t* end_node = H->m_min->left;

			node_t* curr_n = H->m_min;
			unsigned int dmax = 0;

			bool exit = false;
			do {
				node_t* next_n = curr_n->right; //tmp
				unsigned int d = curr_n->degree;

				if ((next_n == curr_n) || (curr_n == end_node)) {
					assert(((next_n == curr_n)) ? H->m_nheap == 1 : true);
					exit = true;
				}

				while (node_arr[d] != nullptr) {
					assert(node_arr[d] != curr_n);
					if (H->__fn_cmp__(node_arr[d]->data, curr_n->data)) {
						std::swap(node_arr[d], curr_n);
					}
					__link(node_arr[d], curr_n);
					node_arr[d] = nullptr;
					++d;
					--H->m_nheap;
				}
				node_arr[d] = curr_n;
				if (d > dmax) {
					dmax = d;
				}
				curr_n = next_n;
			} while (!exit);

			H->m_min = nullptr;
			for (unsigned int i = 0; i <= dmax; ++i) {
				if (node_arr[i] != nullptr) {
					if (H->m_min == nullptr) {
						H->m_min = node_arr[i];
						H->m_min->left = node_arr[i];
						H->m_min->right = node_arr[i];
					}
					else {
						__insert_before(H->m_min, node_arr[i]);
						if (H->__fn_cmp__(node_arr[i]->data, H->m_min->data)) {
							H->m_min = node_arr[i];
						}
					}
				}
			}
		}

		inline static void __link(node_t* c, node_t* p) {
			assert(c != p);
			__remove_node(c);
			if (p->child == nullptr) {
				p->child = c;
				c->left = c;
				c->right = c;
			}
			else {
				__insert_before(p->child, c);
			}
			c->parent = p;
			c->marked = false;
			++p->degree;
		}
	public:
		fibonacci_heap() :	m_min(nullptr){}
		~fibonacci_heap(){}

		void push(node_t* n) {
			if (m_min == nullptr) {
				n->left = n;
				n->right = n;
				m_min = n;
			} else {
				__insert_before(m_min, n);
				assert(m_min != nullptr);
				if (__fn_cmp__(n->data, m_min->data)) {
					m_min = n;
				}
			}
			++m_nheap;
			++m_size;
		}

		inline node_t* const front() const {
			assert(m_size > 0);
			assert(m_min != nullptr);
			return m_min;
		}

		inline node_t* front() {
			assert(m_size > 0);
			assert(m_min != nullptr);
			return m_min;
		}

		void pop() {
			assert(front() != nullptr);
			node_t* child = m_min->child;

			if (child != nullptr) {
				node_t* _c = child->right;
				++m_nheap;
				__insert_before(m_min, child);
				child = _c;
				while (child != m_min->child) {
					++m_nheap;
					node_t* _c = child->right;
					__insert_before(m_min, child);
					child = _c;
				}
			}

			--m_size;
			--m_nheap;
			if (m_min == m_min->right) {
				m_min = nullptr;
				assert(m_size == 0);
				assert(m_nheap == 0);
			} else {
				__remove_node(m_min);
				m_min = m_min->right;
				__consolidate(this);
			}
		}

		inline u32_t size() const {	return m_size; }
		inline bool empty() const {	return m_size == 0; }
	};
}
#endif