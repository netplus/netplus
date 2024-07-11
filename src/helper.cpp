#include <iostream>
#include <fstream>

#include <netp/core.hpp>
#include <netp/helper.hpp>

namespace netp {

	char** duplicate_argv(int argc, char* const argv[]) {
		char** new_argv = new char*[argc + 1]; // +1 for the null terminator
		for (int i = 0; i < argc; ++i) {
			new_argv[i] = new char[strlen(argv[i]) + 1];
			strcpy(new_argv[i], argv[i]);
		}
		new_argv[argc] = nullptr; // Null terminate the array
		return new_argv;
	}

	// Function to free duplicated argv
	void free_duplicated_argv(int argc, char** argv) {
		for (int i = 0; i < argc; ++i) {
			delete[] argv[i];
		}
		delete[] argv;
	}

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

#define __NETP_PATH_MAX (2048)
#ifdef _NETP_GNU_LINUX
	char* realpath(const char* path, char* resolved_path) {
		char cwd[__NETP_PATH_MAX] = {0};
		if (resolved_path == NULL) {
			resolved_path = (char*) std::malloc(__NETP_PATH_MAX);
			if (resolved_path == NULL) {
				return NULL;
			}
		}

		if (path == NULL) {
			if (getcwd(cwd, __NETP_PATH_MAX) == NULL) {
				return NULL;
			}
		}
		else if (path[0] != '/') {
			//update cwd if it's not start by '/'
			if (getcwd(cwd, __NETP_PATH_MAX) == NULL) {
				return NULL;
			}
		}

		//clean begin
		char* token;
		char* next_token;
		char* buffer = (char*) std::malloc(__NETP_PATH_MAX);
		if (buffer == NULL) {
			return NULL;
		}
		std::strncpy(buffer, path, __NETP_PATH_MAX);
		char* slash = strstr(buffer, "//");
		while (slash != NULL) {
			std::memmove(slash, slash + 1, strlen(slash + 1) + 1);
			slash = strstr(buffer, "//");
		}
		token = strtok_r(buffer, "/", &next_token);
		std::strncpy(resolved_path, cwd, __NETP_PATH_MAX);
		while (token != NULL) {
			if (std::strcmp(token, "..") == 0) {
				char* last_slash = strrchr(resolved_path, '/');
				if (last_slash != NULL) {
					*last_slash = '\0';
				}
}
			else if (std::strcmp(token, ".") == 0) {
				// Ignore "." path segments
			}
			else {
				std::strncat(resolved_path, "/", __NETP_PATH_MAX - strlen(resolved_path) - 1);
				std::strncat(resolved_path, token, __NETP_PATH_MAX - strlen(resolved_path) - 1);
			}
			token = strtok_r(NULL, "/", &next_token);
		}
		std::free(buffer);
		return resolved_path;
	}
#endif

	std::string filename(std::string const& filepathname) 
	{
#ifdef _NETP_WIN
	#ifdef UNICODE
		std::wstring wfilepathname;
		netp::chartowchar(filepathname.c_str(), filepathname.length(), wfilepathname);
		std::size_t last_slash = wfilepathname.find_last_of(_T('\\'));
		if (last_slash == std::wstring::npos) {
			return filepathname;
		}
		std::wstring wfilename = wfilepathname.substr(last_slash+1);
		std::string return_buf;
		netp::wchartochar(wfilename.c_str(), wfilename.length(), return_buf);
		return return_buf;
	#else
		std::size_t last_slash = filepathname.find_last_of('\\');
		if (last_slash == std::string::npos) {
			return filepathname;
		}
		return  filepathname.substr(last_slash + 1);
	#endif
#else
		std::size_t last_slash = filepathname.find_last_of('/');
		if (last_slash == std::string::npos) {
			return filepathname;
		}
		return  filepathname.substr(last_slash + 1); 
#endif
	}


	std::string absolute_parent_path(std::string const& filepathname)
	{
#ifdef _NETP_WIN
	#ifdef UNICODE
		WCHAR absolute_path_normalized[MAX_PATH + 1];
		std::wstring wfilepathname;
		netp::chartowchar(filepathname.c_str(), filepathname.length(), wfilepathname);
		if (GetFullPathName(wfilepathname.c_str(), MAX_PATH, absolute_path_normalized, NULL) == 0)
		{
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][to_absolute_path]GetFullPathName failed, path: %s", filepathname.c_str());
			NETP_THROW(_errmsg);
		}

		WCHAR* last_slash = netp::strrchr(absolute_path_normalized, _T('\\'));
		if (last_slash != NULL) {
			*last_slash = _T('\0');
		}
		std::string return_buf;
		netp::wchartochar(absolute_path_normalized, netp::strlen(absolute_path_normalized), return_buf );
		return return_buf;
	#else
		CHAR absolute_path_normalized[MAX_PATH + 1];
		if (GetFullPathName(filepathname.c_str(), MAX_PATH, absolute_path_normalized, NULL) == 0)
		{
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][parent_path]GetFullPathName failed, filepathname: %s", filepathname.c_str());
			NETP_THROW(_errmsg);
		}

		char* last_slash = netp::strrchr(absolute_path_normalized, _T('\\'));
		if (last_slash != NULL) {
			*last_slash = ('\0');
		}
		std::string return_buf(absolute_path_normalized, netp::strlen(absolute_path_normalized));
		return return_buf;
	#endif
