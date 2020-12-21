#include "packing.hpp"

#include <codecvt>

int64_t read_packed_int(std::istream& is)
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
  return sign ? -value : value;
}

void write_packed_int(std::ostream& os, int64_t value)
{
  uint8_t packed[5] = {};
  uint8_t cnt = 1;
  uint64_t tmp = abs(value);
  if (value < 0)
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
  os.write((char*)packed, cnt);
}

std::string read_str(std::istream& is)
{
  // cp's one does directly serialize into a buffer, without conversion
  // also the one used to serialize datum desc name is capped to a 512b buffer
  // BUT it does not seek to compensate for the short read.. nor limit the utf16 read to 256 (but 511 -> 1022bytes)
  // so remember: long strings would probably corrupt a savegame!
  int64_t cnt = read_packed_int(is);
  if (cnt < 0)
  {
    std::string str(-cnt, '\0');
    is.read(str.data(), -cnt);
    return str;
  }
  else
  {
    std::u16string str16(cnt, L'\0');
    is.read((char*)str16.data(), cnt*2);
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(str16);
  }
}

void write_str(std::ostream& os, const std::string& s)
{
  write_packed_int(os, -(int64_t)s.size());
  if (s.size())
    os.write(s.data(), s.size());
}

