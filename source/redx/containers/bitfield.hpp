#pragma once 
#include <type_traits> 
#include <utility> 
 
#include <redx/common.hpp> 
 
namespace redx { 
 
// for controlled bitfield packing 
template <typename PackT, typename T, size_t BitIndex, size_t Bits = 1> 
class bitfield_member 
{ 
  static_assert(std::is_arithmetic_v<T>); 
 
  static constexpr PackT mask = (static_cast<PackT>(1) << Bits) - 1; 
 
public: 
 
  inline bitfield_member& operator=(T value) 
  { 
    const T masked = value & mask;

    if (value != masked)
    {
      throw std::range_error("bitfield_member::operator=: value overflow");
    }

    packed = (packed & ~(mask << BitIndex)) | ((value & mask) << BitIndex); 
    return *this; 
  }
 
  T operator()() const 
  { 
    return (packed >> BitIndex) & mask; 
  } 

  operator T() const 
  { 
    return this->operator()(); 
  } 

  constexpr T max()
  {
    return static_cast<T>((1ull << Bits) - 1);
  }

private: 
 
  PackT packed; 
}; 
 
template <typename T, size_t BitIndex, size_t Bits = 1> 
using bfm8  = bitfield_member< uint8_t, T, BitIndex, Bits>; 
 
template <typename T, size_t BitIndex, size_t Bits = 1> 
using bfm16 = bitfield_member<uint16_t, T, BitIndex, Bits>; 
 
template <typename T, size_t BitIndex, size_t Bits = 1> 
using bfm32 = bitfield_member<uint32_t, T, BitIndex, Bits>; 
 
template <typename T, size_t BitIndex, size_t Bits = 1> 
using bfm64 = bitfield_member<uint64_t, T, BitIndex, Bits>; 
 
} // namespace redx 
 