#else
		char _buf[__NETP_PATH_MAX];
		char* absolute_path_normalized = netp::realpath(filepathname.c_str(), _buf);
		if (absolute_path_normalized == NULL) {
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][parent_path]realpath failed, filepathname: %s, errno: %d", filepathname.c_str(), netp_last_errno() );
			NETP_THROW(_errmsg);
		}

		char* last_slash = netp::strrchr(absolute_path_normalized, ('/'));
		if (last_slash != NULL) {
			*last_slash = '\0';
		}
		std::string return_buf(absolute_path_normalized, netp::strlen(absolute_path_normalized));
		return return_buf;
#endif
	}

	bool file_exists(std::string const& file_path) {
		std::ifstream file(file_path);
		return file.good();
	}

	std::string current_directory() 
	{
		std::string cd;
#ifdef _NETP_WIN
	#ifdef UNICODE
		wchar_t _buf[__NETP_PATH_MAX];
		if (!_tgetcwd(_buf, __NETP_PATH_MAX))
		{
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][current_directory]_tgetcwd failed, path");
			NETP_THROW(_errmsg);
		}
		netp::wchartochar<std::string>(_buf, netp::strlen(_buf), cd);
		return cd;
	#else
		std::string cwd;
		char _buf[__NETP_PATH_MAX];
		if (!_getcwd(_buf), __NETP_PATH_MAX)
		{
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][to_absolute_path]_getcwd failed" );
			NETP_THROW(_errmsg);
		}
		return std::string(_buf, netp::strlen(_buf) );
	#endif
#else
		char _buf1[__NETP_PATH_MAX];
		/* getcwd return null-teminated cstr if succeed */
		if (getcwd(_buf1, __NETP_PATH_MAX) == NULL)
		{
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][to_absolute_path]getcwd failed, errno: %d", netp_last_errno() );
			NETP_THROW(_errmsg);
		}
		return std::string(_buf1, netp::strlen(_buf1) );
#endif
	}

	
	std::string to_absolute_path(std::string const& relative, std::string const& parent_path )
	{
#ifdef _NETP_WIN
		const char* pathstr = relative.c_str();
		char path_0 = *pathstr;
		char path_1 = *(pathstr + 1);
		char path_2 = *(pathstr + 2);

		if (((path_0) >= 'A' && (path_0) <= 'Z')
			|| ((path_0) >= 'a' && (path_0) <= 'z')
			)
		{
			if ((path_1 == ':') && ((path_2 == '\\') || path_2 == '/'))
			{
				return relative;
			}
		}

	#ifdef UNICODE
			std::wstring w_parent;
			std::wstring w_relative;
			netp::chartowchar<std::wstring>(parent_path.c_str(), parent_path.length(), w_parent);
			netp::chartowchar<std::wstring>(relative.c_str(), parent_path.length(), w_relative);

			w_parent.append(_T("\\"));
			w_parent.append(w_relative);

			TCHAR absolute_path_normalized[MAX_PATH + 1];
			if (GetFullPathName(w_parent.c_str(), MAX_PATH, absolute_path_normalized, NULL) == 0)
			{
				char _errmsg[1024];
				snprintf(_errmsg, 1024, "[netp][to_absolute_path]GetFullPathName failed, path: %s", relative.c_str() );
				NETP_THROW(_errmsg);
			}

			std::string buf_to_return;
			netp::wchartochar<std::string>(absolute_path_normalized, netp::strlen(absolute_path_normalized), buf_to_return);
			return buf_to_return;
		#else

			parent_path.append(("\\"));
			parent_path.append(relative);

			TCHAR absolute_path_normalized[MAX_PATH + 1];
			if (GetFullPathName(parent_path.c_str(), MAX_PATH, absolute_path_normalized, NULL) == 0)
			{
				char _errmsg[1024];
				snprintf(_errmsg, 1024, "[netp][to_absolute_path]GetFullPathName failed, path: %s", relative.c_str());
				NETP_THROW(_errmsg);
			}
			return std::string(absolute_path_normalized, netp::strlen(absolute_path_normalized) );
	#endif
#else
		if ( *(relative.c_str())== ('/') ) {
			return relative;
		}

		char _buf1[__NETP_PATH_MAX];
		char _buf2[__NETP_PATH_MAX];
		std::size_t parent_path_len = parent_path.length();
		netp::strncpy(_buf1, parent_path.c_str(), parent_path_len);
		netp::strncpy(_buf1 + parent_path_len, "/", 1 );
		netp::strncpy(_buf1 + parent_path_len + 1, relative.c_str() , relative.length() );

		char* absolute_path_normalized = realpath(_buf1, _buf2);
		if (absolute_path_normalized == NULL) {
			char _errmsg[1024];
			snprintf(_errmsg, 1024, "[netp][to_absolute_path]realpath failed, path: %s, errno: %d", relative.c_str(), netp_last_errno() );
			NETP_THROW(_errmsg);
		}
		return std::string(_buf2, netp::strlen(_buf2));
#endif
	}


	bool helper_test_unit::run()
	{
#ifdef _NETP_GNU_LINUX
		const char* path[] = { 
			"/home/copy/file.log",
			"/home/copy/./file.log",
			"/home/copy//file.log",
			"/home/copy/../../file.log",
			"/home/copy/../..//file.log",
		};
		const char* realpath[] = {
			"/home/copy/file.log",
			"/home/copy/file.log",
			"/home/copy/file.log",
			"/file.log",
			"/file.log",
		};

		for (size_t i = 0; i < sizeof(path) / sizeof(path[0]); ++i)
		{
			char* real = netp::realpath( path[i], NULL );
			NETP_ASSERT(netp::strcmp(realpath[i], real) == 0, "path: %s, expected: %s, get: %s", path[i], realpath[i], real );
			std::free(real);
		}
#endif

		return true;
	}

	void helper_test_unit::test_realpath()
	{

	}
}