
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

	for (int i = 0; i < g_param.for_max; ++i) {
		netp::app::instance()->init(argc,argv);
		netp::app::instance()->cfg_channel_read_buf(g_param.loopbufsize);
		if (g_param.thread != 0) {
			netp::app::instance()->cfg_loop_count(g_param.thread);
		}
		netp::app::instance()->start_loop();
/*
		NRP<netp::packet> outp1 = netp::make_ref<netp::packet>();
		NRP<netp::packet> outp2 = outp1;		

		NRP<netp::packet> outp3 = outp1;
		NRP<netp::packet> outp4 = outp1;

		std::vector<NRP<netp::packet>> pvec;
		pvec.emplace_back(outp1);
		pvec.emplace_back(outp2);
		pvec.emplace_back(outp3);
		pvec.emplace_back(outp4);

		outp2 = nullptr;
		outp3 = nullptr;
		outp4 = nullptr;

		_atomic_long.load(std::memory_order_relaxed);
		_atomic_long.load(std::memory_order_acquire);

		_atomic_long.store(7, std::memory_order_relaxed);
		_atomic_long.store(7, std::memory_order_release);
		_atomic_long.store(7, std::memory_order_seq_cst);

		std::atomic_thread_fence(std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_acq_rel);
		std::atomic_thread_fence(std::memory_order_seq_cst);
		std::atomic_thread_fence(std::memory_order_acquire);

		std::atomic_signal_fence(std::memory_order_relaxed);
		std::atomic_signal_fence(std::memory_order_acq_rel);
		std::atomic_signal_fence(std::memory_order_seq_cst);
		std::atomic_signal_fence(std::memory_order_acquire);

		_atomic_long.load(std::memory_order_relaxed);
		_atomic_long.load(std::memory_order_acquire);

		std::atomic_thread_fence(std::memory_order_seq_cst);
		_atomic_long.store(7, std::memory_order_relaxed);

		_atomic_long.store(7, std::memory_order_release);

		_atomic_long.fetch_add(1, std::memory_order_relaxed);
		_atomic_long.fetch_add(1, std::memory_order_acq_rel);


		printoutp(pvec);
*/
		{
			netp::benchmark bmarker("start");
			handler_start_listener(g_param);
			bmarker.mark("listen done");

			handler_dial_clients(g_param);
			bmarker.mark("dial all done");

			netp::app::instance()->wait();

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
		}

		netp::app::instance()->destroy_instance();
	}
	return 0;
}