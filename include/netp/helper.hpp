#ifndef _NETP_FILE_HELPER_HPP
#define _NETP_FILE_HELPER_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>

namespace netp { namespace file {
	extern NRP<netp::packet> file_get_content(std::string const& filepath);
} }

#endif