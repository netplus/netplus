
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


void printoutp(std::vector<NRP<netp::packet>>& outp) {
	(void)outp;
	for (auto p : outp)
	{
		NETP_INFO("%s", std::string((char*)p->head(), p->len()));
	}
}

std::atomic<long> _atomic_long;

int main(int argc, char** argv) {
	parse_param(g_param, argc, argv);
	std::atomic_thread_fence(std::memory_order_release);

	for (int i = 0; i < g_param.for_max; ++i) {
		netp::app::instance()->init(argc,argv);
		netp::app::instance()->cfg_channel_read_buf(g_param.loopbufsize);
		if (g_param.thread != 0) {
			netp::app::instance()->cfg_loop_count(g_param.thread);
		}
		netp::app::instance()->start_loop();

		{
			netp::benchmark bmarker("start");
			handler_start_listener();
			bmarker.mark("listen done");

			handler_dial_clients();
			bmarker.mark("dial all done");

			netp::app::instance()->wait();

			handler_stop_listener();
			bmarker.mark("wait for listener");

			std::chrono::steady_clock::duration cost = bmarker.mark("test done");
			std::chrono::milliseconds mills = std::chrono::duration_cast<std::chrono::milliseconds>(cost);
			if (mills.count() == 0) {
				mills = std::chrono::milliseconds(1);
			}

			double avgrate = netp::u64_t(g_param.packet_number) * 1.0f * 1000 / (mills.count());
			double avgbits = netp::u64_t(g_param.packet_number) * netp::u64_t(g_param.packet_size) * 1.0f * 1000 / (mills.count()*1000*1000);
			NETP_INFO("\n---\npacket size: %ld bytes\nnumber: %ld\ncost: %lld ms\navgrate: %0.2f/s\navgbits: %0.2fMB/s\n---",
				g_param.packet_size,
				g_param.packet_number,
				mills.count(),
				g_param.client_max * avgrate, g_param.client_max * avgbits);
			NETP_INFO("main exit");
		}

		netp::app::instance()->destroy_instance();
	}
	return 0;
}