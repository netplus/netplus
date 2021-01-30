
#include <netp.hpp>

int main(int argc, char** argv) {

	netp::app _app;
	std::string logserver = "tcp://dev.fine666.com:10122";

	NRP<netp::log::net_logger> nlogger = netp::make_ref<netp::log::net_logger>(logserver);
	netp::log::logger_manager::instance()->add_logger(nlogger);
	NETP_INFO("[main]add logger done");

	nlogger->dial();

	
	std::string computer_name;
	netp::os::get_local_computer_name(computer_name);

	NETP_INFO("[main]local computer name: %s", computer_name.c_str());

	std::vector<netp::adapter> adapters;
	netp::os::get_adapters(adapters, netp::os::adp_f_skip_loopback|netp::os::adp_f_skip_noup);

	std::string adapters_info;
	for (std::size_t i = 0; i < adapters.size(); ++i) {
		adapters_info += adapter_to_string( adapters[i] ) + "\n"; 
	}
	NETP_INFO("[main]adapters: %s", adapters_info.c_str() );

	NRP<netp::upnp::control_point> cp = netp::make_ref<netp::upnp::control_point>(adapters);
	NRP<netp::promise<int>> cf = cp->start();
	cf->wait();

	/*
	std::vector<int> ports;
	ports.push_back(43311);
	ports.push_back(44276);
	ports.push_back(42093);
	ports.push_back(46810);
	ports.push_back(46084);
	ports.push_back(44995);
	ports.push_back(43455);
	ports.push_back(43974);
	ports.push_back(40621);
	ports.push_back(42266);
	ports.push_back(45505);
	ports.push_back(45726);
	ports.push_back(48867);

	for (auto port : ports) {
		netp::upnp::port_map_hint del_port_map_h;
		del_port_map_h.proto = "TCP";
		del_port_map_h.external_port = port;

		NRP<netp::upnp::port_map_promise> del_mf = cp->del_port_map(del_port_map_h);
		std::tuple<int, std::vector<netp::upnp::port_map_record>> del_maprt = del_mf->get();
		NETP_INFO("[main]map done: %d, port: %u", std::get<0>(del_maprt), port );
	}
	*/

	netp::upnp::port_map_hint port_map_h;
	port_map_h.proto = "TCP";
	port_map_h.internal_ip = 0;
	port_map_h.internal_port = 32122;
	//	port_map_h.external_port = 32122;

	NRP<netp::upnp::port_map_promise> mf = cp->add_port_map(port_map_h);
	std::tuple<int, std::vector<netp::upnp::port_map_record>> maprt = mf->get();
	NETP_INFO("[main]map done: %d", std::get<0>(maprt));

	if (std::get<0>(maprt) == netp::OK) {
		for (netp::upnp::port_map_record& r : std::get<1>(maprt)) {
			NETP_INFO("[main]upnp result: %s:%d--%s:%d", netp::ipv4todotip(r.internal_ip).c_str(), r.internal_port, netp::ipv4todotip(r.external_ip).c_str(), r.external_port);

			netp::upnp::port_map_hint del_port_map_h;
			del_port_map_h.proto = "TCP";
			del_port_map_h.external_port = r.external_port;

			NRP<netp::upnp::port_map_promise> del_mf = cp->del_port_map(del_port_map_h);
			std::tuple<int, std::vector<netp::upnp::port_map_record>> del_maprt = del_mf->get();
			NETP_INFO("[main]map done: %d, port: %u", std::get<0>(del_maprt), r.external_port);
		}
	}

	_app.run();
	netp::log::logger_manager::instance()->remove_logger(nlogger);
	

	cp->stop();
	return 0;
}