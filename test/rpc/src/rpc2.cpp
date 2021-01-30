#include <netp.hpp>

#include "shared.hpp"

void on_rpc_call(NRP<netp::rpc> const& RPC, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) {
	(void)RPC;

	nlohmann::json json_in = nlohmann::json::parse(nlohmann::detail::input_adapter(in->begin(), in->len()));
	nlohmann::json json_out;

	json_out["message"] = std::string("message from a aditional function call");
	json_out["req"] = json_in.at("req").get<std::string>();

	const std::string& json_string = json_out.dump();
	NRP<netp::packet> outp = netp::make_ref<netp::packet>();
	outp->write((netp::byte_t*)json_string.c_str(), json_string.length() );

	f->set(std::make_tuple(netp::OK,outp));
}

class foo :
	public netp::ref_base
{
public:
	void bar(NRP<netp::rpc> const& RPC, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) {
		(void)RPC;
		nlohmann::json json_in = nlohmann::json::parse(nlohmann::detail::input_adapter(in->begin(), in->len()));
		nlohmann::json json_out;

		json_out["message"] = std::string("message from a class instance's member function call");
		json_out["req"] = json_in.at("req").get<std::string>();

		const std::string& json_string = json_out.dump();
		NRP<netp::packet> outp = netp::make_ref<netp::packet>();
		outp->write((netp::byte_t*)json_string.c_str(), json_string.length());

		f->set(std::make_tuple(netp::OK,outp));
	}

	void ping(NRP<netp::rpc> const& r, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) {
		NRP<netp::packet> pong = netp::make_ref<netp::packet>(in->begin(), in->len());
		f->set(std::make_tuple(netp::OK, pong));
	}
};

int main(int argc, char** argv) {
	netp::app _app;

	netp::fn_rpc_activity_notify_t fn_bind_api = [](NRP<netp::rpc> const& r) {

		/*
		(void)r;
		r->set_queue_size(1000);

		std::string hello_s = "hello";
		NRP<netp::packet> outp = netp::make_ref<netp::packet>();
		outp->write((netp::byte_t*)hello_s.c_str(), hello_s.length());

		NRP<netp::rpc_push_promise> rf = r->push(outp);
		rf->if_done([]( int const& rt ) {
			NETP_INFO("[rpc]rpc write data on connected, write rt: %d", rt);
		});


		r->bindcall(rpc_call_test_api::API_LAMBDA, [](NRP<netp::rpc> const& r, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) -> void {
			nlohmann::json json_in = nlohmann::json::parse(nlohmann::detail::input_adapter(in->begin(), in->len()));
			nlohmann::json json_out;

			json_out["message"] = std::string("message from remote lambda");
			json_out["req"] = json_in.at("req").get<std::string>();

			const std::string& json_string = json_out.dump();
			
			NRP<netp::packet> outp = netp::make_ref<netp::packet>();
			outp->write((netp::byte_t*)json_string.c_str(), json_string.length());
			
			f->set(std::make_tuple(netp::OK,outp));
		});

		r->bindcall(rpc_call_test_api::API_TRADITIONAL_FUNC, &on_rpc_call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

		r->bindcall(rpc_call_test_api::API_CLASS_INSTANCE_CALL, &foo::bar, foo_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		*/
		NRP<foo> foo_ = netp::make_ref<foo>();
		r->bindcall(rpc_call_test_api::API_PING, &foo::ping, foo_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	};

	NRP<netp::promise<int>> lf = netp::rpc::listen("tcp://0.0.0.0:21001", fn_bind_api);

	int rt = lf->get();
	if (rt != netp::OK) {
		NETP_INFO("[jrpc2]listen rpc service failed: %d", rt);
	}

	_app.run();
	return 0;
}