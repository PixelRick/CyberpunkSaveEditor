#pragma once
#include <stdint.h>

struct namehash
{
  uint32_t crc;
  uint8_t slen;
  uint8_t uk0[2];
};

struct chunk_desc
{
  uint32_t offset;
  uint32_t size;
  uint32_t real_offset;
  uint32_t real_size;
};

struct node_desc
{
  namehash namehash;
  int32_t next_idx;
  int32_t child_idx;
  uint32_t data_offset;
  uint32_t data_size;
};


struct __declspec(align(8)) stream_base
{
  uint64_t vtbl;
  uint32_t flags;
  uint32_t dwordC;
  uint64_t field_10;
  uint64_t field_18;
  uint64_t pstream;
};

struct csav
{
  stream_base ar1;
  stream_base ar2;
};