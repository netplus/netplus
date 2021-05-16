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

	// Field names are case-insensitive.
	// The order in which header fields with differing field names are received is not significant. However, it is "good practice" to send general-header fields first,
	// The order in which header fields with the same field-name are received is therefore significant to the interpretation of the combined field value, and thus a proxy MUST NOT change the order of these field values when a message is forwarded.

	//@date 2021-05-15
	// add support for the following header format
	// header - 1: aaa0, aaa1
	//	header - 2 : aaa0
	//	header - 2 : aaa1
	
	//when forwards for a proxy tunel, we should alway gurantee:
	//1, all header fileds order from the src
	//2, do not merge multi line header field when do forwarding

	struct header final:
		public netp::ref_base
	{
		struct mul_hfv_pos {
			size_t begin;
			size_t len;
		};

		typedef std::list<mul_hfv_pos> mul_header_filed_with_same_name_pos_list_t;
		struct _header_line {
			netp::string_t name;
			netp::string_t value;
			mul_header_filed_with_same_name_pos_list_t mul_hf_pos_list;
		};

		const static inline netp::size_t H_key(netp::string_t const& field) {
			return ihash_seq((const unsigned char*)field.c_str(), field.length());
		}

		typedef std::unordered_map<size_t, _header_line, std::hash<size_t>, std::equal_to<size_t>, netp::allocator<std::pair<const size_t, _header_line>>>	header_map;
		typedef std::pair<size_t, _header_line>	header_pair;
		typedef std::list<netp::string_t, netp::allocator<netp::string_t>> keys_order_list_t;

		header_map map;
		keys_order_list_t keys_order;

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
			size_t key = H_key(field);
			header_map::iterator it = map.find(key);
			if (it != map.end()) {
				keys_order_list_t::iterator it_key = std::find_if(keys_order.begin(), keys_order.end(), [name = it->second.name](netp::string_t const& key) {
					return key == name;
				});
				NETP_ASSERT(it_key != keys_order.end());
				keys_order.erase(it_key);
				map.erase(it);

				it = map.find(key);
			}
		}

		//return merged field value automatically
		string_t get(string_t const& field) const {
			header_map::const_iterator&& it = map.find(H_key(field));
			if (it != map.end()) {
				return it->second.value;
			}
			return "";
		}

		void add_header_line(string_t const& field, string_t const& value) {
			const size_t _key = H_key(field);
			header_map::iterator&& it = map.find(_key);
			if (it != map.end()) {
				mul_hfv_pos hf_pos = {value.length() + 2 /*netp::strlen(", ")*/, value.length()  };
				it->second.value += ", " + value;
				it->second.mul_hf_pos_list.push_back(hf_pos);

#ifdef _NETP_DEBUG
				keys_order_list_t::iterator kit = std::find_if(keys_order.begin(), keys_order.end(), [name=it->second.name](netp::string_t const& key) {
					return key == name;
				});
				NETP_ASSERT( kit != keys_order.end() );
#endif
				return;
			}

			mul_header_filed_with_same_name_pos_list_t hf_pos_list;
			hf_pos_list.push_back({ 0, value.length() });
			map.insert({ _key, { field,value, hf_pos_list } });
			keys_order.push_front(field);
		}

		void replace(string_t const& field, string_t const& value) {
			header_map::iterator&& it = map.find(H_key(field));
			if (it != map.end()) {
				it->second.value = value;
				it->second.mul_hf_pos_list.clear();
				it->second.mul_hf_pos_list.push_back({ 0, value.length() });
			}
		}

		void encode(NRP<packet>& packet_o) const {
			NRP<packet> _out = netp::make_ref<packet>();
			std::for_each(keys_order.rbegin(), keys_order.rend(), [&](string_t const& key) {
				header_map::const_iterator&& it = map.find(H_key(key));
				NETP_ASSERT(it != map.end());
				const char* value_cstr = it->second.value.c_str();
				mul_header_filed_with_same_name_pos_list_t::const_iterator&& it_hf_pos = it->second.mul_hf_pos_list.begin();
				while (it_hf_pos != it->second.mul_hf_pos_list.end()) {
					_out->write((netp::byte_t*)key.c_str(), (netp::u32_t)key.length());
					_out->write((netp::byte_t*)NETP_HTTP_COLON_SP, 2);
					_out->write((netp::byte_t*)(value_cstr + it_hf_pos->begin), u32_t(it_hf_pos->len) );
					_out->write((netp::byte_t*)NETP_HTTP_CRLF, (netp::u32_t)netp::strlen(NETP_HTTP_CRLF));
					++it_hf_pos;
				}
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