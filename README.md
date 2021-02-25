**A new beginning of network programming**

# What is Netplus

Netplus is a network programming library written by c++11. It's highly extensible, and with default configuration it can reach high performance.
It's easy to learn. Basically, you can just implement your own business in a channel handler, then the library would do all the dirty jobs (platform specific considerations, io event delivery&process, basic memory optimization, etc ) for you.
You can also write http/https,websocket,based application even you only have little corresponding knowledge background, Cuz it has relevant modules to simplify programming tasks of such kinds.
Netplus borrows lots of concepts from Netty, and tries to implement these concepts by c++, such as channel, channel handler, channel pipeline, event loop, executor, promise. Any one who is familiar with Netty should be able to start with ease. 

# What can we use Netplus to do
    1) Implement a message gateway
    2) As the network component of a message queue, gameserver or any other network based application.
       please note that netplus is not a gameserver framework, But you can implement one by yourself
    3) Write protocol driver of Middleware, such as kinds of database, message queue server, etc
    4) Write http/https,websocket, rpc based application with well defined modules
    

# Target Arch & Platform
    1) Windows on x86
    2) Linux on x86/arm
    3) IOS/MAC on x86/arm
    4) Android on x86/arm

# Learn more from the following urls:

Build and usage: <https://github.com/netplus/netplus/wiki#building--usage>

Examples: <https://github.com/netplus/netplus/wiki#examples>

Basic concept: <https://github.com/netplus/netplus/wiki#concept>

Modules: <https://github.com/netplus/netplus/wiki#examples>

Zhihu: https://www.zhihu.com/column/c_1339539434091040768

	
# Others

	QQ group: 576499184 (加群请注明来源，thx)
	

