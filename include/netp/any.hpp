#ifndef _NETP_ANY_HPP
#define _NETP_ANY_HPP

#include <typeinfo>
#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/exception.hpp>

namespace netp {
	class bad_cast: public netp::exception {
		public:
			bad_cast():
				exception(netp::E_BAD_CAST, __FILE__, __LINE__, __FUNCTION__, "bad cast" )
			{}
	};

	//no reference here
	template <typename T>
	struct any_broker : public ref_base {
		typename std::decay<T>::type value;
		template <typename U>
		any_broker(U&& u) :
			value(std::forward<U>(u))
		{}
	};

	class any {
		std::type_info* m_type;
		NRP<netp::ref_base> m_broker;

	public:
		any():
			m_type((std::type_info*)&typeid(void)),
			m_broker(nullptr)
		{}

		any(any const& other):
			m_type(other.m_type),
			m_broker(other.m_broker)
		{}

		any(any&& other):
			m_type(std::move(other.m_type)),
			m_broker(std::move(other.m_broker))
		{}

		inline any& operator =(any const& other) {
			any(other).swap(*this);
			return *this;
		}

		inline any& operator=(any&& other) {
			any(std::forward<any>(other)).swap(*this);
			return *this;
		}
		
		template <class T, class = typename std::enable_if<!std::is_same<typename std::decay<T>::type, any>::value,T>::type>
		inline any(T&& t):
			m_type((std::type_info*)&typeid(typename std::decay<T>::type)),
			m_broker(netp::make_ref<any_broker<typename std::decay<T>::type>>(std::forward<T>(t)))
		{}

		inline const std::type_info& type() const { return *m_type;}

		template <class T>
		inline T any_cast() {
			if(m_type->hash_code() == (typeid(void)).hash_code())  {
				return T();
			}
			else if( m_type->hash_code() != ((typeid(T)).hash_code() ) ) {
				throw netp::bad_cast();
			}
			else {
				NRP<any_broker<T>> broker = netp::dynamic_pointer_cast<any_broker<T>>(m_broker);
				NETP_ASSERT(broker != nullptr);
				return broker->value;
			}
		}

		inline void reset() _NETP_NOEXCEPT { m_broker =nullptr;}

		void swap(any& other) _NETP_NOEXCEPT {
			std::swap(m_type,other.m_type);
			std::swap(m_broker,other.m_broker);
		}

		inline bool has_value() _NETP_NOEXCEPT {	return (m_type->hash_code() != (typeid(void)).hash_code()) ;}
	
		inline bool operator ==(netp::any const& r) const { 
			return r.m_type->hash_code() == m_type->hash_code() &&
				r.m_broker == m_broker; 
		}

		inline bool operator !=(netp::any const& r) const { 
			return !(*this == r); 
		}

		//inline bool operator < (netp::any const& r) const { 
		//	return *(m_broker.get()) < *(r.m_broker.get());
		//}
		//inline bool operator > (netp::any const& r) const { 
		//	return *m_broker > *r.m_broker;
		//}	
	};

	//inline bool operator < (const netp::any& left, const netp::any const& right) {
	//	return left < right;
	//}

	template <typename T, class... Args> 
	inline netp::any make_any(Args&&... args) {
		return netp::any(std::forward<T>(T(std::forward<Args>(args)...)));
	}

}

#define NETP_TEST_ANY
#ifdef NETP_TEST_ANY
#include <netp/app.hpp>
#include <utility>

namespace netp { namespace test {
	struct foo {
		int i;
		float f;

		std::string info() {
			return "i: " + std::to_string(i) + ", f:" + std::to_string(f);
		}

		void incre() {
			++i;
			f += 0.1f;
		}
	};

	struct reffoo : public netp::ref_base
	{
		int i;

		void incre() { ++i; }
	};

	inline void void_any_fun() {
		NETP_INFO("void_any_fun_void call");
	}

	inline void void_any_fun_int(int i) {
		NETP_INFO("void_any_fun_int_call: %d", i);
	}

	inline int int_any_fun_int(int i) {
		NETP_INFO("int_any_fun_int_call: %d", i);
		return i++;
	}

