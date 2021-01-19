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

} // namespace cp

