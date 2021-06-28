#include <netp/helper.hpp>

namespace netp {

	NRP<netp::packet> file_get_content(netp::string_t const& filepath) {
		std::FILE* fp = std::fopen(filepath.c_str(), "rb");

		if (fp == 0) {
			return nullptr;
		}

		std::fseek(fp, 0, SEEK_END);
		std::size_t fsize = std::ftell(fp);
		std::fseek(fp, 0, SEEK_SET);

		NRP<netp::packet> data = netp::make_ref<netp::packet>(u32_t(fsize));
		std::size_t rsize = std::fread(data->head(), 1, fsize, fp);
		std::fclose(fp);
		NETP_ASSERT(rsize == fsize);
		data->incre_write_idx(u32_t(fsize));
		return data;
	}
}