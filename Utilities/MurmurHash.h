#pragma once
#include "../Utilities/Utilities.h"
//Class to help compute hash of arbitrary data.
namespace rkg {
	class MurmurHash
	{
	public:
		inline void Seed(uint32_t seed) { hash_ = seed; }
		void Add(const char* data, const uint32_t num);
		void Add(const uint32_t word);
		uint32_t Finish();

	private:
		inline void HashWord(const uint32_t word) {
			constexpr uint32_t c1 = 0xcc9e2d51;
			constexpr uint32_t c2 = 0x1b873593;
			constexpr uint32_t r1 = 15;
			constexpr uint32_t r2 = 13;
			constexpr uint32_t m = 5;
			constexpr uint32_t n = 0xe6546b64;

			uint32_t k = word * c1;
			k = rotl(k, r1);
			k *= c2;
			hash_ ^= k;
			hash_ = rotl(hash_, r2) * m + n;
			len_ += 4;
		}
		uint32_t len_{ 0 }; //Length of hash info in bytes.
		uint32_t hash_{ 0 };
		uint32_t slop_{ 0 };
		uint32_t slop_bytes_{ 0 };
	};

	inline void MurmurHash::Add(const char* data, const uint32_t num)
	{
		
		uint32_t i;
		uint32_t temp;

		for (i = 0; slop_bytes_ < 4 && i < num; i++, slop_bytes_++) {
			slop_ = (slop_ << 8) | data[i];
		}

		if (slop_bytes_ == 4) {
			HashWord(slop_);
			slop_ = 0;
			slop_bytes_ = 0;
		}

		if (i == num) {
			return;//Out of data to hash.
		}

		while (i+4 < num) {
			temp = *(reinterpret_cast<const uint32_t*>(data + i));
			HashWord(temp);
			i += 4;
		}
		
		slop_bytes_ = (num - i);
		slop_ = 0;
		for (; i < num; i++) {
			slop_ = slop_ << 8 | data[i];
		}
		
	}

	inline void MurmurHash::Add(const uint32_t word)
	{
		uint32_t temp = (slop_ << 8 * (4 - slop_bytes_)) | (word >> 8 * slop_bytes_);
		slop_ = word & (0xFFFFFFFF >> 8 * (4 - slop_bytes_));
		HashWord(temp);
	}

	inline uint32_t MurmurHash::Finish()
	{
		if (slop_ > 0) Add(0);

		hash_ ^= len_;
		hash_ ^= (hash_ >> 16);
		hash_ *= 0x85ebca6b;
		hash_ ^= (hash_ >> 13);
		hash_ *= 0xc2b2ae35;
		hash_ ^= (hash_ >> 16);
		return hash_;
	}
}