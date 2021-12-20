#ifndef _NETP_HPP_
#define _NETP_HPP_

#include "../3rd/nlohmann/include/nlohmann/json.hpp"

#include <netp/core.hpp>
#include <netp/CPUID.hpp>
#include <netp/funcs.hpp>

#include <netp/smart_ptr.hpp>
#include <netp/singleton.hpp>
#include <netp/any.hpp>

#include <netp/string.hpp>

#include <netp/os/api_wrapper.hpp>

#include <netp/bytes_helper.hpp>
#include <netp/ringbuffer.hpp>
#include <netp/packet.hpp>
#include <netp/bytes_ringbuffer.hpp>
#include <netp/heap.hpp>

#include <netp/helper.hpp>

#include <netp/mutex.hpp>
#include <netp/condition.hpp>
#include <netp/thread.hpp>
#include <netp/promise.hpp>

#include <netp/event_broker.hpp>
#include <netp/timer.hpp>

#ifdef __ANDROID__
	#include <netp/log/android_log_print.hpp>
#endif

#include <netp/logger/console_logger.hpp>
#include <netp/logger/file_logger.hpp>
#include <netp/logger/sys_logger.hpp>
#include <netp/logger/net_logger.hpp>

#include <netp/scheduler.hpp>

#include <netp/security/dh.hpp>
#include <netp/security/xxtea.hpp>

#include <netp/address.hpp>
#include <netp/socket.hpp>
#include <netp/icmp.hpp>

#include <netp/util_hlen.hpp>

#include <netp/handler/hlen.hpp>
#include <netp/handler/fragment.hpp>
#include <netp/handler/symmetric_encrypt.hpp>
#include <netp/handler/http.hpp>

#ifdef NETP_WITH_BOTAN
	#include <netp/handler/tls_client.hpp>
	#include <netp/handler/tls_server.hpp>
	#include <netp/handler/websocket.hpp>
#endif

#include <netp/handler/echo.hpp>
#include <netp/handler/dump_in_text.hpp>
#include <netp/handler/dump_out_text.hpp>
#include <netp/handler/dump_in_len.hpp>
#include <netp/handler/dump_out_len.hpp>

#include <netp/dns_resolver.hpp>
#include <netp/http/message.hpp>
#include <netp/http/parser.hpp>
#include <netp/http/client.hpp>

#include <netp/rpc.hpp>

#include <netp/signal_broker.hpp>
#include <netp/app.hpp>

#include <netp/benchmark.hpp>
#include <netp/test.hpp>
#endif