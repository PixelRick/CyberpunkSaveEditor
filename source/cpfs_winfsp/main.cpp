#define WIN32_NO_STATUS
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

#include <winfsp/winfsp.h>
#include <cstdio>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/dist_sink.h>



#include <cpfs_winfsp/resource.h>
#include <cpfs_winfsp/cpfs.hpp>


void debug_symlink(const std::filesystem::path& p);


#define CONSOLE_LOG

constexpr auto WNDCLS_NAME = L"cpfs_systray";
static constexpr UINT WMAPP_SYSTRAYCB = (WM_APP + 100);

static HINSTANCE s_hInst = NULL;
static HWND s_hAboutDlg = NULL;
static HICON s_hIcon = NULL;
static HMENU s_hCtxMenu = NULL;

LRESULT WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL AddNotificationIcon(HWND hWnd);
BOOL DeleteNotificationIcon(HWND hWnd);

BOOL ShowAboutDlg(HWND hWnd);
VOID ShowContextMenu(HWND hWnd, POINT pt);

std::shared_ptr<spdlog::sinks::dist_sink_mt> s_console_sink_wrapper;
spdlog::sink_ptr s_console_sink;

void ShowConsole()
{
  if (!s_console_sink)
  {
    AllocConsole();
    SetConsoleCtrlHandler(nullptr, true);

    // remove close button
    HWND hWnd = ::GetConsoleWindow();
    if (hWnd != NULL)
    {
      HMENU hMenu = ::GetSystemMenu(hWnd, FALSE);
      if (hMenu != NULL)
      {
        DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
      }
    }

    s_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    s_console_sink_wrapper->add_sink(s_console_sink);
  }
}

void HideConsole()
{
  if (s_console_sink)
  {
    FreeConsole();

    s_console_sink_wrapper->remove_sink(s_console_sink);
    s_console_sink.reset();
  }
}

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nShowCmd)
{
  s_hInst = hInstance;

  //----------------------------------------

  s_hIcon = LoadIconW(s_hInst, MAKEINTRESOURCE(IDI_ICON1));

  WNDCLASSEXW wcex = {sizeof(wcex)};
  wcex.lpfnWndProc    = WndProc;
  wcex.hInstance      = s_hInst;
  wcex.lpszClassName  = WNDCLS_NAME;
  RegisterClassExW(&wcex);

  HWND hwnd = CreateWindowExW(0, WNDCLS_NAME, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  UpdateWindow(hwnd);

  //----------------------------------------

  s_console_sink_wrapper = std::make_shared<spdlog::sinks::dist_sink_mt>();

  std::vector<spdlog::sink_ptr> spdlog_sinks;
  spdlog_sinks.emplace_back(s_console_sink_wrapper);
  // todo: add rotating file logger..
  auto combined_logger = std::make_shared<spdlog::logger>("cpfs_logger", std::begin(spdlog_sinks), std::end(spdlog_sinks));
  
  spdlog::set_default_logger(combined_logger);
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%x %X.%e] [%^-%L-%$] [tid:%t] [%s:%#] %!: %v");

#ifdef CONSOLE_LOG

  ShowConsole();

#endif

  // 
  //ShowAboutBubble();

  cpfs cpfs;

  SPDLOG_INFO("initializing cpfs");
  cpfs.init(-1);

  SPDLOG_INFO("loading archives");
  cpfs.load_archives();

  SPDLOG_INFO("starting cpfs");
  cpfs.start();

  if (1)
  {
    std::filesystem::path test_path(cpfs.disk_letter);
    test_path /= "\\D251917154DF532F";

    SPDLOG_INFO("symlink test..");

    std::filesystem::directory_entry dirent(test_path);
    if (dirent.is_symlink())
    {
      SPDLOG_INFO("symlink {} resolved to: {}", test_path.string(), std::filesystem::read_symlink(test_path).string());
    }
    else
    {
      SPDLOG_INFO("{} wasn't recognized as a symlink (type:{})", test_path.string(), dirent.status().type());
    }
  }

  if (0)
  {
    SPDLOG_INFO("symlink testing #2");
    debug_symlink("Z:\\0af6d3d6f362bf06");
    debug_symlink("D:\\Desktop\\cpsavedit\\BUF_FILES\\big_folder_test2\\sdsd");
  }


  MSG msg;
  bool running = true;
  while (running)
  {
    if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        running = false;
    }

    if (running)
    {
    }
  }

  


  //if (0)
  //{
  //  redx::filesystem::recursive_directory_iterator it(cpfs.tfs, "base\\worlds\\03_night_city\\sectors\\_generated\\global_illumination");
  //  SPDLOG_INFO("start");
  //  size_t i = 0;
  //  for (const auto& dirent: it)
  //  {
  //    SPDLOG_INFO("[{:04d}][{}] {}", i++,
  //      (dirent.is_reserved_for_file() ? "R" : (dirent.is_file() ? "F" : "D")),
  //      dirent.tfs_path().strv());
  //
  //    if (dirent.is_reserved_for_file())
  //    {
  //      SPDLOG_INFO("IS_RESERVED_FOR_FILE");
  //      redx::filesystem::directory_entry d(cpfs.tfs, dirent.tfs_path());
  //      SPDLOG_INFO("resolved: {}", d.tfs_path().strv());
  //    }
  //
  //    if (i > 500) break;
  //  }
  //  SPDLOG_INFO("end");
  //}

  

  
  
}

