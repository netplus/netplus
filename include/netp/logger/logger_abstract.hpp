#ifndef _NETP_LOGGER_LOGGER_ABSTRACT_HPP_
#define _NETP_LOGGER_LOGGER_ABSTRACT_HPP_

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

namespace netp { namespace logger {

	enum log_level {
		LOG_LEVEL_ERR	= 0,
		LOG_LEVEL_WARN	= 1,
		LOG_LEVEL_INFO	= 2,
		LOG_LEVEL_DEBUG = 3,
		LOG_LEVEL_MAX	= 4
	};

	enum log_mask {
		LOG_MASK_DEBUG = 0x01, //for netp debug
		LOG_MASK_INFO = 0x02, //for generic debug
		LOG_MASK_WARN = 0x04, //warning, refuse client service
		LOG_MASK_ERR = 0x08 //fatal issue, terminate client service
	};

	const static char __log_mask_char[9] = {
		' ',
		'D',
		'I',
		' ',
		'W',
		' ',
		' ',
		' ',
		'E'
	};

	#define LOG_LEVELS_ERR			( netp::logger::LOG_MASK_ERR)
	#define LOG_LEVELS_WARN		( LOG_LEVELS_ERR | netp::logger::LOG_MASK_WARN )
	#define LOG_LEVELS_INFO			( LOG_LEVELS_WARN | netp::logger::LOG_MASK_INFO )
	#define LOG_LEVELS_DEBUG		( LOG_LEVELS_INFO | netp::logger::LOG_MASK_DEBUG )

	class logger_abstract:
		public netp::ref_base
	{
		NETP_DECLARE_NONCOPYABLE(logger_abstract)

	public:
		logger_abstract():
			m_level_masks(translate_to_mask_from_level(NETP_DEFAULT_LOG_LEVEL))
		{
		}

		virtual ~logger_abstract() {}

		const inline static int translate_to_mask_from_level( u8_t level_int ) {
			const static int log_level_mask_config[LOG_LEVEL_MAX] = {
				LOG_LEVELS_ERR,
				LOG_LEVELS_WARN,
				LOG_LEVELS_INFO,
				LOG_LEVELS_DEBUG
			};
			if (level_int < LOG_LEVEL_MAX) {
				return log_level_mask_config[level_int];
			}
			return log_level_mask_config[NETP_DEFAULT_LOG_LEVEL];
		}

		void add_mask( int mask /*level mask set*/) {
			m_level_masks |= mask;
		}

		void remove_mask( int masks ) {
			m_level_masks &= ~masks;
		}

		inline void set_mask_by_level( u8_t level ) {
			m_level_masks = translate_to_mask_from_level(level) ;
		}
		inline void reset_masks() {
			m_level_masks = translate_to_mask_from_level(NETP_DEFAULT_LOG_LEVEL);
		}

		virtual void write( log_mask level, char const* log, netp::size_t len ) = 0 ;

		inline void debug( char const* log, netp::size_t len ) {
			write( LOG_MASK_DEBUG, log, len );
		}
		inline void info( char const* log, netp::size_t len ) {
			write( LOG_MASK_INFO, log, len );
		}
		inline void warn( char const* log, netp::size_t len ) {
			write( LOG_MASK_WARN, log, len );
		}
		inline void fatal( char const* log, netp::size_t len ) {
			write( LOG_MASK_ERR, log, len );
		}

		inline bool test_mask( log_mask const& mask) {
			return ((m_level_masks & mask ) != 0 );
		}

	private:
		int m_level_masks;
	};

}}

#endif