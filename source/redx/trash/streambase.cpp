#include <redx/core/streambase.hpp>

#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#include <codecvt>

namespace redx {

streambase& streambase::serialize_str_lpfxd(std::string& s)
{
  if (has_error())
  {
    return *this;
  }

  if (is_reader())
  {
    // cp's one does directly serialize into a buffer, without conversion
    // also the one used to serialize datum desc gname is capped to a 512b buffer
    // BUT it does not seek to compensate for the short read.. nor limit the utf16 read to 256 (but 511 -> 1022bytes)
    // so remember: long strings would probably corrupt some files!
    int64_t cnt = read_int_packed();
    if (cnt < 0)
    {
      const size_t len = static_cast<size_t>(-cnt);
      s.resize(len, '\0');
      serialize_bytes(s.data(), len);
    }
    else
    {
      const size_t len = static_cast<size_t>(cnt);
      std::u16string str16(len, L'\0');
      serialize_bytes(str16.data(), len * 2);
      std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
      s = convert.to_bytes(str16);
    }
  }
  else
  {
    const size_t len = s.size();
    const int64_t cnt = -static_cast<int64_t>(len);
    write_int_packed(cnt);
    if (len)
      serialize_bytes(s.data(), len);
  }

  return *this;
}

int64_t streambase::read_int_packed()
{
  uint8_t a = 0;
  serialize_byte(&a);
  int64_t value = a & 0x3F;
  const bool sign = !!(a & 0x80);
  if (a & 0x40)
  {
    serialize_byte(&a);
    value |= static_cast<uint64_t>(a & 0x7F) << 6;
    if (a < 0)
    {
      serialize_byte(&a);
      value |= static_cast<uint64_t>(a & 0x7F) << 13;
      if (a < 0)
      {
        serialize_byte(&a);
        value |= static_cast<uint64_t>(a & 0x7F) << 20;
        if (a < 0)
        {
          serialize_byte(&a);
          value |= static_cast<uint64_t>(a & 0xFF) << 27;
        }
      }
    }
  }
  return sign ? -value : value;
}

void streambase::write_int_packed(int64_t v)
{
  std::array<uint8_t, 5> packed {};
  uint8_t cnt = 1;
  uint64_t tmp = std::abs(v);
  if (v < 0)
    packed[0] |= 0x80;
  packed[0] |= tmp & 0x3F;
  tmp >>= 6;
  if (tmp != 0)
  {
    cnt++;
    packed[0] |= 0x40;
    packed[1] = tmp & 0x7F;
    tmp >>= 7;
    if (tmp != 0)
    {
      cnt++;
      packed[1] |= 0x80;
      packed[2] = tmp & 0x7F;
      tmp >>= 7;
      if (tmp != 0)
      {
        cnt++;
        packed[2] |= 0x80;
        packed[3] = tmp & 0x7F;
        tmp >>= 7;
        if (tmp != 0)
        {
          cnt++;
          packed[3] |= 0x80;
          packed[4] = tmp & 0xFF;
        }
      }
    }
  }
  serialize_bytes((char*)packed.data(), cnt);
}

} // namespace redx

