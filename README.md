**A new beginning of network programming**

# What is Netplus

Netplus is a network programming library written in c++11. It's highly extensible, and with default configuration it can reach high performance.
It's easy to learn. Basically, you can just implement your own business in a channel handler, then the library would do all the dirty work (platform specific considerations, io event delivery&process, basic memory optimization, etc ) for you.
You can also write http/https,websocket,based application even you only have little corresponding knowledge background, Cuz it has relevant modules to simplify programming tasks of such kinds.
Netplus borrows lots of concepts from Netty, and tries to implement these concepts by c++, such as channel, channel handler, channel pipeline, executor, scheduler, promise. Any one who is familiar with Netty should be able to start with ease. 

# What can we use Netplus to do
    1) Implement a message gateway
    2) Use as the network component of a message queue, gameserver or any other network based application.
       please note that netplus is not a gameserver framework, But you can implement one with netplus
    3) Implement protocol driver of Middleware, such as kinds of database, message queue server, etc
    4) Implement http/https, websocket based application
    5) Implement RPC server

# Target Arch & Platform
    1) Windows on x86/x64
    2) Linux on x86/x64/arm
    3) IOS/MAC on x86/arm
    4) Android on x86/arm
    
    
# First impression

```CPP

//listen on 0.0.0.0:80 port for a http service, 
netp::listen_on("tcp://0.0.0.0:80", [](NRP<netp::channel> const& ch) {
    ch->pipeline()->add_last(netp::make_ref<netp::handler::http>());
    ...
}

//fast request for a http request
NRP<netp::http::request_promise> rp = netp::http::get("https://x.com/");

//listen on 10088 for a rpc service
netp::rpc::listen("tcp://0.0.0.0:10088",[](NRP<netp::rpc> const& r){
   r->bindcall( api_id, []( NRP<netp::rpc> const& r, NRP<netp::packet> cosnt& in, NRP<netp::rpc_call_promise> const& rcp ) {
       NETP_INFO("[rpc]a request in, request pkt len: %u", in->len() );
       NRP<netp::packet> reply = netp::make_ref<netp::packet>();
       reply->write("this is a reply from the call to api_id");
       
       //call return with netp::OK, and a message in reply pkt
       rcp->set(std::make_tuple(netp::OK, reply));
   });
},...);

```

# Learn more from the following urls:

Build and usage: <https://github.com/netplus/netplus/wiki#building--usage>

Quick Start: <https://github.com/netplus/netplus/wiki#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B>

Benchmark: <https://github.com/netplus/netplus/wiki#benchmark>

Examples: <https://github.com/netplus/netplus/wiki#examples>

Basic concept: <https://github.com/netplus/netplus/wiki#concept>

Modules: <https://github.com/netplus/netplus/wiki#modules>

Zhihu: https://www.zhihu.com/column/c_1339539434091040768

	
# Others

	QQ group: 576499184 (加群请注明来源，thx)

