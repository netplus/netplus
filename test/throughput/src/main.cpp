
// This is a simple throughput evulate example
// The main goal is to get a quick impression of Netplus's performance

// the evulate method is as bellow:
// step 1, listen on tcp://0.0.0.0:32002
// step 2, dial to tcp://0.0.0.0:32002, once it is connected, the client writes a packet, set WRITTEN_PACKET to 1
// step 3, the remote endpoint will reply this packet to client
// step 4, when the client has received the full reply, it compares the WRITTEN_PACKET with TOTAL_PACKET, and writes another packet and incre(WRITTEN_PACKET) if WRITTEN_PACKET<TOTAL_PACKET 
// step 5, record the total cost time as TOTAL TITME 
// step 6, calc avgrate as (TOTAL PACKET)/TOTAL TIME
// step 7, calc avgbits as (TOTAL PACKET * TOTAL NUMBER) / TOTAL TIME

//example: 
//thp.exe -h
//thp.exe -l 128 -n 1000000

#include <netp.hpp>

#include "thp_param.hpp"
#include "thp_handler.hpp"
#include "thp_socket.hpp"

thp_param g_param;

int main(int argc, char** argv) {

	parse_param(g_param, argc, argv);

	//for (int i = 0; i < 100; ++i) {
		netp::app_cfg appcfg;
		appcfg.poller_cfgs[netp::u8_t(NETP_DEFAULT_POLLER_TYPE)].ch_buf_size = g_param.loopbufsize;
		appcfg.cfg_poller_count(NETP_DEFAULT_POLLER_TYPE, 1);
		appcfg.cfg_channel_rcv_buf(NETP_DEFAULT_POLLER_TYPE, 64);

		netp::app _app(appcfg);
		netp::benchmark bmarker("start");
		handler_start_listener(g_param);
		bmarker.mark("listen done");

		handler_dial_clients(g_param);
		bmarker.mark("dial all done");

		_app.run();

		handler_stop_listener();
		bmarker.mark("wait for listener");

		std::chrono::steady_clock::duration cost = bmarker.mark("test done");
		std::chrono::seconds sec = std::chrono::duration_cast<std::chrono::seconds>(cost);
		if (sec.count() == 0) {
			sec = std::chrono::seconds(1);
		}

		float avgrate = netp::u64_t(g_param.packet_number) * 1.0 / (sec.count());
		float avgbits = netp::u64_t(g_param.packet_number) * netp::u64_t(g_param.packet_size) * 1.0 / (sec.count() * 1000 * 1000);
		NETP_INFO("\n---\npacket size: %ld bytes\nnumber: %ld\ncost: %ld s\navgrate: %0.2f/s\navgbits: %0.2fMB/s\n---",
			g_param.packet_size,
			g_param.packet_number,
			sec.count(),
			g_param.client_max * avgrate, g_param.client_max * avgbits);
		NETP_INFO("main exit");
	//}
	return 0;
}