#pragma once
#include <inttypes.h>
#include <iostream>
#include <vector>
#include <array>
#include <codecvt>

// because
// os << cbytes_ref<T>(obj) << ...
// looks better than
// os.read((char*)&obj, sizeof(obj)).read(...


template <typename T>
class cbytes_ref
{
  T& ref = 0;

public:
  constexpr explicit cbytes_ref(T& val)
    : ref(val) {}

  cbytes_ref(const cbytes_ref&) = delete;
  cbytes_ref& operator=(const cbytes_ref&) = delete;

  friend std::ostream& operator<<(std::ostream& os, cbytes_ref<T>&& v)
  {
    return os.write((char*)&v.ref, sizeof(T));
  }

  template <typename U = T, std::enable_if_t<std::is_same_v<U, T> && !std::is_const_v<T>, int> = 0>
  friend std::istream& operator>>(std::istream& is, cbytes_ref<T>&& v)
  {
    return is.read((char*)&v.ref, sizeof(T));
  }
};


template <typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, int64_t>, int> = 0>
class cp_packedint_ref
{
  T& ref;

public:
  constexpr explicit cp_packedint_ref(T& val)
    : ref(val) {}

  cp_packedint_ref(const cp_packedint_ref&) = delete;
  cp_packedint_ref& operator=(const cp_packedint_ref&) = delete;

  friend std::ostream& operator<<(std::ostream& os, cp_packedint_ref&& v)
  {
    std::array<uint8_t, 5> packed {};
    uint8_t cnt = 1;
    uint64_t tmp = std::abs(v.ref);
    if (v.ref < 0)
      packed[0] |= 0x80;
    packed[0] |= tmp & 0x3F;
    tmp >>= 6;
    if (tmp != 0) {
      packed[0] |= 0x40; cnt++;
      packed[1] = tmp & 0x7F;
      tmp >>= 7;
      if (tmp != 0) {
        packed[1] |= 0x80; cnt++;
        packed[2] = tmp & 0x7F;
        tmp >>= 7;
        if (tmp != 0) {
          packed[2] |= 0x80; cnt++;
          packed[3] = tmp & 0x7F;
          tmp >>= 7;
          if (tmp != 0) {
            packed[3] |= 0x80; cnt++;
            packed[4] = tmp & 0x7F;
          }
        }
      }
    }
    return os.write((char*)packed.data(), cnt);
  }

  //template <typename U = T, std::enable_if_t<std::is_same_v<U, T> && !std::is_const_v<T>, int> = 0>
  friend std::istream& operator>>(std::istream& is, cp_packedint_ref&& v)
  {
    char a = 0;
    is.read(&a, 1);
    int64_t value = a & 0x3F;
    const bool sign = !!(a & 0x80);
    if (a & 0x40) {
      is.read(&a, 1);
      value |= (a & 0x7F) << 6;
      if (a < 0) {
        is.read(&a, 1);
        value |= (a & 0x7F) << 13;
        if (a < 0) {
          is.read(&a, 1);
          value |= (a & 0x7F) << 20;
          if (a < 0) {
            is.read(&a, 1);
            value |= (a & 0xFF) << 27;
          }
        }
      }
    }
    v.ref = sign ? -value : value;
    return is;
  }
};


template <typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, std::string>, int> = 0>
class cp_plstring_ref
{
  T& ref;

public:
  constexpr explicit cp_plstring_ref(T& val)
    : ref(val) {}

  cp_plstring_ref(const cp_plstring_ref&) = delete;
  cp_plstring_ref& operator=(const cp_plstring_ref&) = delete;

  friend std::ostream& operator<<(std::ostream& os, cp_plstring_ref&& v)
  {
    const auto& s = v.ref;
    int64_t sz = -(int64_t)s.size();
    os << cp_packedint_ref(sz);
    if (s.size())
      os.write(s.data(), s.size());
    return os;
  }

  //template <typename U = T, std::enable_if_t<std::is_same_v<U, T> && !std::is_const_v<T>, int> = 0>
  friend std::istream& operator>>(std::istream& is, cp_plstring_ref&& v)
  {
    // cp's one does directly serialize into a buffer, without conversion
    // also the one used to serialize datum desc name is capped to a 512b buffer
    // BUT it does not seek to compensate for the short read.. nor limit the utf16 read to 256 (but 511 -> 1022bytes)
    // so remember: long strings would probably corrupt a savegame!
    int64_t cnt = 0;
    is >> cp_packedint_ref(cnt);
    if (cnt < 0)
    {
      std::string str(-cnt, '\0');
      is.read(str.data(), -cnt);
      v.ref = str;
    }
    else
    {
      std::u16string str16(cnt, L'\0');
      is.read((char*)str16.data(), cnt*2);
      std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
      v.ref = convert.to_bytes(str16);
    }
    return is;
  }
};

