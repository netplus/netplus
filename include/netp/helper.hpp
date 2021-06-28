#ifndef _NETP_FILE_HELPER_HPP
#define _NETP_FILE_HELPER_HPP

#include <netp/core.hpp>
#include <netp/string.hpp>
#include <netp/packet.hpp>

namespace netp {
	extern NRP<netp::packet> file_get_content(netp::string_t const& filepath);
}

#endif