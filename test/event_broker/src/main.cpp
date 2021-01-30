#include <netp.hpp>
//#include "server_node.hpp"

#include <iostream>

//typedef int(*foo_t)(int);
//typedef std::function<int(int)> foo_tt;

void fooo_do_nothing(int arg) {
	//NETP_INFO("arg: %d", arg);
	(void)0;
//	return arg;
}

typedef decltype(fooo_do_nothing) _fooo_do_nothing_t;
typedef decltype( std::bind(&fooo_do_nothing, std::placeholders::_1) ) fooo_do_nothing_bind_t;

typedef std::function<void(int)> event_handler_int_t;
typedef std::function<void(int, int)> event_handler_int_int_t;

class user_event_trigger final:
	public netp::ref_base,
	public netp::event_broker_any
{
public:
};

class user_event_handler:
	public netp::ref_base
{
public:
	long call_count;
	user_event_handler() :call_count(0)
	{}

	void foo_int(int arg) {
		//NETP_DEBUG("cfoo::foo(), arg: %d", arg);
		call_count++;
	}
	void foo_int_int(int a, int b) {
		//NETP_DEBUG("cfoo::foo(), arg: %d, %d", a, b);
		call_count++;
	}
};


struct benchmark {
	std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> begin;
	std::string m_tag;
	benchmark(std::string const& tag): m_tag(tag),
		begin(std::chrono::steady_clock::now())
	{
	}

	~benchmark() {
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> end = std::chrono::steady_clock::now();
		NETP_INFO("[%s][end]cost: %lld micro second", m_tag.c_str(), ((end - begin).count()/1000) );
	}

	void mark(std::string const& tag ) {
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> end = std::chrono::steady_clock::now();
		NETP_INFO("[%s][%s]cost: %lld micro second", m_tag.c_str(), tag.c_str(), ((end - begin).count() / 1000));
	}
};


enum invoke_type {
	t_invoke,
	t_dynamic,
	t_static,
	t_virtual_read_callee_address_invoke
};

template<typename invoke_hint, typename... _Args>
void invoke_test(NRP<user_event_trigger> const&ut, int t, _Args&&... args) {
	switch (t) {
	case t_invoke:
	{
		ut->invoke<invoke_hint>(std::forward<_Args>(args)...);
	}
	break;
#ifdef __NETP_DEBUG_BROKER_INVOKER_
	case t_dynamic:
	{
		ut->dynamic_invoke<invoke_hint>(std::forward<_Args>(args)...);
	}
	break;
	case t_static:
	{
		ut->static_invoke<invoke_hint>(std::forward<_Args>(args)...);
	}
	break;
	case t_virtual_read_callee_address_invoke:
	{
		ut->virtual_read_callee_address_invoke<invoke_hint>(std::forward<_Args>(args)...);
	}
	break;
#endif
	}
}

