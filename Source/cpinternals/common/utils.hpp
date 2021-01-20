#pragma once
#include <inttypes.h>

namespace cp {

constexpr uint16_t byteswap(uint16_t value) noexcept
{
	return (value << 8) | (value >> 8);
}

constexpr uint32_t byteswap(uint32_t value) noexcept
{
	uint32_t tmp = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0x00FF00FF);
	return (tmp << 16) | (tmp >> 16);
}

template <typename T>
typename std::vector<T>::iterator 
insert_sorted(std::vector<T>& vec, const T& item)
{
	return vec.insert( 
		std::lower_bound(vec.begin(), vec.end(), item),
		item);
}

template <typename T>
typename std::vector<T>::iterator 
insert_sorted_nodupe(std::vector<T>& vec, const T& item)
{
	auto it = std::lower_bound(vec.begin(), vec.end(), item);
	if (*it == item)
		return vec.end();
	return vec.insert(it, item);
}


} // namespace cp

