#include <cpinternals/oodle/oodle.hpp>

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>

#include <cpinternals/common/windowz.hpp>

#define LIBNAME "oo2ext_7_win64.dll"

namespace cp::oodle {

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
    auto game_path_opt = cp::windowz::get_cp_executable_path();
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
  auto& lib = library::get();

  if (lib.pfn_OodleLZ_Decompress == nullptr)
  {
    SPDLOG_ERROR("OodleLZ_Decompress isn't available; either the oodle library is not present or versions mistmatch.");
    return false;
  }

  const header& hdr = *reinterpret_cast<const header*>(src.data());
  const size_t hdr_size = sizeof(header);

  if (!hdr.is_magic_ok())
  {
    SPDLOG_ERROR("oodle::decompress: wrong magic");
    return false;
  }

  if (hdr.size != dst.size())
  {
    SPDLOG_ERROR("oodle::decompress: dst_size doesn't match uncompressed size");
    return false;
  }

  std::array<char, library::OODLELZ_BLOCK_LEN * 2> decoder_mem;
  
  size_t decompressed = lib.pfn_OodleLZ_Decompress(
    src.data() + hdr_size, src.size() - hdr_size, dst.data(), dst.size(),
    library::OodleLZ_FuzzSafe::Yes,
    check_crc, 0, 0, 0, 0, 0,
    decoder_mem.data(), decoder_mem.size(),
    library::OodleLZ_Decode_Thread::Current);

  return true;
}

size_t compress(std::span<const char> src, std::span<char> dst, compression_level level)
{
  auto& lib = library::get();

  SPDLOG_ERROR("oodle::compress: not implemented yet");

  return false;
}

} // namespace cp::oodle

