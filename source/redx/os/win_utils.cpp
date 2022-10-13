#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <Windows.h>

#include <optional>
#include <filesystem>

#include <redx/os/platform_utils.hpp>
#include <spdlog/spdlog.h>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

namespace redx::os {

std::optional<std::filesystem::path> find_exe_path(std::string_view exename) {
  // Computer\HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Compatibility Assistant\Store
  // Computer\HKEY_CURRENT_USER\SOFTWARE\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache

  HKEY hKey;

  if (RegOpenKeyExA(
    HKEY_CURRENT_USER,
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store",
    0, KEY_READ, &hKey)
    == ERROR_SUCCESS)
  {
    WCHAR ValueName[MAX_VALUE_NAME];
    DWORD cchValueName = MAX_VALUE_NAME;

    DWORD i = 0;
    while (1)
    {
      cchValueName = MAX_VALUE_NAME;
      ValueName[0] = '\0';

      if (RegEnumValueW(
        hKey, i++, ValueName, &cchValueName, NULL, NULL, NULL, NULL)
        != ERROR_SUCCESS)
      {
        RegCloseKey(hKey);
        break;
      }

      std::filesystem::path p(ValueName);

      if (p.filename() == exename && std::filesystem::exists(p))
      {
        RegCloseKey(hKey);
        return p;
      }
    }
  }

  if (RegOpenKeyExA(
    HKEY_CURRENT_USER,
    "SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
    0, KEY_READ, &hKey)
    == ERROR_SUCCESS)
  {
    WCHAR ValueName[MAX_VALUE_NAME];
    DWORD cchValueName = MAX_VALUE_NAME;

    DWORD i = 0;
    while (1)
    {
      cchValueName = MAX_VALUE_NAME;
      ValueName[0] = '\0';

      if (RegEnumValueW(hKey, i++, ValueName, &cchValueName, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
      {
        RegCloseKey(hKey);
        break;
      }
      std::filesystem::path p(ValueName);

      // Keys here are in format: Cyberpunk2077.exe.XXX
      // -Cyberpunk2077.exe.ApplicationCompany
      // -Cyberpunk2077.exe.FriendlyAppName
      if (p.filename().string().find(exename) != std::string::npos)
      {
        p.remove_filename().append(exename);
        if (std::filesystem::exists(p))
        {
          RegCloseKey(hKey);
          return p;
        }
      }
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> get_cp_executable_path()
{
  static std::optional<std::filesystem::path> s_path = find_exe_path("Cyberpunk2077.exe");
  return s_path;
}

std::string last_error_string()
{
  return format_error(GetLastError());
}

std::string format_error(error_type id)
{
  if (id != 0)
  {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
    auto ret = fmt::format("{} (0x{:X})", std::string_view(messageBuffer, size), id);

    LocalFree(messageBuffer);

    return ret;
  }

  return "";
}

} // namespace redx::os



