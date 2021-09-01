#include <redx/os/platform_utils.hpp>
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <memory_resource>
#include <chrono>

#include <spdlog/spdlog.h>
#include <redx/core/hashing.hpp>
#include <redx/os/platform_utils.hpp>

namespace fs = std::filesystem;

int wmain(int argc, wchar_t* argv[])
{
  spdlog::set_level(spdlog::level::debug);

  system("PAUSE");

  //---------------------------------------------------------------------------------------

  //std::wstring process_wname = L"notepad++.exe";
  std::wstring process_wname = L"Cyberpunk2077.exe";
  std::wstring dll_wname = L"rtti_dumper_dll.dll";

  fs::path dll_path(dll_wname);
  dll_path = fs::absolute(dll_path);

  if (!fs::exists(dll_path) || !fs::is_regular_file(dll_path))
  {
    SPDLOG_ERROR("dll file not found at {}", dll_path.string());
    return -1;
  }

  HANDLE hToken = NULL;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
  {
    SPDLOG_ERROR("couldn't open current process with (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY): {}", redx::os::last_error_string());
    return -1;
  }

  LUID luidDebug;
  if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luidDebug))
  {
    SPDLOG_ERROR("couldn't lookup the debugging privilege: {}", redx::os::last_error_string());
    return -1;
  }

  TOKEN_PRIVILEGES tokenPriv {};
  tokenPriv.PrivilegeCount = 1;
  tokenPriv.Privileges[0].Luid = luidDebug;
  tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (!AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(tokenPriv), NULL, NULL))
  {
    CloseHandle(hToken);
    SPDLOG_ERROR("couldn't adjust privilege: {}", redx::os::last_error_string());
    return -1;
  }

  CloseHandle(hToken);

  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
  {
    SPDLOG_ERROR("couldn't create a processes snapshot: {}", redx::os::last_error_string());
    return -1;
  }

  PROCESSENTRY32W processEntry{};
  processEntry.dwSize = sizeof(PROCESSENTRY32W);
  if (!Process32FirstW(hSnapshot, &processEntry))
  {
    SPDLOG_ERROR("no process in snapshot");
    CloseHandle(hSnapshot);
    return -1;
  }

  SetLastError(0);

  HANDLE hProcess = 0;
  DWORD dwPID = 0;
  do
  {
    if (_wcsicmp(process_wname.c_str(), processEntry.szExeFile) == 0)
    {
      dwPID = processEntry.th32ProcessID;
      break;
    }

  } while (Process32NextW(hSnapshot, &processEntry));

  CloseHandle(hSnapshot);

  if (dwPID == 0)
  {
    SPDLOG_ERROR("couldn't find target process");
    return -1;
  }

  hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID);
  if (hProcess == 0)
  {
    SPDLOG_ERROR("couldn't open target process: {}", redx::os::last_error_string());
    return -2;
  }

  auto dll_wspath = dll_path.wstring() + L'\0';

  HMODULE hModule = GetModuleHandleA("kernel32.dll");
  LPVOID pLoadLibrary = GetProcAddress(hModule, "LoadLibraryW");
  LPVOID pGetModuleHandle = GetProcAddress(hModule, "GetModuleHandleW");
  LPVOID pFreeLibrary = GetProcAddress(hModule, "FreeLibrary");

  LPVOID lpParam = VirtualAllocEx(hProcess, NULL, dll_wspath.size() * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (lpParam == nullptr)
  {
    
    SPDLOG_ERROR("couldn't allocate memory in remote process: {}", redx::os::last_error_string());
    return -1;
  }

  SIZE_T nWritten = 0;
  if (!WriteProcessMemory(hProcess, lpParam, dll_wspath.data(), dll_wspath.size() * 2, &nWritten))
  {
    SPDLOG_ERROR("couldn't write memory in remote process: {}", redx::os::last_error_string());
    VirtualFreeEx(hProcess, lpParam, 0, MEM_RELEASE);
    return -1;
  }

  HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, static_cast<LPTHREAD_START_ROUTINE>(pLoadLibrary), lpParam, 0, nullptr);
  if (hThread == nullptr)
  {
    SPDLOG_ERROR("couldn't create remote thread: {}", redx::os::last_error_string());
    VirtualFreeEx(hProcess, lpParam, 0, MEM_RELEASE);
    return -1;
  }

  SPDLOG_INFO("remote thread created.");

  WaitForSingleObject(hThread, INFINITE);
  SPDLOG_INFO("remote thread returned.");

  VirtualFreeEx(hProcess, lpParam, 0, MEM_RELEASE);
  CloseHandle(hThread);

  system("PAUSE");

  return 0;
}