BOOL InitCtxMenu()
{
  HMENU hMenu = LoadMenuW(s_hInst, MAKEINTRESOURCE(IDR_MENU1));
  if (hMenu)
  {
    s_hCtxMenu = GetSubMenu(hMenu, 0);
    RemoveMenu(hMenu, 0, MF_BYPOSITION);
    DestroyMenu(hMenu);
    return TRUE;
  }
  return FALSE;
}

BOOL AddNotificationIcon(HWND hWnd)
{
  NOTIFYICONDATAW nid = {sizeof(nid)};
  nid.hWnd = hWnd;
  nid.uID = IDI_ICON1;
  nid.uFlags = NIF_ICON | NIF_MESSAGE;
  nid.uCallbackMessage = WMAPP_SYSTRAYCB;
  nid.hIcon = s_hIcon;

  if (!Shell_NotifyIconW(NIM_ADD, &nid))
  {
    SPDLOG_CRITICAL("couldn't add notify icon, {}", redx::windowz::get_last_error());
    return false;
  }

  nid.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &nid);

  return true;
}

BOOL DeleteNotificationIcon(HWND hWnd)
{
  NOTIFYICONDATAW nid = {sizeof(nid)};
  nid.hWnd = hWnd;
  nid.uID = IDI_ICON1;
  //wnid.uFlags = NIF_GUID;
  //nid.guidItem = __uuidof(cpfs_systray_cls);
  return Shell_NotifyIcon(NIM_DELETE, &nid);
}

