// windows-only
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>

#include <redx/io/oodle/oodle.hpp>
#include <redx/os/platform_utils.hpp>

#define LIBNAME "oo2ext_7_win64.dll"

namespace redx::oodle {

struct proc_pointer
{
  explicit proc_pointer(FARPROC ptr)
    : ptr(ptr) {}

  template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
  operator T*() const
  {
    return reinterpret_cast<T *>(ptr);
  }

  FARPROC ptr;
};

proc_pointer get_proc_address(HMODULE module, LPCSTR proc_name)
{
  return proc_pointer(GetProcAddress(module, proc_name));
}

library::library()
{
  // todo: could use global settins and try to load it from different locations, e.g. game folder
  HMODULE handle = LoadLibraryA(LIBNAME);

  if (!handle)
  {
    auto game_path_opt = redx::os::get_cp_executable_path();
    if (game_path_opt.has_value())
    {
      auto dll_path = game_path_opt.value().replace_filename(LIBNAME);
      handle = LoadLibraryW(dll_path.c_str());
    }
  }

  if (handle)
  {
    pfn_OodleLZ_Decompress = get_proc_address(handle, "OodleLZ_Decompress");
    pfn_OodleLZ_Compress = get_proc_address(handle, "OodleLZ_Compress");
    pfn_OodleLZ_GetCompressedBufferSizeNeeded = get_proc_address(handle, "OodleLZ_GetCompressedBufferSizeNeeded");
  }

  m_handle = (void*)handle;
}

library::~library()
{
  if (m_handle != nullptr)
  {
    FreeLibrary((HMODULE)m_handle);
  }
}


struct header
{
  uint32_t magic = 'KRAK';
  uint32_t size = 0;

  bool is_magic_ok() const
  {
    return magic == 'KRAK';
  }
};


bool decompress(std::span<const char> src, std::span<char> dst, bool check_crc)
{
  const header& hdr = *reinterpret_cast<const header*>(src.data());

  if (!hdr.is_magic_ok())
  {
    SPDLOG_ERROR("wrong magic");
    return false;
  }

  if (hdr.size != dst.size())
  {
    SPDLOG_ERROR("dst_size doesn't match uncompressed size");
    return false;
  }

  return decompress_noheader(src.subspan(sizeof(header)), dst, check_crc);
}

bool decompress_noheader(std::span<const char> src, std::span<char> dst, bool check_crc)
{
  auto& lib = library::get();

  if (lib.pfn_OodleLZ_Decompress == nullptr)
  {
    SPDLOG_ERROR("OodleLZ_Decompress isn't available; either the oodle library is not present or versions mistmatch.");
    return false;
  }

  const size_t decoder_mem_size = library::OODLELZ_BLOCK_LEN * 2;
  char* decoder_mem = nullptr;
  bool decoder_mem_is_on_stack = true;

  __try
  {
    decoder_mem = reinterpret_cast<char*>(_malloca(decoder_mem_size));
  }
  __except(GetExceptionCode() == STATUS_STACK_OVERFLOW)
  {
    SPDLOG_WARN("_malloca failed, switching to malloc");
    decoder_mem_is_on_stack = false;
    decoder_mem = reinterpret_cast<char*>(malloc(decoder_mem_size));
  };

  //std::array<char, library::OODLELZ_BLOCK_LEN * 2> decoder_mem;

  size_t decompressed = lib.pfn_OodleLZ_Decompress(
    src.data(), src.size(), dst.data(), dst.size(),
    library::OodleLZ_FuzzSafe::Yes,
    check_crc, 0, 0, 0, 0, 0,
    decoder_mem, decoder_mem_size,
    library::OodleLZ_Decode_Thread::Current);

  if (decoder_mem_is_on_stack)
  {
    _freea(decoder_mem);
  }
  else
  {
    free(decoder_mem);
  }

  if (dst.size() != decompressed)
  {
    SPDLOG_ERROR("decompressed size doesn't match header info");
    return false;
  }

  return true;
}

bool check_lib_is_ready_to_compress(library& lib)
{
  if (lib.pfn_OodleLZ_Compress == nullptr)
  {
    SPDLOG_ERROR("OodleLZ_Compress isn't available; either the oodle library is not present or versions mistmatch.");
    return false;
  }

  if (lib.pfn_OodleLZ_GetCompressedBufferSizeNeeded == nullptr)
  {
    SPDLOG_ERROR("OodleLZ_GetCompressedBufferSizeNeeded isn't available; either the oodle library is not present or versions mistmatch.");
    return false;
  }

  return true;
}

// returns compressed size
inline size_t compress_to(library& lib, std::span<const char> src, std::span<char> dst, compression_level level)
{
  return lib.pfn_OodleLZ_Compress(
    library::OodleLZ_Compressor::Kraken,
    src.data(), src.size(), dst.data(),
    level, 0, 0, 0,
    nullptr, 0);
}

// returns an empty buffer on error or worthless compression
std::vector<char> compress_with_blank_header(std::span<const char> src, size_t header_size, compression_level level)
{
  auto& lib = library::get();

  if (!check_lib_is_ready_to_compress(lib))
  {
    return {};
  }

  if (src.size() < 0x100)
  {
    return {};
  }

  const size_t needed_size = lib.pfn_OodleLZ_GetCompressedBufferSizeNeeded(src.size());
  std::vector<char> buf(header_size + needed_size);

  const size_t compressed = compress_to(
    lib, src, std::span<char>(buf.data() + header_size, buf.size() - header_size), level);

  if (compressed == 0 || compressed + header_size >= src.size())
  {
    return {};
  }

  return buf;
}

std::vector<char> compress(std::span<const char> src, compression_level level)
{
  std::vector<char> buf = compress_with_blank_header(src, sizeof(header), level);

  if (!buf.empty())
  {
    header& hdr = *reinterpret_cast<header*>(buf.data());
    hdr = header(); // sets magic
    hdr.size = numeric_cast<uint32_t>(src.size());
  }

  return buf;
}

std::vector<char> compress_noheader(std::span<const char> src, compression_level level)
{
  return compress_with_blank_header(src, 0, level);;
}



} // namespace redx::oodle

