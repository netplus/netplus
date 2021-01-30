#ifdef __ANDROID__

#include <wawo/log/android_log_print.hpp>
#include <android/log.h>

namespace wawo { namespace log {
	android_log_print::android_log_print()
	{}
	
	android_log_print::~android_log_print()
	{}
	
	void android_log_print::write( log_mask const& mask, char const* logstr, wawo::size_t const& len ) {
		WAWO_ASSERT(test_mask(mask));
		
		if( mask == LOG_MASK_DEBUG) {
			__android_log_print(ANDROID_LOG_DEBUG,"WAWO","%s",logstr);
		} else if( mask == LOG_MASK_INFO ) {
			__android_log_print(ANDROID_LOG_INFO,"WAWO","%s",logstr);
		} else if( mask == LOG_MASK_WARN) {
			__android_log_print(ANDROID_LOG_WARN,"WAWO","%s",logstr);
		} else if( mask == LOG_MASK_ERR ) {
			__android_log_print(ANDROID_LOG_ERROR,"WAWO","%s",logstr);
		} else {WAWO_THROW("WHAT!!, INVALID LOG MASK");}
		
		(void)len;
	}
}}
#endif