	inline void any_basic_operation() {

		netp::any any_var_;
		netp::any any_var = 1;
		NETP_INFO("init as int: %s, value: %d", any_var.type().name(), any_var.any_cast<int>());
		NETP_ASSERT(any_var.any_cast<int>()==1);

		any_var = 1111.01f;
		NETP_INFO("assign as float, type: %s, value: %f", any_var.type().name(), any_var.any_cast<float>());
		float float_from_any_var_cast = any_var.any_cast<float>();
		NETP_ASSERT(NETP_ABS2(float_from_any_var_cast,1111.01f)<0.01f);

		foo foo_ = foo{100,1.0f};
		any_var = foo_; //foo is a user defined struct
		NETP_INFO("assign a foo, type: %s, value: %s", any_var.type().name(), any_var.any_cast<foo>().info().c_str());
		NETP_ASSERT(any_var.any_cast<foo>().i == 100);
		any_var.any_cast<foo>().incre();
		NETP_ASSERT(any_var.any_cast<foo>().i == 100);
		NETP_ASSERT(foo_.i == 100);


		NSP<foo> shared_foo_ = netp::make_shared<foo>();
		shared_foo_->i = 1;
		shared_foo_->f = 1.0f;

		any_var = shared_foo_;
		NETP_INFO("assign a shared_ptr<foo>, type: %s, value: %s", any_var.type().name(), any_var.any_cast<NSP<foo>>()->info().c_str());
		any_var.any_cast<NSP<foo>>()->incre();
		NETP_INFO("after any_var.any_cast<shared_foo>()->incre(), type: %s, value: %s", any_var.type().name(), any_var.any_cast<NSP<foo>>()->info().c_str());
		NETP_INFO("after any_var.any_cast<shared_foo>()->incre(), value : %s", shared_foo_->info().c_str());

		NETP_ASSERT(shared_foo_->i == any_var.any_cast<NSP<foo>>()->i );

		NRP<reffoo> reffoo_instance = netp::make_ref<reffoo>();
		reffoo_instance->i = 100;

		any_var = reffoo_instance;
		NETP_INFO("ref, value: %d", reffoo_instance->i);

		any_var.any_cast<NRP<reffoo>>()->incre();
		NETP_INFO("ref after incre, value: %d", reffoo_instance->i);
		NETP_ASSERT(reffoo_instance->i == 101);


		nlohmann::json _json = nlohmann::json(nlohmann::json::object());
		_json["key"] = "json_v";
		any_var = _json;
		NETP_INFO("assign a foo instance ref, type: %s", any_var.type().name());
		NETP_INFO("assign a foo instance ref, type: %s, value: %s", any_var.type().name(), any_var.any_cast<nlohmann::json>().dump().c_str());

		typedef std::function<void()> void_fn_t;
		void_fn_t fn = std::bind(&void_any_fun);
		any_var = fn;
		any_var.any_cast<void_fn_t>().operator()();

		typedef std::function<void(int)> void_any_fun_int_t;
		void_any_fun_int_t aa = std::bind(&void_any_fun_int, std::placeholders::_1);

		//typename std::decay<decltype(std::bind(&void_any_fun_int, std::placeholders::_1))>::type aa1;
		any_var = netp::make_any<void_any_fun_int_t>(std::bind(&void_any_fun_int, std::placeholders::_1));
		//any_xx.any_func<void_any_fun_int_t>(std::bind(&void_any_fun_int, std::placeholders::_1));

		any_var.any_cast<void_any_fun_int_t>().operator()(22);

		typedef std::function<int(int)> int_any_fun_int_t;

		any_var = netp::make_any< int_any_fun_int_t>(std::bind(&int_any_fun_int, std::placeholders::_1));
		any_var = any_var;

		int jjj = any_var.any_cast<int_any_fun_int_t>().operator()(11);
		NETP_ASSERT(jjj == 11);

		int iii = any_var.any_cast<int_any_fun_int_t>().operator()(11);
		NETP_ASSERT(iii == 11);
	}

	//we can not use netp::any for now
	typedef std::map<std::string, netp::any> dict_t;
	typedef std::tuple<netp::any, netp::any> tuple_t;

	inline void any_container_operation() {
		dict_t dict;

		dict["int_v"] = 1;
		dict["b"] = "string v";
		dict["foo"] = foo{10,10.0f};

		tuple_t tuple_;
	}

}}
#endif //DEBUG

#endif