#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>
#include <ShlObj.h>
#include <intrin.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include "spdlog/spdlog.h"

#include "utils.hpp"

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

std::string bytes_to_hex(const void* buf, size_t len)
{
  const uint8_t* const u8buf = (uint8_t*)buf;
  std::stringstream ss;
  ss << std::hex << std::uppercase;
  for (size_t i = 0; i < len; ++i)
    ss << std::setfill('0') << std::setw(2) << (uint32_t)u8buf[i];
  return ss.str();
}

std::optional<std::filesystem::path> find_user_saved_games() {
  PWSTR out_ptr{};
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_DEFAULT, nullptr, &out_ptr);
  if (SUCCEEDED(hr)) {
    std::shared_ptr<void> free_guard(out_ptr, CoTaskMemFree);
    auto subdir = std::filesystem::path(out_ptr) / "CD Projekt Red" / "Cyberpunk 2077";
    std::error_code ec;
    if (exists(subdir, ec) && !ec && is_directory(subdir, ec) && !ec) {
      return subdir;
    }
  }
  return {};
}

