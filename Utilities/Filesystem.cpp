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

		return result;
#else
#error "Filesystem not supported on this platform. Working on it."
#endif
	}
}