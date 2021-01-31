#include <netp.hpp>

int main(int argc, char** argv) {

	netp::app _app;
	std::string log_server("tcp://127.0.0.1:30033");

	netp::rpc::listen(log_server, [](NRP<netp::rpc> const& rpc_) {
		rpc_->on_push([](NRP<netp::rpc> const& rpc_, NRP<netp::packet> const& in) {
			printf("[netlog]<<\n%s-----\n", std::string((char*)in->head(), in->len()).c_str() );
		});
	});

	NRP<netp::logger::net_logger> nlogger = netp::make_ref<netp::logger::net_logger>(log_server);
	netp::logger_broker::instance()->add(nlogger);
	NRP<netp::promise<int>> drp = nlogger->dial();
	if (drp->get() != netp::OK) {
		netp::logger_broker::instance()->remove(nlogger);
		NETP_ERR("dail log server failed");
		return -1;
	}

	for (int i = 0; i < 5; ++i) {
		NETP_INFO("test net logger");
	}
	netp::logger_broker::instance()->remove(nlogger);

	_app.run();
	return 0;
}