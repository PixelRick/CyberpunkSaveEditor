#define WIN32_NO_STATUS
#define NOMINMAX
#include <winfsp/winfsp.h>

#include <cpinternals/common.hpp>
#include <cpinternals/filesystem/archive.hpp>
#include <cpinternals/filesystem/treefs.hpp>
#include <cpinternals/oodle/oodle.hpp>

#include <cpfs_winfsp/cpfs.hpp>


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
      SPDLOG_INFO("GetFileInformationByHandleEx failed:{}", cp::windowz::get_last_error());
    }

    BY_HANDLE_FILE_INFORMATION ByHandleInfo;
    if (GetFileInformationByHandle(Handle, &ByHandleInfo))
    {
       SPDLOG_INFO("symlink target nFileSizeHigh:{:X} nFileSizeLow:{:X} nNumberOfLinks:{:X} nFileIndexHigh:{:X} nFileIndexLow:{:X}",
        ByHandleInfo.nFileSizeHigh, ByHandleInfo.nFileSizeLow, ByHandleInfo.nNumberOfLinks, ByHandleInfo.nFileIndexHigh, ByHandleInfo.nFileIndexLow);
    }
    else
    {
      SPDLOG_INFO("GetFileInformationByHandle failed:{}", cp::windowz::get_last_error());
    }

    WCHAR wbuf[1000]{};
    if (GetFinalPathNameByHandleW(Handle, wbuf, 1000, FILE_NAME_OPENED))
    {
       SPDLOG_INFO("symlink target FinalPathName:{}", std::filesystem::path(wbuf).string());
    }
    else
    {
      SPDLOG_INFO("GetFinalPathNameByHandleW failed:{}", cp::windowz::get_last_error());
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
      SPDLOG_INFO("GetFileInformationByHandleEx failed:{}", cp::windowz::get_last_error());
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
      SPDLOG_INFO("DeviceIoControl failed:{}", cp::windowz::get_last_error());
    }

    WCHAR wbuf[1000]{};
    if (GetFinalPathNameByHandleW(Handle, wbuf, 1000, FILE_NAME_OPENED))
    {
       SPDLOG_INFO("symlink target FinalPathName:{}", std::filesystem::path(wbuf).string());
    }
    else
    {
      SPDLOG_INFO("GetFinalPathNameByHandleW failed:{}", cp::windowz::get_last_error());
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


//int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nShowCmd)
int main(int argc, char* argv[])
{
  //auto ar = cp::archive::load("D:/Desktop/cpsavedit/bufferfiles/basegame_1_engine.archive");
  
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%x %X.%e] [%^-%L-%$] [tid:%t] [%s:%#] %!: %v");

  auto game_path_opt = cp::windowz::get_cp_executable_path();
  if (!game_path_opt.has_value())
  {
    SPDLOG_ERROR("game path not found");
  }

  auto game_path = game_path_opt.value().parent_path();
  auto content_path = game_path.parent_path().parent_path() / "archive/pc/content";

  SPDLOG_INFO("game path: {}", game_path.string());
  SPDLOG_INFO("oodle available: {}", cp::oodle::is_available() ? "yes" : "no");

  if (!std::filesystem::exists(content_path))
  {
    SPDLOG_ERROR("there is problem with the game content path");
  }


  SPDLOG_INFO("loading fsp lib..");
  if (!NT_SUCCESS(FspLoad(0)))
  {
    SPDLOG_ERROR("fsp couldn't be loaded (install it first..)");
    system("pause");
    return 0;
  }

  cpfs cpfs;

  SPDLOG_INFO("loading archives..");
  {
    scope_timer st("load_archive loop");

    if (1)
    {
      for (const auto& dirent: std::filesystem::directory_iterator(content_path))
      {
        auto fname = dirent.path().filename();
        auto sfname = fname.string();
        if (fname.extension() == ".archive")
        {
          if (cp::starts_with(sfname, "lang_"))
          {
            if (!cp::starts_with(sfname, "lang_en"))
            {
              continue;
            }
          }
      
          cpfs.tfs.load_archive(dirent.path().string());
        }
      }
    }
    else
    {
      cpfs.tfs.load_archive((content_path / "basegame_3_nightcity_gi.archive").string());
    }

  }

  if (0)
  {
    cp::filesystem::recursive_directory_iterator it(cpfs.tfs, "base\\worlds\\03_night_city\\sectors\\_generated\\global_illumination");
    SPDLOG_INFO("start");
    size_t i = 0;
    for (const auto& dirent: it)
    {
      SPDLOG_INFO("[{:04d}][{}] {}", i++,
        (dirent.is_reserved_for_file() ? "R" : (dirent.is_file() ? "F" : "D")),
        dirent.tfs_path().strv());

      if (dirent.is_reserved_for_file())
      {
        SPDLOG_INFO("IS_RESERVED_FOR_FILE");
        cp::filesystem::directory_entry d(cpfs.tfs, dirent.tfs_path());
        SPDLOG_INFO("resolved: {}", d.tfs_path().strv());
      }

      if (i > 500) break;
    }
    SPDLOG_INFO("end");
  }

  SPDLOG_INFO("initializing cpfs");
  cpfs.init(0);

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
    SPDLOG_INFO("symlink testing");
    debug_symlink("Z:\\0af6d3d6f362bf06");
  }

  SPDLOG_INFO("cpfs started, closing this window will stop the system");
  system("pause");
}



