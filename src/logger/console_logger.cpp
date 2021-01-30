#include <netp/logger/console_logger.hpp>

namespace netp { namespace logger {

	console_logger::console_logger() :
	logger_abstract()
	{
	}

	console_logger::~console_logger() {
	}

	void console_logger::write( log_mask mask, char const* log, netp::size_t len ) {
		NETP_ASSERT(test_mask(mask));
		printf("%s\n", log );
		(void)&len;
	}

}}