#ifndef _NETP_HTTP_MESSAGE_HPP
#define _NETP_HTTP_MESSAGE_HPP

#include <string>
#include <list>
#include <unordered_map>

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/string.hpp>

#define NETP_HTTP_CR	"\r"
#define NETP_HTTP_LF	"\n"
#define NETP_HTTP_SP	" "
#define NETP_HTTP_CRLF	"\r\n"
#define NETP_HTTP_COLON	":"
#define NETP_HTTP_COLON_SP	": "

#define NETP_HTTP_METHOD_NAME_MAX_LEN 7

namespace netp { namespace http {

	enum message_type {
		T_REQ,
		T_RESP
	};

	//http://tools.ietf.org/html/rfc2616#section-9.2
	enum option {
		O_NONE = -1,
		O_GET = 0,
		O_HEAD,
		O_POST,
		O_PUT,
		O_DELETE,
		O_CONNECT,
		O_OPTIONS,
		O_TRACE,
		O_M_SEARCH, //ssdp
		O_NOTIFY, //ssdp
		O_SUBSCRIBE,//ssdp
		O_UNSUBSCRIBE,//ssdp
		O_MAX
	};

	extern const char* option_name[(int)netp::http::option::O_MAX];

	struct url_fields {
		string_t schema;
		string_t host;
		string_t path;
		string_t query;
		string_t fragment;
		string_t userinfo;
		u16_t port;
	};

	struct version {
		u16_t major;
		u16_t minor;
	};

	struct header final:
		public netp::ref_base
	{
		struct _H {
			netp::string_t name;
			netp::string_t value;
		};
		const static inline netp::string_t H_key(netp::string_t const& field) {
			netp::string_t _key(field);
#ifdef _NETP_WIN
#pragma warning(push)
#pragma warning(disable:4244)
#endif
			std::transform(_key.begin(), _key.end(), _key.begin(), ::tolower);
#ifdef _NETP_WIN
#pragma warning(pop)
#endif
			return _key;
		}

		typedef std::unordered_map<typename netp::string_t, _H, std::hash<typename netp::string_t>, std::equal_to<typename netp::string_t>, netp::allocator<std::pair<const typename netp::string_t, _H>>>	header_map;
		typedef std::pair<netp::string_t, _H>	header_pair;

		header_map map;
		std::list<netp::string_t> keys_order;

		header() {}
		~header() {}
		void reset() {
			map.clear();
			keys_order.clear();
		}
		bool have(netp::string_t const& field) const {
			header_map::const_iterator&& it = map.find(H_key(field));
			return it != map.end();
		}

		void remove(netp::string_t const& field) {
			header_map::iterator it = map.find(H_key(field));
			if (it != map.end()) {
				const header_pair& HP = *it;
				std::list<netp::string_t>::iterator&& it_key = std::find_if(keys_order.begin(), keys_order.end(), [name = HP.second.name](netp::string_t const& key) {
					return key == name;
				});
				NETP_ASSERT(it_key != keys_order.end());
				keys_order.erase(it_key);
				map.erase(it);
			}
		}

		string_t get(string_t const& field) const {
			header_map::const_iterator&& it = map.find(H_key(field));
			if (it != map.end()) {
				return it->second.value;
			}
			return "";
		}

		void set(string_t const& field, string_t const& value) {

			/*
			 * replace if exists, otherwise add to tail
			 */
			const string_t _key = H_key(field);
			header_map::iterator&& it = map.find(_key);
			if (it != map.end()) {
				it->second.value = value;
				return;
			}

			header_pair pair(_key, { field,value });
			map.insert(pair);

			keys_order.push_back(field);
		}

		void replace(string_t const& field, string_t const& value) {
			header_map::iterator&& it = map.find(H_key(field));
			if (it != map.end()) {
				it->second.value = value;
			}
		}

		void encode(NRP<packet>& packet_o) const {
			NRP<packet> _out = netp::make_ref<packet>();
			std::for_each(keys_order.begin(), keys_order.end(), [&](string_t const& key) {
				header_map::const_iterator&& it = map.find(H_key(key));
				NETP_ASSERT(it != map.end());
				const string_t& value = it->second.value;
				_out->write((netp::byte_t*)key.c_str(), (netp::u32_t)key.length());
				_out->write((netp::byte_t*)NETP_HTTP_COLON_SP, 2);
				_out->write((netp::byte_t*)value.c_str(), (netp::u32_t)value.length());
				_out->write((netp::byte_t*)NETP_HTTP_CRLF, (netp::u32_t)netp::strlen(NETP_HTTP_CRLF));
			});
			packet_o = std::move(_out);
		}
	};

	struct message final:
		public netp::ref_base
	{
		message_type type;
		option opt;
		version ver;

		u16_t code;
		string_t status;
		url_fields urlfields;
		string_t url;

		NRP<header> H;
		NRP<netp::packet> body;

		void encode(NRP<netp::packet>& outp) const;

		string_t dump() const {
			NRP<netp::packet> encp;
			encode(encp);
			return string_t((char*)encp->head(), encp->len());
		}
	};

}}
#endif