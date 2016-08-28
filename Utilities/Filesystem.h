#pragma once

/*
	A small collection of filesystem functions to make life easier.
*/

#include <string>
#include <vector>
#include <ctime>
/*
Unsure about api for some of these - dealing with a lot of strings. Could do this a couple of ways. 
If I use the STL, its easy - return a bunch of vectors of strings, no problem. Obviously not great for memory allocation.
I could write my own containers and strings, which might help a bit, but wouldn't be great. 
Win32 has a FindFirstFile, FindNextFile, FindClose API, which isn't terrible, but FindClose kinda sucks.
Linux is similar - Open a directory, loop through it, then close it. Ugh.
I think I would prefer a GetDirectoryListing(filepath, int index, char** buffer, int buffer_size);
That's not terrible, returns the string every time, no memory management in the function, still same looping over it.
Problem is that it doesn't map well to the existing API - for windows, I'd need to open up, loop over to the right spot, every call.
So I think the only good abstraction is to either do the same in a platform-independent way, or return a container.
TBH, I'm thinking STL might be the easiest solution here, at least for now.
For a game, maybe I'd do more, but for my applications this will be fine.
To do it right and easy, I'd want some container that can hold a collection of immutable strings in a nice way - 
basically one long char array, that I separate by null. That can wait, though it would be fun to write. Down the rabbit hole another day.
*/

namespace rkg 
{
	std::vector<std::string> GetDirectoryListing(const char* path, bool files = true, bool folders = true);

	time_t GetFileEditedTime(const char* path);//Returns time that the file was last edited/modified.
}