void test_case(int t) {

	NRP<user_event_trigger> user_et = netp::make_ref<user_event_trigger>();
	NRP<user_event_handler> user_evh = netp::make_ref<user_event_handler>();

	invoke_test<event_handler_int_t>(user_et, t, 1, 0);
	invoke_test<event_handler_int_int_t>(user_et, t, 2, 0,0);

	int expected_callcount = 0;
	int bind_1_count = 0;
	int bind_2_count = 0;

	event_handler_int_t h_frombind = std::bind(&user_event_handler::foo_int, user_evh, std::placeholders::_1);
	user_et->bind<event_handler_int_t>(1, h_frombind);
	++bind_1_count;

	user_et->bind<event_handler_int_t>(1, &user_event_handler::foo_int, user_evh, std::placeholders::_1);
	++bind_1_count;
	invoke_test<event_handler_int_t>(user_et, t, 1, 13);
	expected_callcount = (expected_callcount + bind_1_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	invoke_test<event_handler_int_t>(user_et, t, 1, 13);
	expected_callcount = (expected_callcount + bind_1_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	event_handler_int_t h_frombind_2 = std::bind(&fooo_do_nothing, std::placeholders::_1);
	user_et->bind<event_handler_int_t>(14, h_frombind_2);
	//++bind_1_count;

	user_et->bind<event_handler_int_t>(14, std::bind(&fooo_do_nothing, std::placeholders::_1));
	//++bind_1_count;

	invoke_test<event_handler_int_t>(user_et, t, 14, 1);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	event_handler_int_t lambda = [&user_evh](int arg) -> void {
		user_evh->foo_int(arg);
	};
	user_et->bind(1, lambda);
	++bind_1_count;

	invoke_test<event_handler_int_t>(user_et, t, 1, 1);
	expected_callcount = (expected_callcount + bind_1_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	//failed with lvalue pass to rvalue reference --DO NOT NEED TO SUPPORT
	//user_et->bind<event_handler_int_t>(1, lambda);

	event_handler_int_t const& lambda_ref = lambda;
	user_et->bind(1, lambda_ref);
	++bind_1_count;

	invoke_test<event_handler_int_t>(user_et, t, 1,123);
	expected_callcount = (expected_callcount + bind_1_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	user_et->bind<event_handler_int_t>(1, [&user_evh](int a) {
		user_evh->foo_int(a);
	});
	++bind_1_count;

	invoke_test<event_handler_int_t>(user_et, t, 1,123);
	expected_callcount = (expected_callcount + bind_1_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	event_handler_int_int_t lambda2 = [&user_evh](int a, int b) -> void {
		user_evh->foo_int_int(a, b);
		(void)b;
	};
	user_et->bind(2, lambda2);
	++bind_2_count;
	invoke_test<event_handler_int_int_t>(user_et, t, 2,2,2);

	expected_callcount = (expected_callcount + bind_2_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));

	invoke_test<event_handler_int_int_t>(user_et, t, 2, 2,2);
	expected_callcount = (expected_callcount + bind_2_count);
	NETP_ASSERT(user_evh->call_count == (expected_callcount));
}


void test_invoker(int t, long loop, std::string const& tag) {
	NETP_INFO("loop: %ld", loop);
	benchmark b(tag);
	while (loop-->0) {
		test_case(t);
	}
}

class v_holder :
	public netp::ref_base
{
	netp::u32_t v;
public:
	v_holder(int v_) :v(v_) {}
	int val() { return v; }
};

template<typename map_t>
void test_map(long size, long loop, std::string const& tag) {
	std::srand(0);
	benchmark b(tag);
	map_t m;
	while (size-- > 0) {
		netp::u32_t v = netp::random_u32();
		m.insert({ v, netp::make_ref<v_holder>(v) });
	}
	b.mark("finish insert");
	while (loop-- > 0) {
		netp::u32_t v = netp::random_u32();
		typename map_t::iterator&& it = m.find(v);
		if (it != m.end()) {
			NETP_ASSERT(it->second->val() == v);
		}
	}
	b.mark("finish find");
	typename map_t::iterator&& it = m.begin();
	while (it != m.end()) {
		netp::u32_t xx = it->second->val() ;
		++xx;
		it++;
	}
}

struct KeyHash {
	inline std::size_t operator()(const netp::u32_t k) const
	{
		return k;
	}
};

//std::size_t vvv = std::hash<int>(123);
//std::size_t hash_u32(const netp::u32_t k) { return k; }
//bool hash_u32_cmp(const netp::u32_t& lhs, const netp::u32_t& rhs) { return lhs == rhs; }

struct KeyEqual {
	inline bool operator()(const netp::u32_t& lhs, const netp::u32_t& rhs) const
	{
		return lhs == rhs;
	}
};


void test_unorderedmap_mem(int max) {

	while (1) {
		typedef std::unordered_map<int, NRP<v_holder>> map_t;
		map_t map;
		
		NETP_INFO("size: %d", map.size());
		NETP_INFO("bucket_count: %d", map.bucket_count());

		const int max_i = max;
		for (int i = 0; i < max_i; ++i) {
			map.insert({i, netp::make_ref<v_holder>(i)});

			if ((i % 100) == 0) {
				NETP_INFO("max_bucket: %d, element count: %d, bucket_count: %d, load factor: %f", map.max_bucket_count(), map.size(), map.bucket_count(), map.load_factor());
			}
		}

		{
			map_t::iterator&& it = map.begin();
			while (it != map.end()) {
				++it;
			}
		}

		for (int i = 0; i < max_i; ++i) {
			map_t::iterator&& it = map.find(i);
			map.erase(it);

			if ((i % 100) == 0) {
				NETP_INFO("max_bucket: %d, element count: %d, bucket_count: %d, load factor: %f", map.max_bucket_count(), map.size(), map.bucket_count(), map.load_factor());
			}
		}

		NETP_INFO("before swap: max_bucket: %d, element count: %d, bucket_count: %d, load factor: %f", map.max_bucket_count(), map.size(), map.bucket_count(), map.load_factor() );
		//map.rehash(1);
		{
			map_t().swap(map);
		}
		NETP_INFO("after swap: max_bucket: %d, element count: %d, bucket_count: %d, load factor: %f", map.max_bucket_count(), map.size(), map.bucket_count(), map.load_factor());

		while (1) {
			//NETP_INFO("map operation done: block here");
			netp::this_thread::sleep(1);
		}
	}
}

int main(int argc, char** argv) {

	NRP<netp::promise<int>> p = netp::make_ref<netp::promise<int>>();
	p->add_listener([]( NRP<netp::future<int>> const& f ) {
		NETP_ASSERT(f->get() == netp::OK);
	});

	p->set_success(0);

	/*
	{
		netp::app _app;

		int max = 10000;
		if (argc == 2) {
			max = netp::to_u32(argv[1]);
		}
		test_unorderedmap_mem(max);
		_app.run();
	}
	*/

	int loop = 100000000;
	if (argc == 2) {
		loop = netp::to_i32(argv[1]);
	}

#ifdef  DEBUG
	loop = 100000;
#endif //  DEBUG

#ifdef TEST_MAP

	for (int i = 50; i < 10000000; ++i) {

		typedef std::map<netp::u32_t, NRP<v_holder>> map_t;
		test_map<map_t>(i, i, "map" + std::to_string(i) );

		typedef std::unordered_map<int, NRP<v_holder>> intumap_t;
		test_map<intumap_t>(i, i, "intunordermap" + std::to_string(i));

		typedef std::unordered_map<netp::u32_t, NRP<v_holder>> umap_t;
		test_map<umap_t>(i, i, "u32unordermap" +std::to_string(i) );

		typedef std::unordered_map<netp::u32_t, NRP<v_holder>, KeyHash, KeyEqual> uhmap_t;
		test_map<uhmap_t>(i, i, "uhunordermap" + std::to_string(i));

		i *= 5;
		NETP_INFO("\n");

	}
#endif

#ifdef __NETP_DEBUG_BROKER_INVOKER_
	test(t_dynamic, loop, "t_dynamic");
	test(t_static, loop, "t_static");
	test(t_virtual_read_callee_address_invoke, loop, "t_callee_address");
#endif

	test_invoker(t_invoke, loop, "t_invoke");

#if NETP_ISWIN
	system("pause");
#endif
	return 0;
}