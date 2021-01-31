
#include <netp.hpp>
#include "ServiceShare.h"

class hello_handler :
	public netp::channel_handler_abstract
{
private:
	std::string m_addr;
	int m_max_concurrency;
	int m_curr_peer_count;

public:
	hello_handler(std::string const& url, int max_concurrency = 1) :
		channel_handler_abstract(netp::CH_INBOUND_READ | netp::CH_ACTIVITY_CLOSED | netp::CH_ACTIVITY_CONNECTED),
		m_addr(url),
		m_max_concurrency(max_concurrency),
		m_curr_peer_count(0)
	{
	}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) override {
		NETP_INFO(">>> %u", income->len());

		NRP<netp::packet> outp = netp::make_ref<netp::packet>();
		outp->write(income->head(), income->len());
		ctx->write(outp);
	}

	void closed(NRP<netp::channel_handler_context> const& ctx) override {
		NETP_INFO("socket closed");
	}

	/*
	void async_spawn() {
		NRP<netp::socket> so = netp::make_ref<netp::socket>(m_addrinfo.so_family, m_addrinfo.so_type, m_addrinfo.so_protocol);

		NRP<netp::channel_future> f = so->dial(m_addrinfo.so_address, [&](NRP<netp::channel> const& ch) {
			ch->pipeline()->add_last(NRP<netp::channel_handler_abstract>(this));
		});

		NETP_ASSERT(f->get() == netp::OK );
	}
	*/
	void connected(NRP<netp::channel_handler_context> const& ctx) override {
		services::HelloProcessor::SendHello(ctx);

		++m_curr_peer_count;
		if (m_curr_peer_count < m_max_concurrency) {
			//async_spawn();
		}
	}
};

int main(int argc, char** argv) {
	netp::app app ;
	std::string url = "tcp://127.0.0.1:22310";
	NRP<netp::channel_dial_promise> f = netp::socket::dial(url, [url](NRP<netp::channel> const& ch) {
		ch->pipeline()->add_last(netp::make_ref<hello_handler>(url, 2000));
	});

	//NETP_ASSERT(f->get() == netp::OK);
	f->if_done([url](std::tuple<int, NRP<netp::channel>> const& tupc) {
		int rt = std::get<0>(tupc);
		NETP_INFO("connect rt: %d, address: %s", rt, url.c_str());
		if ( std::get<0>(tupc) != netp::OK) {
		}
	});

	app.run();
	int rt = std::get<0>(f->get());
	if (rt != netp::OK) {
		return rt ;
	}
	std::get<1>(f->get())->ch_close();
	std::get<1>(f->get())->ch_close_promise()->wait();

	NETP_WARN("[main]socket server exit done ...");
	return netp::OK;
}