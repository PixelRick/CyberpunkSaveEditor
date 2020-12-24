#include "utils.h"

#include <windows.h>
#include <intrin.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

void replace_all_in_str(std::string& s, const std::string& from, const std::string& to)
{
  size_t start_pos = 0;
  while((start_pos = s.find(from, start_pos)) != std::string::npos) {
    s.replace(start_pos, from.length(), to);
    start_pos += to.length(); // handles case where 'to' is a substring of 'from'
  }
}

std::string u64_to_cpp(uint64_t val)
{
  std::stringstream ss;
  ss << "0x" << std::setfill('0') << std::setw(16) 
    << std::hex << std::uppercase << val;
  return ss.str();
}

template <typename T, std::enable_if_t<sizeof(T) <= 4, int> = 0>
inline unsigned ctz(const T value)
{
  assert(value != 0);
  DWORD trailing_zero = 0;
  return _BitScanForward(&trailing_zero, value) ? trailing_zero : sizeof(T) * 8;
}

inline uint16_t candidates_lookup(__m128i headblk, __m128i tailblk, const unsigned char* headseq, const unsigned char* tailseq)
{
  const __m128i head_seq = _mm_loadu_si128(reinterpret_cast<const __m128i*>(headseq));
  const __m128i tail_seq = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tailseq));
  const __m128i eq_head = _mm_cmpeq_epi8(headblk, head_seq);
  const __m128i eq_tail = _mm_cmpeq_epi8(tailblk, tail_seq);
  return _mm_movemask_epi8(_mm_and_si128(eq_head, eq_tail)); // LSB -> first char match result
}

struct needle_chunk {
  uint32_t mask = { 0 };
  __m128i value;
};

std::vector<uintptr_t> sse2_strstr_masked(const unsigned char* s, size_t m, const unsigned char* needle, size_t n, const char* mask, size_t maxcnt)
{
  assert(n > 2);
  assert(m >= n);
  assert(strlen(mask) == n);
  assert(mask[0] == 'x');
  assert(mask[n - 1] == 'x');

  std::vector<uintptr_t> ret;

  needle_chunk needle_chunks[4] = {};
  // we pre-check first and last character so the final cmp is on range [1:n-2]
  int nchunks = ((int)n - 2 + 15) / 16;
  assert(nchunks <= 4);
  char needle_copy[16 * 4] = { 0 }; // to avoid access violation by _mm_loadu_si128
  memcpy(needle_copy, needle, n);
  for (int i = 0, rem = (int)n - 2; i < nchunks; ++i, rem -= 16)
  {
    const int ej = min(rem, 16);
    for (size_t j = 0; j < ej; ++j) {
      if (mask[16 * i + j + 1] == 'x')
        needle_chunks[i].mask |= 1 << j;
    }
    needle_chunks[i].value = _mm_loadu_si128((const __m128i*)(needle_copy + 1 + i * 16));
  }

  const __m128i headblk = _mm_set1_epi8((char)needle[0]);
  const __m128i tailblk = _mm_set1_epi8((char)needle[n - 1]);

  for (size_t pos = 0; pos + n <= m; pos += 16)
  {
    if (pos + n + 15 > m) // avoid access violation case
      pos = m - n - 15;

    const BYTE* pc = s + pos;

    uint16_t bitmask = candidates_lookup(headblk, tailblk, pc, pc + n - 1);
    while (bitmask != 0)
    {
      const auto match_offset = ctz(bitmask);
      bool match = true;
      for (int j = 0; j < nchunks && match; ++j)
      {
        auto hay = _mm_loadu_si128((const __m128i*)(pc + match_offset + 1 + j * 16));
        auto cmpmask = _mm_movemask_epi8(_mm_cmpeq_epi8(hay, needle_chunks[j].value));
        match = ((cmpmask & needle_chunks[j].mask) == needle_chunks[j].mask);
      }
      if (match) {
        ret.push_back(pos + match_offset);
        if (maxcnt && ret.size() == maxcnt)
          return ret;
      }
      bitmask ^= (1 << match_offset);
    }
  }

  return ret;
}

std::vector<uintptr_t> sse2_strstr(const unsigned char* s, size_t m, const unsigned char* needle, size_t n, size_t maxcnt)
{
  if (n == 0 || m < n)
    return {};

  std::vector<uintptr_t> ret;

  if (n < 2) {
    // fallback to default impl
    const unsigned char* c = s;
    const unsigned char* e = s + m;
    while (c < e)
      if (*c == *needle)
        ret.push_back(s - c);
    return ret;
  }

  const __m128i headblk = _mm_set1_epi8((char)needle[0]);
  const __m128i tailblk = _mm_set1_epi8((char)needle[n - 1]);

  for (size_t pos = 0; pos + n <= m; pos += 16)
  {
    if (pos + n + 15 > m) // avoid access violation case
      pos = m - n - 15;

    const BYTE* pc = s + pos;

    uint16_t bitmask = candidates_lookup(headblk, tailblk, pc, pc + n - 1);
    while (bitmask != 0)
    {
      const auto match_offset = ctz(bitmask);
      if (!memcmp(pc + match_offset + 1, needle + 1, n - 2)) {
        ret.push_back(pos + match_offset);
        if (maxcnt && ret.size() == maxcnt)
          return ret;
      }
      bitmask ^= (1 << match_offset);
    }
  }

  return ret;
}

