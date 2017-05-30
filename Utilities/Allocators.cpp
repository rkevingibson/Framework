#include "Allocators.h"



/*
	This file is very platform specific, so we just split it up accordingly.
	Right now, we'll get linker errors on non-windows platforms. That's fine with me. 

*/

#ifdef WIN32
#include <Windows.h>

using namespace rkg;

void* virtual_memory::ReserveAddressSpace(size_t size, void* location)
{
	return VirtualAlloc(location, size, MEM_RESERVE, PAGE_NOACCESS);
}

void * rkg::virtual_memory::AllocatePhysicalMemory(void * location, size_t size)
{
	return VirtualAlloc(location, size, MEM_COMMIT, PAGE_READWRITE);
}

void virtual_memory::ReleaseAddressSpace(void * location)
{
	VirtualFree(location, 0, MEM_RELEASE);
}

void virtual_memory::DeallocatePhysicalMemory(void* ptr, size_t size)
{
	VirtualFree(ptr, size, MEM_DECOMMIT);
}

#endif


