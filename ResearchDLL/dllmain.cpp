#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <fstream>
#include <filesystem>
#include "cpstructs.h"


uint64_t g_baseaddr = 0;
uint64_t g_vtbladdr = 0;
bool g_inited = false;

std::ofstream g_log;

// experiment: overwrite the serializer vtbl

// vtbl is between '%02ld:%02ld:%02ld, %ld.%02ld.%ld' and 'SaveCompressedChunkSize' strings

struct csav_vtbl_t
{
  uint64_t (*csav_func00            )(csav* _this);
  csav*    (*csav_func08            )(csav* _this, uint64_t flags);
  uint64_t (*csav_func10_src_read   )(csav* _this, char* pbuf, size_t len);
  uint64_t (*csav_func18_src_tellg  )(csav* _this);
  uint64_t (*csav_func20_src_size   )(csav* _this);
  bool     (*csav_func28_src_seek   )(csav* _this, size_t pos);
  void     (*csav_func30_src_uk0    )(csav* _this);
  void     (*csav_func38            )(csav* _this);
  char*    (*csav_func40_file_name  )(csav* _this);
  uint64_t (*csav_func48_vermajor   )(csav* _this);
  uint64_t (*csav_func50_verminor   )(csav* _this);
  uint64_t (*csav_func58            )(csav* _this, uint64_t uk);
  uint64_t (*csav_func60            )(csav* _this, uint64_t uk);
  bool     (*csav_func68_init       )(csav* _this);
  bool     (*csav_func70_seek_node  )(csav* _this, uint64_t hash64, bool err_if_not_fount);
  bool     (*csav_func78_close_node )(csav* _this);
  uint64_t (*csav_func80            )(csav* _this);
  void     (*csav_func88            )(csav* _this);
  bool     (*csav_func90_is_ok      )(csav* _this);
  bool     (*csav_func98_err_is_4   )(csav* _this);
};
csav_vtbl_t g_saved_tbl;

size_t g_depth = 0;

uint64_t csav_func10_src_read_hook(csav* _this, char* pbuf, size_t len)
{
  if (!g_log.is_open()) g_log.open("C:\\experiment_log.txt");

  g_log << len << ", ";
  return g_saved_tbl.csav_func10_src_read(_this, pbuf, len);
}

bool     csav_func70_seek_node_hook(csav* _this, uint64_t hash64, bool err_if_not_fount)
{
  if (!g_log.is_open()) g_log.open("C:\\experiment_log.txt");

  g_log << "\n node " << (uintptr_t)hash64 << " { \n";
  return g_saved_tbl.csav_func70_seek_node(_this, hash64, err_if_not_fount);
}

bool     csav_func78_close_node_hook(csav* _this)
{
  if (!g_log.is_open()) g_log.open("C:\\experiment_log.txt");

  g_log << "\n } \n";
  return g_saved_tbl.csav_func78_close_node(_this);
}


void init()
{
  g_log.open("C:\\experiment_log.txt");
  
  g_baseaddr = (uint64_t)GetModuleHandleA("Cyberpunk2077.exe");
  g_depth = 0;

  g_log << "HELLO";

  if (g_baseaddr == 0) {
    g_log << "target not found";
    return;
  }

  const uint64_t ida_baseaddr = 0x140000000;
  const uint64_t sig_addr     = 0x14345CAE8 - ida_baseaddr + g_baseaddr;
                 g_vtbladdr   = 0x14345CB10 - ida_baseaddr + g_baseaddr;

  if (0 != strcmp("%02ld:%02ld:%02ld, %ld.%02ld.%ld", (char*)sig_addr)) {
    g_log << "sig mismatch";
    return;
  }

  memcpy(&g_saved_tbl, (char*)g_vtbladdr, sizeof(g_saved_tbl));
  g_inited = true;

  // now apply hooks

  DWORD oldRw;
  if (VirtualProtect((void*)g_vtbladdr, sizeof(csav_vtbl_t), PAGE_EXECUTE_READWRITE, &oldRw))
  {
    csav_vtbl_t* target_vtbl = (csav_vtbl_t*)g_vtbladdr;
    target_vtbl->csav_func10_src_read = csav_func10_src_read_hook;
    target_vtbl->csav_func70_seek_node = csav_func70_seek_node_hook;
    target_vtbl->csav_func78_close_node = csav_func78_close_node_hook;

    g_log << "vtbl methods are hooked";

    VirtualProtect((void*)g_vtbladdr, sizeof(csav_vtbl_t), oldRw, NULL);
  }


}

void shutdown()
{
  g_log.close();

  DWORD oldRw;
  if (VirtualProtect((void*)g_vtbladdr, sizeof(csav_vtbl_t), PAGE_EXECUTE_READWRITE, &oldRw))
  {
    memcpy((char*)g_vtbladdr, &g_saved_tbl, sizeof(g_saved_tbl));
    g_log << "vtbl methods are unhooked";
    VirtualProtect((void*)g_vtbladdr, sizeof(csav_vtbl_t), oldRw, NULL);
  }
  
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
    init();
    break;
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  case DLL_PROCESS_DETACH:
    shutdown();
    break;
  }
  return TRUE;
}

