//#pragma once
//#define WIN32_NO_STATUS
//#define NOMINMAX
//#include <windows.h>
//#include <shellapi.h>
//#include <commctrl.h>
//#include <inttypes.h>
//
//#include <functional>
//#include <memory>
//#include <string>
//
//inline constexpr UINT WM_SYSTRAY_CALLBACK = WM_APP + 100;
//
//LRESULT wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
//
//LRESULT window_proc(UINT message, WPARAM wParam, LPARAM lParam)
//{
//  switch (message)
//  {
//  case WM_SIZE:
//    if (m_device.Get() && wParam != SIZE_MINIMIZED)
//      d3d_resize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
//    return 0;
//  case WM_DESTROY:
//    PostQuitMessage(0);
//    return 0;
//  case WM_DROPFILES:
//  {
//    HDROP hDrop = (HDROP)wParam;
//    UINT files_cnt = (UINT)DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
//    std::wstring wsPath;
//    for (UINT i = 0; i < files_cnt; ++i)
//    {
//      UINT nameLen = DragQueryFileW(hDrop, i, nullptr, 0);
//      if (nameLen)
//      {
//        wsPath.resize(nameLen + 1);
//        DragQueryFileW(hDrop, i, wsPath.data(), (UINT)wsPath.size());
//        while (wsPath.size() && wsPath.back() == L'\0')
//          wsPath.pop_back();
//        on_file_drop(wsPath);
//      }
//    }
//
//    return 0;
//  }
//  default:
//    break;
//  }
//  return ::DefWindowProc(m_hwnd, message, wParam, lParam);
//}
//
//
//struct systray_item
//{
//  systray_item(std::wstring text, std::function<void(systray_item*)>&& onclick, bool enabled, bool checked)
//    : m_text(text), m_onclick(std::move(onclick)), m_enabled(enabled), m_checked(checked) {}
//  
//  systray_item(std::wstring text, const std::function<void(systray_item*)>& onclick, bool enabled, bool checked)
//    : m_text(text), m_onclick(onclick), m_enabled(enabled), m_checked(checked) {}
//
//  systray_item(std::wstring text, bool enabled, std::initializer_list<systray_item> subitems)
//    : m_text(text), m_enabled(enabled), m_subitems(subitems) {}
//
//  void append_to_menu(HMENU hmenu, UINT& idgen)
//  {
//    if (m_text == L"-")
//    {
//      InsertMenuW(hmenu, idgen++, MF_SEPARATOR, TRUE, L"");
//      return;
//    }
//
//    MENUITEMINFO item{};
//    item.cbSize = sizeof(MENUITEMINFO);
//
//    item.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | MIIM_DATA;
//    item.fType = MFT_STRING;
//    item.fState = MFS_UNHILITE;
//    item.wID = idgen++;
//    item.dwTypeData = (LPWSTR)m_text.c_str();
//    item.dwItemData = (ULONG_PTR)this;
//
//    if (!m_subitems.empty())
//    {
//      HMENU hmenu = CreatePopupMenu();
//      for (auto& subitem : m_subitems)
//      {
//        subitem.append_to_menu(hmenu, idgen);
//      }
//
//      item.fMask = item.fMask | MIIM_SUBMENU;
//      item.hSubMenu = hmenu;
//    }
//
//    if (!m_enabled)
//    {
//      item.fState |= MFS_DISABLED;
//    }
//
//    if (m_checked)
//    {
//      item.fState |= MFS_CHECKED;
//    }
//
//    InsertMenuItemW(hmenu, item.wID, TRUE, &item);
//  }
//
//private:
//
//  std::wstring m_text;
//  bool m_enabled = true;
//  bool m_checked = false;
//
//  std::function<void(systray_item*)> m_onclick;
//  std::vector<systray_item> m_subitems;
//};
//
//struct systray_icon
//{
//  systray_icon() noexcept = default;
//
//  void reset()
//  {
//    if (m_displayed)
//    {
//      hide();
//    }
//
//    if (m_wnid.hIcon)
//    {
//      DestroyIcon(m_wnid.hIcon);
//      m_wnid.hIcon = NULL;
//    }
//
//    m_wnid = {};
//    m_inited = false;
//  }
//
//  bool init(HWND hwnd, uint16_t iconid)
//  {
//    if (!hwnd || m_inited)
//    {
//      return false;
//    }
//
//    reset();
//
//    if (!LoadIconMetric(0, MAKEINTRESOURCE(iconid), LIM_SMALL, &(m_wnid.hIcon)))
//    {
//      return false;
//    }
//
//    m_wnid.cbSize = sizeof(NOTIFYICONDATA); 
//    m_wnid.hWnd = hwnd;
//    m_wnid.uID = iconid; 
//    m_wnid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
//    m_wnid.uCallbackMessage = WM_SYSTRAY_CALLBACK;
//    m_wnid.uVersion = NOTIFYICON_VERSION_4;
//
//    m_inited = true;
//
//    return true;
//  }
//
//  bool show()
//  {
//    if (!Shell_NotifyIconW(NIM_ADD, &m_wnid));
//    {
//      return false;
//    }
//
//    m_displayed = true;
//    return true;
//  }
//
//  bool hide()
//  {
//    Shell_NotifyIconW(NIM_DELETE, &m_wnid);
//    m_displayed = false;
//  }
//
//  LRESULT wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
//  {
//  }
//
//private:
//
//  NOTIFYICONDATAW m_wnid{};
//  HMENU m_hmenu{0};
//  bool m_inited = false;
//  bool m_displayed = false;
//};
//
//
//
//HICON hMainIcon = LoadIcon(0,(LPCTSTR)MAKEINTRESOURCE(IDI_MAINICON));
//                    
//  NOTIFYICONDATA nid{};
//  
//  //nidApp.uCallbackMessage = WM_USER + 1; 
//
//  LoadStringW(0, IDC_TRAYMENU, nid.szTip, 100);
//  
//
//  Shell_NotifyIcon(NIM_ADD, &nid); 
//
