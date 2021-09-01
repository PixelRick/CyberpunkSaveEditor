#include <redx/io/bstream.hpp>

//#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
//#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
//#endif
//#include <codecvt>

namespace redx {

ibstream& ibstream::read_str_lpfxd(std::string& s)
{
  int64_t cnt = read_int64_packed();
  if (0 > cnt && cnt > -0x1000)
  {
    const size_t len = static_cast<size_t>(-cnt);
    s.resize(len, '\0');
    read_bytes(s.data(), len);
  }
  else if (0 < cnt && cnt < 0x1000)
  {
    const size_t len = static_cast<size_t>(cnt);
    std::u16string str16(len, L'\0');
    read_bytes(reinterpret_cast<char*>(str16.data()), len * 2);
    // conversion disabled
    s.resize(len * 2);
    std::transform(str16.begin(), str16.end(), s.begin(),
      [](char16_t wc) -> char { return static_cast<char>(wc); });

    //std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    //s = convert.to_bytes(str16);
  }
  else if (cnt != 0)
  {
    STREAM_LOG_AND_SET_ERROR(*this, "read lpfxd string size is unexpectedly long");
  }
  else
  {
    s = "";
  }

  if (fail())
  {
    s = "";
  }

  return *this;
}

obstream& obstream::write_str_lpfxd(const std::string& s)
{
  const size_t len = s.size();
  const int64_t cnt = -reliable_numeric_cast<int64_t>(len);

  write_int64_packed(cnt);
  if (len)
  {
    write_bytes(s.data(), len);
  }

  return *this;
}

int64_t ibstream::read_int64_packed()
{
  uint8_t a = read<uint8_t>();
  int64_t value = a & 0x3F;
  const bool sign = !!(a & 0x80);
  if (a & 0x40)
  {
    a = read<uint8_t>();
    value |= static_cast<uint64_t>(a & 0x7F) << 6;
    if (a < 0)
    {
      a = read<uint8_t>();
      value |= static_cast<uint64_t>(a & 0x7F) << 13;
      if (a < 0)
      {
        a = read<uint8_t>();
        value |= static_cast<uint64_t>(a & 0x7F) << 20;
        if (a < 0)
        {
          a = read<uint8_t>();
          value |= static_cast<uint64_t>(a & 0xFF) << 27;
        }
      }
    }
  }

  if (fail())
  {
    return 0;
  }

  return sign ? -value : value;
}

obstream& obstream::write_int64_packed(int64_t v)
{
  std::array<uint8_t, 5> packed {};
  uint8_t cnt = 1;
  uint64_t tmp = std::abs(v);

  if (v < 0)
  {
    packed[0] |= 0x80;
  }

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

  write_bytes((char*)packed.data(), cnt);

  return *this;
}

} // namespace redx

