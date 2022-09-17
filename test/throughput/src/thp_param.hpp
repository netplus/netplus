#ifndef _THP_PARAM_HPP
#define _THP_PARAM_HPP
#include <netp.hpp>

enum mode {
	m_rps = 0,
	m_tps,
	m_notset
};

class thp_param
{
public:
	long client_max;

	long packet_number;
	long packet_size;
	long rcvwnd;
	long sndwnd;
	long loopbufsize;
	long thread;
	long for_max;
	long mode;
	long ackdelta;

	thp_param() :
		client_max(1),
		packet_number(10000),
		packet_size(64),
		rcvwnd(128*1024),
		sndwnd(64*1024),
		loopbufsize(128 * 1024),
		thread(0),
		for_max(1),
		mode(m_rps),
		ackdelta(10000)
	{}
};

 void parse_param(thp_param& p, int argc, char** argv) {
	static struct option long_options[] = {
		{"len", optional_argument, 0, 'l'}, //packet len
		{"number",optional_argument, 0, 'n'}, //packet number
		{"rcvwnd",optional_argument,0,'r'},
		{"sndwnd", optional_argument,0, 's'},
		{"clients", optional_argument, 0, 'c'},
		{"buf-for-evtloop", optional_argument, 0, 'b'},
		{"thread-for-evtloop", optional_argument, 0, 't'},
		{"for", optional_argument, 0, 'f'},
		{"mode", optional_argument, 0, 'm'},
		{"ack-delta", optional_argument, 0, 'a'},
		{"help", optional_argument, 0, 'h'},
		{0,0,0,0}
	};

	const char* optstring = "l:n:c:r:s:b:t:f:m:a:h::";

	int opt;
	int opt_idx;
	while ((opt = getopt_long(argc, argv, optstring, long_options, &opt_idx)) != -1) {
		switch (opt) {
		case 'l':
		{
			p.packet_size = std::atol(optarg);
		}
		break;
		case 'n':
		{
			p.packet_number = std::atol(optarg);
		}
		break;
		case 'c':
		{
			p.client_max = std::atol(optarg);
		}
		break;
		case 'r':
		{
			p.rcvwnd = std::atol(optarg);
		}
		break;
		case 's':
		{
			p.sndwnd = std::atol(optarg);
		}
		break;
		case 'b':
		{
			p.loopbufsize = std::atol(optarg);
		}
		break;
		case 't':
		{
			p.thread = std::atol(optarg);
		}
		break;
		case 'f':
		{
			p.for_max = std::atol(optarg);
		}
		break;
		case 'm':
		{
			p.mode = std::atol(optarg);
		}
		break;
		case 'a':
		{
			p.ackdelta = std::atol(optarg);
		}
		break;
		case 'h':
		{
			printf("usage:  -c max_clients -l bytes_len -n packet_number\nexample: thp.exe -c 1 -l 64 -n 1000000 -m 0\n");
			exit(-1);
			break;
		}
		default:
		{
			NETP_ASSERT(!"AAA");
		}
		}
	}
}

#endif