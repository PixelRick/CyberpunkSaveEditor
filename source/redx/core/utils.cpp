#include <redx/core/utils.hpp>

#include <intrin.h>
#include <cassert>

namespace inl {

FORCE_INLINE void memcpy(void* dst, const void* src, size_t size) 
{
  __movsb(static_cast<unsigned char*>(dst), static_cast<const unsigned char*>(src), size);
}

FORCE_INLINE void memset(void* dst, uint8_t val, size_t size) 
{
  __stosb(static_cast<unsigned char*>(dst), val, size);
}

FORCE_INLINE int memcmp(const void* m1, const void* m2, size_t len)
{
  const uint8_t* pb1 = static_cast<const uint8_t*>(m1);
  const uint8_t* pb2 = static_cast<const uint8_t*>(m2);
  while (len && (*pb1 == *pb2)) { ++pb1; ++pb2; --len; }
  return *pb1 - *pb1;
}

} // namespace inl

namespace redx {

inline uint16_t candidates_lookup(__m128i headblk, __m128i tailblk, const char* headseq, const char* tailseq)
{
  const __m128i head_seq = _mm_loadu_si128(reinterpret_cast<const __m128i*>(headseq));
  const __m128i tail_seq = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tailseq));
  const __m128i eq_head = _mm_cmpeq_epi8(headblk, head_seq);
  const __m128i eq_tail = _mm_cmpeq_epi8(tailblk, tail_seq);
  return _mm_movemask_epi8(_mm_and_si128(eq_head, eq_tail)); // LSB -> first char match result
}

struct needle_chunk
{
  uint32_t mask = { 0 };
  __m128i value {};
};

uintptr_t sse2_strstr(uintptr_t haystack, size_t haystack_size, const char* needle, size_t needle_size)
{
  if (needle_size == 0 || haystack_size < needle_size)
  {
    return sse2_strstr_npos;
  }

  const char* s = (const char*)haystack;

  if (needle_size == 1)
  {
    // fallback to simple loop
    const char* c = s;
    const char* e = s + haystack_size;
    while (c < e)
    {
      if (*c == *needle)
      {
        return s - c;
      }
      c++;
    }
    return sse2_strstr_npos;
  }

  const __m128i headblk = _mm_set1_epi8(needle[0]);
  const __m128i tailblk = _mm_set1_epi8(needle[needle_size - 1]);

  for (size_t pos = 0; pos + needle_size <= haystack_size; pos += 16)
  {
    if (pos + needle_size + 15 > haystack_size) // avoid access violation case
    {
      pos = haystack_size - needle_size - 15;
    }

    const char* pc = s + pos;

    uint16_t bitmask = candidates_lookup(headblk, tailblk, pc, pc + needle_size - 1);
    while (bitmask != 0)
    {
      const auto match_offset = ctz(bitmask);
      if (!inl::memcmp(pc + match_offset + 1, needle + 1, needle_size - 2))
      {
        return pos + match_offset;
      }
      bitmask ^= (1 << match_offset);
    }
  }

  return sse2_strstr_npos;
}

constexpr size_t max_nchunks = 8;

// values > 0xFF are considered wildcards
// mask must not begin nor end with a wildcard value, and cannot be longer than (2 + 16 * max_nchunks)
uintptr_t sse2_strstr_masked(uintptr_t haystack, size_t haystack_size, const wchar_t* masked_needle, size_t needle_size)
{
  if (needle_size == 0 || needle_size > haystack_size || needle_size > (2 + 16 * max_nchunks))
    return sse2_strstr_npos;

  const wchar_t wc_mask = 0xFF00;

  if (masked_needle[0] & wc_mask || masked_needle[needle_size - 1] & wc_mask)
    return sse2_strstr_npos;

  if (needle_size <= 2)
  {
    // fallback to non-masked..
    char tmp[2] {};
    for (size_t i = 0; i < needle_size; ++i)
    {
      tmp[i] = (char)masked_needle[i];
    }
    return sse2_strstr(haystack, haystack_size, tmp, needle_size);
  }

  const char* hs = (const char*)haystack;

  // this algorithm first search for matches of the first and last character
  // using 2 consecutive 16-parallel comparisons 
  // for each match the inner needle range [1:needle_size-2] is compared in chunks of 16

  // prepare the inner range chunks
  needle_chunk inr_chunks[max_nchunks] = {};
  size_t nchunks = ((size_t)needle_size - 2 + 15) / 16;

  char tmp_chunk[16] = { 0 };

  for (size_t i = 0, cnt = needle_size - 2; i < nchunks; ++i, cnt -= 16)
  {
    inl::memset(tmp_chunk, 0, 16);
    const size_t ej = (cnt < 16) ? cnt : 16;
    for (size_t j = 0; j < ej; ++j)
    {
      const wchar_t val = masked_needle[16 * i + j + 1];
      if (0 == (val & wc_mask))
      {
        inr_chunks[i].mask |= 1 << j;
        tmp_chunk[j] = (char)val;
      }
    }
    inr_chunks[i].value = _mm_loadu_si128((const __m128i*)(tmp_chunk));
  }

  const __m128i headblk = _mm_set1_epi8((char)masked_needle[0]);
  const __m128i tailblk = _mm_set1_epi8((char)masked_needle[needle_size - 1]);

  for (size_t pos = 0; pos + needle_size <= haystack_size; pos += 16)
  {
    // avoid access violation case
    // required readable size from pos is needle_size + 15 (mind the tailblk)
    if (pos + needle_size + 15 > haystack_size)
    {
      pos = haystack_size - needle_size - 15;
      // after iteration (pos == haystack_size - needle_size + 1)
      // thus (pos + needle_size == haystack_size + 1), which ends of loop
    }

    const char* pc = hs + pos;

    uint16_t bitmask = candidates_lookup(headblk, tailblk, pc, pc + needle_size - 1);

    while (bitmask != 0)
    {
      const auto match_offset = ctz(bitmask);
      const char* match_in_hs = pc + match_offset;

      bool matched = true;
      for (size_t i = 0; i < nchunks && matched; ++i)
      {
        const char* inr_chunk_in_hs = match_in_hs + 1 + i * 16;
        auto hay_chunk = _mm_loadu_si128((const __m128i*)(inr_chunk_in_hs));
        auto cmpmask = _mm_movemask_epi8(_mm_cmpeq_epi8(hay_chunk, inr_chunks[i].value));
        matched = ((cmpmask & inr_chunks[i].mask) == inr_chunks[i].mask);
      }
      if (matched)
      {
        return pos + match_offset;
      }

      bitmask ^= (1 << match_offset);
    }
  }

  return sse2_strstr_npos;
}

} // namespace redx

