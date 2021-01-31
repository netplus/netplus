#ifdef __ANDROID__

#include <netp/log/android_log_print.hpp>
#include <android/log.h>

namespace netp { namespace log {
	android_log_print::android_log_print()
	{}
	
	android_log_print::~android_log_print()
	{}
	
	void android_log_print::write( log_mask const& mask, char const* logstr, netp::size_t const& len ) {
		NETP_ASSERT(test_mask(mask));
		
		if( mask == LOG_MASK_DEBUG) {
			__android_log_print(ANDROID_LOG_DEBUG,"netp","%s",logstr);
		} else if( mask == LOG_MASK_INFO ) {
			__android_log_print(ANDROID_LOG_INFO,"netp","%s",logstr);
		} else if( mask == LOG_MASK_WARN) {
			__android_log_print(ANDROID_LOG_WARN,"netp","%s",logstr);
		} else if( mask == LOG_MASK_ERR ) {
			__android_log_print(ANDROID_LOG_ERROR,"netp","%s",logstr);
		} else {NETP_THROW("WHAT!!, INVALID LOG MASK");}
		
		(void)len;
	}
}}
#endif