LRESULT WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  switch (Msg)
  {
    case WM_CREATE:
    {
      InitCtxMenu();
      AddNotificationIcon(hWnd);
      break;
    }
    case WM_DESTROY:
    {
      DeleteNotificationIcon(hWnd);
      DestroyMenu(s_hCtxMenu);
      PostQuitMessage(0);
      break;
    }
    case WMAPP_SYSTRAYCB:
    {
      switch (LOWORD(lParam))
      {
        case WM_CONTEXTMENU:
        {
            POINT const pt = {LOWORD(wParam), HIWORD(wParam)};
            ShowContextMenu(hWnd, pt);
            break;
        }
        default:
          break;
      }
      break;
    }
    case WM_COMMAND:
    {
      switch (LOWORD(wParam))
      {
        case IDM_ABOUT:
        {
          ShowAboutDlg(hWnd);
          break;
        }
        case IDM_LOG:
        {
          UINT state = GetMenuState(s_hCtxMenu, IDM_LOG, MF_BYCOMMAND);
          if (!(state & MF_CHECKED)) 
          { 
              CheckMenuItem(s_hCtxMenu, IDM_LOG, MF_BYCOMMAND | MF_CHECKED); 
              ShowConsole();
          }
          else 
          { 
              CheckMenuItem(s_hCtxMenu, IDM_LOG, MF_BYCOMMAND | MF_UNCHECKED); 
              HideConsole();
          } 

          break;
        }
        case IDM_EXIT:
        {
          DestroyWindow(hWnd);
          break;
        }
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  return DefWindowProc(hWnd, Msg, wParam, lParam);
}

INT_PTR CALLBACK AboutDlgFunc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam) 
{
  switch (Msg)
  {
    case WM_INITDIALOG:
    {
      SendMessageW(hWndDlg, WM_SETICON, 0, (LPARAM)s_hIcon);
      return TRUE;
    }
    case WM_COMMAND:
    {
      switch (LOWORD(wParam))
      {
        case IDOK:
        {
          EndDialog(hWndDlg, wParam);
          return TRUE;
        }
        case IDC_BUTTON1:
        {
          ShellExecuteA(NULL, "open", "https://github.com/PixelRick/", NULL, NULL, SW_SHOWNORMAL);
          return TRUE;
        }
        case IDC_BUTTON2:
        {
          ShellExecuteA(NULL, "open", "http://www.secfs.net/winfsp/", NULL, NULL, SW_SHOWNORMAL);
          return TRUE;
        }
        default:
          break;
      }
    }
    default:
      break;
  }

  return FALSE;
} 

BOOL ShowAboutDlg(HWND hWnd)
{
  //CreateDialogW
  DialogBoxW(s_hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, &AboutDlgFunc);
  return true;
}

void ShowContextMenu(HWND hWnd, POINT pt)
{
  if (s_hCtxMenu)
  {
    // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
    SetForegroundWindow(hWnd);

    // respect menu drop alignment
    UINT uFlags = TPM_RIGHTBUTTON;
    if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
    {
      uFlags |= TPM_RIGHTALIGN;
    }
    else
    {
      uFlags |= TPM_LEFTALIGN;
    }

    TrackPopupMenuEx(s_hCtxMenu, uFlags, pt.x, pt.y, hWnd, NULL);
  }
}

void debug_symlink(const std::filesystem::path& p)
{
  HANDLE Handle;

  SPDLOG_INFO("checking symlink {} target", p.string());

  Handle = CreateFileW(p.c_str(),
    FILE_READ_ATTRIBUTES | READ_CONTROL, 0, 0,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

  if (Handle != INVALID_HANDLE_VALUE)
  {
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;
    if (GetFileInformationByHandleEx(Handle, FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
    {
      SPDLOG_INFO("symlink target attributes:{:X} tag:{:X}",
        AttributeTagInfo.FileAttributes, AttributeTagInfo.ReparseTag);
    }
    else
    {
      SPDLOG_INFO("GetFileInformationByHandleEx failed:{}", redx::windowz::get_last_error());
    }

    BY_HANDLE_FILE_INFORMATION ByHandleInfo;
    if (GetFileInformationByHandle(Handle, &ByHandleInfo))
    {
      SPDLOG_INFO("symlink target nFileSizeHigh:{:X} nFileSizeLow:{:X} nNumberOfLinks:{:X} nFileIndexHigh:{:X} nFileIndexLow:{:X}",
        ByHandleInfo.nFileSizeHigh, ByHandleInfo.nFileSizeLow, ByHandleInfo.nNumberOfLinks, ByHandleInfo.nFileIndexHigh, ByHandleInfo.nFileIndexLow);
    }
    else
    {
      SPDLOG_INFO("GetFileInformationByHandle failed:{}", redx::windowz::get_last_error());
    }

    WCHAR wbuf[1000]{};
    if (GetFinalPathNameByHandleW(Handle, wbuf, 1000, FILE_NAME_OPENED))
    {
      SPDLOG_INFO("symlink target FinalPathName:{}", std::filesystem::path(wbuf).string());
    }
    else
    {
      SPDLOG_INFO("GetFinalPathNameByHandleW failed:{}", redx::windowz::get_last_error());
    }

    CloseHandle(Handle);
  }

  SPDLOG_INFO("checking symlink {}", p.string());

  Handle = CreateFileW(p.c_str(),
    FILE_READ_ATTRIBUTES | READ_CONTROL, 0, 0,
    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, 0);

  if (Handle != INVALID_HANDLE_VALUE)
  {
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;
    if (GetFileInformationByHandleEx(Handle, FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
    {
      SPDLOG_INFO("symlink attributes:{:X} tag:{:X}", AttributeTagInfo.FileAttributes, AttributeTagInfo.ReparseTag);
    }
    else
    {
      SPDLOG_INFO("GetFileInformationByHandleEx failed:{}", redx::windowz::get_last_error());
    }

    char buf[2000];
    DWORD resp_size = 0;
    if (DeviceIoControl(Handle, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, 2000, &resp_size, NULL))
    {
      auto reparse_data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buf);
      SPDLOG_INFO("symlink reparse_data->ReparseTag:{}", reparse_data->ReparseTag);
      if (reparse_data->ReparseTag == IO_REPARSE_TAG_SYMLINK)
      {
        const size_t subslen = reparse_data->SymbolicLinkReparseBuffer.SubstituteNameLength / 2;
        const size_t subsoff = reparse_data->SymbolicLinkReparseBuffer.SubstituteNameOffset / 2;
        const size_t prntlen = reparse_data->SymbolicLinkReparseBuffer.PrintNameLength / 2;
        const size_t prntoff = reparse_data->SymbolicLinkReparseBuffer.PrintNameOffset / 2;

        std::wstring_view wsubs(reparse_data->SymbolicLinkReparseBuffer.PathBuffer + subsoff, subslen);
        std::filesystem::path subs_path(wsubs);

        std::wstring_view wprnt(reparse_data->SymbolicLinkReparseBuffer.PathBuffer + prntoff, prntlen);
        std::filesystem::path prnt_path(wprnt);

        SPDLOG_INFO("symlink Flags:{:X}, SubstituteName:{} PrintName:{}", reparse_data->SymbolicLinkReparseBuffer.Flags, subs_path.string(), prnt_path.string());
      }
    }
    else
    {
      SPDLOG_INFO("DeviceIoControl failed:{}", redx::windowz::get_last_error());
    }

    WCHAR wbuf[1000]{};
    if (GetFinalPathNameByHandleW(Handle, wbuf, 1000, FILE_NAME_OPENED))
    {
       SPDLOG_INFO("symlink target FinalPathName:{}", std::filesystem::path(wbuf).string());
    }
    else
    {
      SPDLOG_INFO("GetFinalPathNameByHandleW failed:{}", redx::windowz::get_last_error());
    }

    CloseHandle(Handle);
  }

  WIN32_FIND_DATA FindData;
  Handle = FindFirstFileExW(p.c_str(), FindExInfoStandard, &FindData, FindExSearchNameMatch, NULL, 0);
  if (Handle != INVALID_HANDLE_VALUE)
  {
    SPDLOG_INFO("symlink dirsearch Attrs:{:X}, FileName:{} AltFileName:{}",
      FindData.dwFileAttributes, std::filesystem::path(FindData.cFileName).string(), std::filesystem::path(FindData.cAlternateFileName).string());
  }
}

