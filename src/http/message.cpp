#include <netp/http/message.hpp>

namespace netp { namespace http {

	const char* option_name[(int)netp::http::option::O_MAX] = {
		"GET",
		"HEAD",
		"POST",
		"PUT",
		"DELETE",
		"CONNECT",
		"OPTIONS",
		"TRACE",
		"M-SEARCH",
		"NOTIFY",
		"SUBSCRIBE",
		"UNSUBSCRIBE"
	};

	void message::encode(NRP<packet>& outp) const {
		NRP<packet> _outp = netp::make_ref<packet>();
		NETP_ASSERT(ver.major != 0);
		char tmp[256];
		int n = 0;
		if (type == T_REQ) {
			_outp->write((netp::byte_t*)option_name[(int)opt], (netp::u32_t)netp::strlen(option_name[(int)opt]) );
			_outp->write<u8_t>(' ');
			_outp->write((netp::byte_t*)urlfields.path.c_str(), urlfields.path.length());
			if (urlfields.query.length()) {
				_outp->write<u8_t>('?');
				_outp->write((netp::byte_t*)urlfields.query.c_str(), urlfields.query.length());
			}
			n = snprintf(tmp, 256, " HTTP/%d.%d\r\n", ver.major, ver.minor);
			NETP_ASSERT(n > 0 && n < 256);
			_outp->write((netp::byte_t*)tmp, n);
		} else if (type == T_RESP) {
			NETP_ASSERT(status.length() > 0);
			n = snprintf(tmp, 256, "HTTP/%d.%d %d %s\r\n", ver.major, ver.minor, code, status.c_str());
			NETP_ASSERT(n > 0 && n < 256);
			_outp->write((netp::byte_t*)tmp, n);
		}

		NETP_ASSERT(H != nullptr);
		NRP<packet> H_packet;
		H->encode(H_packet);
		const bool has_body = (body != nullptr && (body->len() > 0));

		if (has_body && !H->have("Content-Length") && !H->have("Transfer-Encoding")) {
			int rt = snprintf(tmp, 64, "Content-Length: %u\r\n", body->len());
			NETP_ASSERT(rt > 0 && rt < 64);
			_outp->write((netp::byte_t*)tmp, rt);
		}
		H_packet->write((netp::byte_t*)NETP_HTTP_CRLF, u32_t(netp::strlen(NETP_HTTP_CRLF)));
		_outp->write(H_packet->head(), H_packet->len());
		if (has_body) {
			_outp->write((netp::byte_t*)body->head(), body->len());
		}
		outp = std::move(_outp);
	}
}}