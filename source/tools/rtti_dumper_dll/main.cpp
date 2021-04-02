#include <Windows.h>
#include <stdio.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <tools/rtti_dumper_dll/rtti.hpp>

static HWND g_hWnd = NULL;
static int g_stdout_fd = -1;
static fpos_t g_stdout_pos;

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
        dumper::dump();
      }
      catch (std::exception& e)
      {
        SPDLOG_DEBUG("dump() threw an exception: {}", e.what());
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

