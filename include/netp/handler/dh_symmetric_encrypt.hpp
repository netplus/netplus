#ifndef  _NETP_HANDLER_DH_SYMMETRIC_ENCRYPT_HPP
#define _NETP_HANDLER_DH_SYMMETRIC_ENCRYPT_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/security/cipher_abstract.hpp>
#include <netp/security/obfuscate.hpp>
#include <netp/security/dh.hpp>
#include <netp/security/xxtea.hpp>

#include <netp/channel_handler.hpp>

#ifdef NETP_ENABLE_TRACE_DH
	#define NETP_TRACE_DH NETP_INFO
#else
	#define NETP_TRACE_DH(...)
#endif

namespace netp { namespace handler {

#define NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
#ifdef NETP_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
	enum DH_frame_type {
		F_HANDSHAKE_HELLO = 1,
		F_HANDSHAKE_HELLO_REPLY = 2,
		F_HANDSHAKE_OK = 3,
		F_HANDSHAKE_ERROR = 4,

		//below frame not implemented yet
		F_CHANGE_KEY = 5,
		F_CHANGE_KEY_OK = 6,
		F_DATA = 7
	};

	enum DH_cipher_suite {
		CS_NONE = 1,
		CS_XXTEA = 2, //the only supported one right now

		CS_MAX = 3
	};
	enum DH_compress {
		C_NONE = 1,
		C_GZIP = 2,
		C_DFLATE = 3,

		C_MAX = 4
	};
#else
	enum DH_frame_type {
		F_HANDSHAKE_HELLO = 0x01 << 1,
		F_HANDSHAKE_HELLO_RESP = 2,
		F_HANDSHAKE_OK = 0x01 << 3,
		F_HANDSHAKE_ERROR = 0x01 << 4,

		//below frame not implemented yet
		F_CHANGE_KEY = 5,
		F_CHANGE_KEY_OK = 6,
		F_DATA = 7
	};

	enum DH_cipher_suite {
		CS_NONE,
		CS_XXTEA, //the only supported one right now
		CS_MAX
	};
	enum DH_compress {
		C_NONE,
		C_GZIP,
		C_DFLATE,
		C_MAX
	};
#endif

	inline NSP<netp::security::cipher_abstract> make_cipher_suite(DH_cipher_suite const& id) {
		switch (id)
		{
		case CS_XXTEA:
		{
			return netp::atomic_shared::make<netp::security::xxtea>();
		}
		break;
		default:
		{
			return nullptr;
		}
		break;
		}
	}

	struct DH_context {
		netp::security::DH_factor dhfactor;
		u8_t cipher_count;
		u8_t compress_count;
		DH_cipher_suite ciphers[CS_MAX];
		DH_compress compress[C_MAX];
	};

	struct DH_frame {
		u8_t frame;
		NRP<packet> fpacket;
		DH_frame(u8_t const& f) :
			frame(f) {}

		~DH_frame() {}
	};
	struct DH_frame_handshake_hello :
		public DH_frame
	{
		DH_frame_handshake_hello() :DH_frame(F_HANDSHAKE_HELLO) {}

		u64_t pub_key;
		u8_t cipher_count;
		u8_t compress_count;
		DH_cipher_suite ciphers[CS_MAX];
		DH_compress compress[C_MAX];
	};

	struct DH_frame_handshake_hello_reply :
		public DH_frame
	{
		DH_frame_handshake_hello_reply() :DH_frame(F_HANDSHAKE_HELLO_REPLY) {}

		u64_t pub_key;
		DH_cipher_suite cipher;
		DH_compress compress;
	};

	struct DH_frame_handshake_ok :
		public DH_frame
	{
		DH_frame_handshake_ok() :DH_frame(F_HANDSHAKE_OK) {}
	};

	struct DH_frame_data :
		public DH_frame
	{
		DH_frame_data() :DH_frame(F_DATA) {}
	};

	class dh_symmetric_encrypt final:
		public netp::channel_handler_abstract
	{
		enum DH_state {
			DH_IDLE,
			DH_HANDSHAKE,
			DH_DATA_TRANSFER,
			DH_CHANGE_KEY,
			DH_ERROR
		};

		DH_state m_dhstate;
		DH_context m_context;

		NSP<netp::security::cipher_abstract> m_cipher;
	public:
		dh_symmetric_encrypt();
		virtual ~dh_symmetric_encrypt();

		void handshake_assign_dh_factor(netp::security::DH_factor factor);

		void handshake_make_hello_packet(NRP<packet>& hello) ;
		void handshake_packet_arrive( NRP<channel_handler_context> const& ctx, NRP<packet> const& in );

		void connected(NRP<channel_handler_context> const& ctx)  override;
		void closed(NRP<channel_handler_context> const& ctx) override ; 
		void read_closed(NRP<channel_handler_context> const& ctx) override;
		void write_closed(NRP<channel_handler_context> const& ctx) override;

		void read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) override;
		void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;
	};
}}

#endif