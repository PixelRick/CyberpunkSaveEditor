#pragma once
#include <redx/core.hpp>
#include <filesystem>

namespace redx::oodle {

enum class compression_level : uint64_t
{
  none = 0,
  fast3,
  fast2,
  fast1,
  normal,
  best1,
  best2,
  best3,
  best4,
  best5,
};

struct library
{
  static library& get()
  {
    static library s;
    return s;
  }

  library(const library&) = delete;
  library& operator=(const library&) = delete;

  bool is_available() const
  {
    return m_handle != nullptr;
  }

  static constexpr size_t OODLELZ_BLOCK_LEN = 0x40000;

  enum class OodleLZ_Compressor : uint64_t
  {
    LZH = 0,
    LZHLW,
    LZNIB,
    None,
    LZB16,
    LZBLW,
    LZA,
    LZNA,
    Kraken,
    Mermaid,
    BitKnit,
    Selkie,
    Akkorokamui,
  };
  
  enum class OodleLZ_FuzzSafe : uint64_t
  {
    No = 0,
    Yes,
  };
  
  enum class OodleLZ_Decode_Thread : uint64_t
  {
    Default = 0,
    Phase1  = 1,
    Phase2  = 2,
    Current = 3,
  };

  //bool try_load_from_dir(const std::filesystem::path& p);

  size_t (*
  pfn_OodleLZ_Decompress)(
    const void* src, size_t src_size,
    const void* dst, size_t dst_size,
    OodleLZ_FuzzSafe fuzz_safe,
    bool check_crc,
    uint32_t log_level,
    uint64_t a8,
    uint64_t a9,
    uint64_t a10,
    uint64_t a11,
    const void* decoder_mem,
    size_t decoder_mem_size,
    OodleLZ_Decode_Thread thread
  ) = nullptr;

  size_t (*
  pfn_OodleLZ_Compress)(
    OodleLZ_Compressor compressor,
    const void* src, size_t src_size,
    const void* dst,
    compression_level level,
    uint64_t a6,
    uint64_t a7,
    uint64_t a8,
    const void* encoder_mem,
    size_t encoder_mem_size
  ) = nullptr;

  size_t (*
  pfn_OodleLZ_GetCompressedBufferSizeNeeded)(
    size_t src_size
  ) = nullptr;

  // OodlePlugins_SetAllocators(NULL,NULL);
  // OodlePlugins_SetAssertion(NULL);
  // OodlePlugins_SetPrintf(NULL);

protected:

  library();
  ~library();

  bool m_is_loaded = false;
  void* m_handle;
};

inline bool is_available()
{
  return library::get().is_available();
}

bool decompress(std::span<const char> src, std::span<char> dst, bool check_crc);
size_t compress(std::span<const char> src, std::span<char> dst, compression_level level = compression_level::normal);

} // namespace redx::oodle

