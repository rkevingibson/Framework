#include "Filesystem.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace rkg
{
	std::vector<std::string> GetDirectoryListing(const char * path, bool files, bool folders)
	{
#ifdef _WIN32
		WIN32_FIND_DATA file;
		char directory[MAX_PATH];
		strcpy_s(directory, path);
		strcat_s(directory, "\\*");
		auto handle = FindFirstFile(directory, &file);

		std::vector<std::string> result;
		do 
		{
			if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && folders) {
				result.push_back(std::string(file.cFileName) + "/");
			}
			else if (files) {
				result.push_back(file.cFileName);
			}
		} while (FindNextFile(handle, &file));

		FindClose(handle);
		return result;
#else
#error "Filesystem not supported on this platform. Working on it."
#endif
	}

	const char * GetFileExtension(const char * path, int len)
	{
		//Go backwards to nearest "."
		const char* ptr = &path[len - 1];
		while (*ptr != '.' && ptr >= path) { ptr--; }
		return ptr + 1;
	}

	time_t GetFileEditedTime(const char* path)
	{
#ifdef _WIN32
		auto file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		FILETIME edit_time;
		bool success = GetFileTime(file, nullptr, nullptr, &edit_time);
		CloseHandle(file);
		if (!success) {
			return 0;
		}
		

		//Convert to time_t, based on https://support.microsoft.com/en-ca/kb/167296
		//Win_time = time_t * 10000000 + 116444736000000000
		ULARGE_INTEGER large_int;
		large_int.HighPart = edit_time.dwHighDateTime;
		large_int.LowPart = edit_time.dwLowDateTime;
		uint64_t win_time = large_int.QuadPart;
		time_t result = (win_time - 116444736000000000) / 10000000;
		return result;
#else
#error "Filesystem not supported on this platform."
#endif
	}
}