#pragma once
#include <inttypes.h>
#include <vector>

namespace cp {

constexpr uint32_t rotl32(uint32_t x, int8_t r)
{
  return (x << r) | (x >> (32 - r));
}

template <int LSB, int Size, typename T>
T read_bitfield(T& v) 
{
	return (v >> LSB) & ((1 << Size) - 1);
} 

template <int LSB, typename T>
T read_bitfield(T& v) 
{
	return read_bitfield<LSB, 1>(v);
}

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
std::pair<typename std::vector<T>::iterator, bool>
insert_sorted_nodupe(std::vector<T>& vec, const T& item)
{
	auto it = std::lower_bound(vec.begin(), vec.end(), item);
	if (it != vec.end() && *it == item)
		return std::make_pair(it, false);
	return std::make_pair(vec.insert(it, item), true);
}

template <typename T>
std::pair<typename std::vector<T>::iterator, bool>
insert_sorted_nodupe(std::vector<T>& vec, typename std::vector<T>::iterator start, const T& item)
{
	auto it = std::lower_bound(start, vec.end(), item);
	if (it != vec.end() && *it == item)
		return std::make_pair(it, false);
	return std::make_pair(vec.insert(it, item), true);
}

} // namespace cp

