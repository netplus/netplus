#include <netp.hpp>

class Pong :
	public netp::channel_handler_abstract {
public:
	Pong() : 
		channel_handler_abstract(netp::CH_INBOUND_READ)
	{}
	//for inbound
	void read(netp::ref_ptr<netp::channel_handler_context> const& ctx, netp::ref_ptr<netp::packet> const& income) {
		//reply with PONG
		const std::string pong = "PONG";
		netp::ref_ptr<netp::packet> PONG = netp::make_ref<netp::packet>(pong.c_str(), pong.length());
		netp::ref_ptr<netp::promise<int>> write_promise = ctx->write(PONG);

		//check the reply status once the write operation is done
		write_promise->if_done([](int reply_rt) {
			NETP_INFO("[PONG]reply PONG, rt: %d", reply_rt );
		});
	}
};

class Ping : 
	public netp::channel_handler_abstract {
public:
	Ping():
		channel_handler_abstract(netp::CH_ACTIVITY_CONNECTED|netp::CH_INBOUND_READ)
	{}
	void connected(netp::ref_ptr<netp::channel_handler_context> const& ctx) {
		NETP_INFO("[PING]connected");
		//initial PING
		do_ping(ctx);
	}
	void read(netp::ref_ptr<netp::channel_handler_context> const& ctx, netp::ref_ptr<netp::packet> const& income) {
		NETP_INFO("[PING]reply income");
		do_ping(ctx);
	}
	void do_ping(netp::ref_ptr<netp::channel_handler_context> const& ctx) {
		const std::string ping = "PING";
		netp::ref_ptr<netp::packet> message_ping = netp::make_ref<netp::packet>();
		message_ping->write(ping.c_str(), ping.length());
		netp::ref_ptr<netp::promise<int>> write_p = ctx->write(message_ping);
		write_p->if_done([]( int rt ) {
			NETP_INFO("[PING]write PING, rt: %d", rt );
		});
	}
};

int main(int argc, char** argv) {

	int li = sizeof(long);

	//initialize a netplus app instance
	netp::app app;
	std::string host = "tcp://127.0.0.1:13103";

	netp::ref_ptr<netp::channel_listen_promise> listenp = netp::socket::listen_on(host, [](netp::ref_ptr<netp::channel>const& ch) {
		ch->pipeline()->add_last( netp::make_ref<netp::handler::hlen>());
		ch->pipeline()->add_last( netp::make_ref<Pong>() );
	});
	int listenrt = std::get<0>(listenp->get());
	if (listenrt != netp::OK) {
		NETP_INFO("listen on host: %s failed, fail code: %d", host.c_str(), listenrt);
		return listenrt;
	}
//	app.run();

	netp::ref_ptr<netp::channel_dial_promise> dialp = netp::socket::dial(host, [](netp::ref_ptr<netp::channel> const& ch ) {
		ch->pipeline()->add_last( netp::make_ref<netp::handler::hlen>() );
		ch->pipeline()->add_last( netp::make_ref<Ping>() );
	});
	int dialrt = std::get<0>(dialp->get());
	if (dialrt != netp::OK) {
		//close listen channel and return
		std::get<1>(listenp->get())->ch_close();
		return dialrt;
	}

	//wait for signal to exit
	//Ctrl+C on windows
	//kill -15 on linux
	app.run();

	//close listen channel
	std::get<1>(listenp->get())->ch_close();

	//close dial channel
	std::get<1>(dialp->get())->ch_close();
	return 0;
}
