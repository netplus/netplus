#include <netp/helper.hpp>

namespace netp { namespace file {

	NRP<netp::packet> file_get_content(std::string const& filepath) {
		std::FILE* fp = std::fopen(filepath.c_str(), "rb");

		std::fseek(fp, 0, SEEK_END);
		std::size_t fsize = std::ftell(fp);
		std::fseek(fp, 0, SEEK_SET);

		NRP<netp::packet> data = netp::make_ref<netp::packet>(fsize);
		std::size_t rsize = std::fread(data->head(), 1, fsize, fp);
		std::fclose(fp);
		NETP_ASSERT(rsize == fsize);
		data->incre_write_idx(fsize);
		return data;
	}
}}