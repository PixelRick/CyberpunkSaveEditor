
#include <Windows.h>
#include <stdio.h>

#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <tools/rtti_dumper_dll/rtti.hpp>
#include <tools/rtti_dumper_dll/pe.hpp>

static HWND g_hWnd = NULL;
static int g_stdout_fd = -1;
static fpos_t g_stdout_pos;

uintptr_t hook_addr = 0;
char saved_bytes[20];
std::mutex abool;

void do_dump()
{
  if (!abool.try_lock())
  {
    abool.lock();
    abool.unlock();
    return;
  }
  memcpy((void*)hook_addr, saved_bytes, 20);
  try
  {
    dumper::dump();
  }
  catch (std::exception& e)
  {
    std::ignore = e;
    SPDLOG_DEBUG("dump() threw an exception: {}", e.what());
  }
  abool.unlock();
}

bool WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
  switch (fdwReason)
  {
    case DLL_PROCESS_ATTACH:
    {
      AllocConsole();
      SetConsoleCtrlHandler(nullptr, true);
      g_hWnd = GetConsoleWindow();

      //if (g_hWnd != NULL)
      //{
      //   HMENU hMenu = GetSystemMenu(g_hWnd, FALSE);
      //   if (hMenu != NULL) DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
      //}

      fgetpos(stdout, &g_stdout_pos);
      g_stdout_fd = _dup(_fileno(stdout));

      FILE* stream;
      if (::freopen_s(&stream, "CONOUT$", "a+", stdout))
      { // error
        g_stdout_fd = -1;
      }

      spdlog::set_level(spdlog::level::debug);

      SPDLOG_DEBUG("hello!");

      try
      {
        auto matches = dumper::find_pattern_in_game_text(L"\x48\x83\xEC\xF00\x65\x48\x8B\x04\x25\xF00\xF00\x00\x00\xBA\xF00\xF00\xF00\xF00\x48\x8B\x08\x8B\x04\x0A\x39\x05\xF00\xF00\xF00\xF00\x0F\x8F");
        if (matches.size() != 5)
        {
          SPDLOG_CRITICAL("couldn't find CClass::GetDefaultInstance pattern");
        }
        else
        {
          hook_addr = matches[1];
          DWORD old;
          if (!VirtualProtect((void*)hook_addr, 0x100, PAGE_EXECUTE_READWRITE, &old))
          {
            SPDLOG_CRITICAL("couldn't reprot for hook");
          }
          else
          {
            memcpy(saved_bytes, (void*)hook_addr, 20);

            uintptr_t op = 0x25FF;
            uintptr_t target = (uintptr_t)&do_dump;

            *(uintptr_t*)hook_addr = op;
            *(uintptr_t*)((char*)hook_addr + 6) = target;
          }
        }
      }
      catch (std::exception& e)
      {
        std::ignore = e;
        SPDLOG_DEBUG("hook failed", e.what());
      }

      //return FreeLibrary(hinstDLL); can work with an external C3
      break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
    {
      if (g_stdout_fd != -1)
      {
        fflush(stdout);
        _dup2(g_stdout_fd, _fileno(stdout));
        _close(g_stdout_fd);
        clearerr(stdout);
        fsetpos(stdout, &g_stdout_pos);
      }

      break;
    }
  }
  return true;
}

