#pragma once
#define WIN32

#include <cstdint>
#include <cmath>


#ifdef WIN32
#	define NOMINMAX
#	include <intrin.h>
#endif

#define MACRO2STR_IMPL(x) #x
#define MACRO2STR(x) MACRO2STR_IMPL(x)
#ifdef WIN32
#	define ASSERT(cond) do {if(!(cond)) __debugbreak();} while(0)
#else
#	include <cassert>
#	define ASSERT(cond) assert(cond)
#endif

#define KILO(x) (1024*x)
#define MEGA(x) (1024L*1024*x)
#define GIGA(x) (1024L*1024L*1024L*x)

#define RKG_ARRAY_LENGTH(arr) ((int)(sizeof(arr)/sizeof(*arr)))

/*
====================
ASSERTIONS
====================
*/

//Here, define Expects and Ensures, as defined in the C++ Core Guidelines.
#define Expects(cond) ASSERT((cond) && "Precondition failure at " __FILE__ ": " MACRO2STR(__LINE__))
#define Ensures(cond) ASSERT((cond) && "Postcondition failure at " __FILE__ ": " MACRO2STR(__LINE__))

namespace rkg
{

/*
====================
CORE FRAMEWORK TYPES
====================
*/

struct MemoryBlock
{
	void* ptr;
	size_t length;
};



/*
====================
UTILITY FUNCTIONS
====================
*/
//Number of bits set in a 32 bit word. 
inline uint8_t popcount(uint32_t v)
{
#ifdef WIN32
	return __popcnt(v);
#else
	/*
	See section 8.6, Software optimization guide for AMD64 Processors
	If visual studio gets better, this could be a constexpr function.
	*/
	const auto w = v - ((v >> 1) & 0x55555555);
	const auto x = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	const auto y = (x + (x >> 4)) & 0x0F0F0F0F;
	return (y * 0x01010101) >> 24;
#endif
}

inline uint32_t rotl(uint32_t v, uint32_t s)
{
#ifdef WIN32
	return _rotl(v, s);
#else
	return ((x << y) | (x >> (32 - y)));
#endif
}

inline size_t log2(size_t v)
{
#ifdef WIN32
	unsigned long index;
	_BitScanReverse64(&index, v);
	return index;
#else
	return round(std::log2(v));
#endif

}

inline size_t RoundToAligned(size_t n, size_t alignment)
{
	return (n + alignment - 1) & ~(alignment - 1);
}

//Round up n to the nearest power of 2.
inline size_t RoundToPow2(size_t n)
{
	return 1ull << (log2(n) + 1);
}

constexpr inline unsigned int Min(unsigned int a, unsigned int b)
{
	return (a < b) ? a : b;
}

constexpr inline unsigned int Max(unsigned int a, unsigned int b)
{
	return (a > b) ? a : b;
}

//A couple compile-time hash functions that work on strings literals. Great for IDs!
constexpr uint32_t hash32_val_const = 0x811c9dc5;
constexpr uint32_t hash32_prime_const = 0x1000193;
constexpr inline uint32_t hash32(const char* const str, const uint32_t value = hash32_val_const) noexcept {
	return (str[0] == '\0') ? value : hash32(&str[1], (value ^ uint32_t(str[0])) * hash32_prime_const);
}

constexpr uint64_t hash64_val_const = 0xcbf29ce484222325;
constexpr uint64_t hash64_prime_const = 0x100000001b3;
constexpr inline uint64_t hash64(const char* const str, const uint64_t value = hash64_val_const) noexcept {
	
	return (str[0] == '\0') ? value : hash64(&str[1], (value ^ uint64_t(str[0])) * hash64_prime_const);
}

/*
====================
CONVENIENCE TYPES
====================
*/

using byte = unsigned char;

}