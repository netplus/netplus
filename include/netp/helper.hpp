#ifndef _NETP_FILE_HELPER_HPP
#define _NETP_FILE_HELPER_HPP

#include <netp/core.hpp>
#include <netp/string.hpp>
#include <netp/packet.hpp>
#include <netp/test.hpp>

namespace netp {
	extern NRP<netp::packet> file_get_content(netp::string_t const& filepath);

	extern std::string filename(std::string const& filepathname);
	extern std::string absolute_parent_path(std::string const& filepathname);
	extern std::string current_directory();
	/* relative could be path or filepath */
	extern std::string to_absolute_path(std::string const& relative, std::string const& parent_path = current_directory() );


	extern char** duplicate_argv(int argc, char* const argv[]) ;
	extern void free_duplicated_argv(int argc, char** argv) ;

	struct helper_test_unit :
		public netp::test_unit
	{
		bool run();
		void test_realpath();
	};
}

#endif