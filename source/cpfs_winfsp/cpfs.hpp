#pragma once
#include <cpfs_winfsp/winfsp.h>

#include <filesystem>

#include <redx/core.hpp>
#include <redx/os/platform_utils.hpp>
#include <redx/depot/treefs.hpp>
#include <redx/io/oodle/oodle.hpp>

extern FSP_FILE_SYSTEM_INTERFACE s_cpfs_interface;

NTSTATUS fill_winfsp_file_info(FSP_FSCTL_FILE_INFO& fsp_finfo, const redx::depot::directory_entry& de);
NTSTATUS fill_winfsp_file_info(FSP_FSCTL_FILE_INFO& fsp_finfo, HANDLE handle);

// todo list:
//  - named streams (:raw for raw access, otherwise try to uncook)

struct cpfs
{
  cpfs()
  {
    m_volume_params = {};
    m_volume_params.SectorSize = 1;
    m_volume_params.SectorsPerAllocationUnit = 1;
    m_volume_params.VolumeCreationTime = redx::file_time(redx::clock::now()).hns_since_win_epoch.count();
    m_volume_params.VolumeSerialNumber = 0;
    m_volume_params.FileInfoTimeout = 1000;
    m_volume_params.ReparsePointsAccessCheck = 1;
    m_volume_params.ReparsePoints = 1;
    //m_volume_params.AllowOpenInKernelMode = 1;

    // i don't think it is possible to only provide case-sensitive search..
    // so the lowercase naming convention should be kept.

    m_volume_params.CaseSensitiveSearch = 0; 
    m_volume_params.CasePreservedNames = 0; // todo: check the eventual problems it would cause with a diff dir
    m_volume_params.UnicodeOnDisk = 1;
    m_volume_params.PersistentAcls = 1;
    m_volume_params.NamedStreams = 1;
    m_volume_params.ReadOnlyVolume = 1;
    m_volume_params.PostCleanupWhenModifiedOnly = 1;
    m_volume_params.PassQueryDirectoryPattern = 1;
    m_volume_params.FlushAndPurgeOnCleanup = 1;
    m_volume_params.UmFileContextIsUserContext2 = 1;
    wcscpy_s(m_volume_params.FileSystemName, L"CPFS");
    volume_label = L"CP2077 Game Depot";
  }

  ~cpfs()
  {
    reset();
  }

  void reset()
  {
    if (is_started())
    {
      shutdown();
    }

    if (m_fsp_fs != nullptr)
    {
      FspFileSystemDelete(m_fsp_fs);
      m_fsp_fs = nullptr;
    }
  }

  bool init(uint32_t fsp_loglvl)
  {
    if (m_fsp_fs)
    {
      return false;
    }

    auto game_bin_path_opt = redx::os::get_cp_executable_path();
    if (!game_bin_path_opt.has_value())
    {
      MessageBoxA(0, "Game path could not be located", "error", 0);
      return false;
    }

    auto game_bin_path = game_bin_path_opt.value();
    auto game_path = game_bin_path.parent_path().parent_path().parent_path();

    content_path = game_path / "archive/pc/content";
    if (!std::filesystem::exists(content_path))
    {
      MessageBoxA(0, "Game content path could not be located", "error", 0);
      return false;
    }

    if (!redx::oodle::is_available())
    {
      MessageBoxA(0, "oodle couldn't be loaded", "error", 0);
      return false;
    }

    SPDLOG_INFO("loading fsp lib..");
    if (!NT_SUCCESS(FspLoad(0)))
    {
      MessageBoxA(0, "WinFSP couldn't be loaded (install it first..)", "error", 0);
      return false;
    }

    SPDLOG_INFO("game path: {}", game_path.string());

    NTSTATUS Status = STATUS_SUCCESS;

    Status = FspFileSystemCreate(
        (PWSTR)(L"" FSP_FSCTL_DISK_DEVICE_NAME),
        &m_volume_params, &s_cpfs_interface, &m_fsp_fs);

    if (!NT_SUCCESS(Status))
    {
      SPDLOG_ERROR("FspFileSystemCreate: error {:08X}", Status);
      return false;
    }

    m_fsp_fs->UserContext = this;

    Status = FspFileSystemSetMountPoint(
      m_fsp_fs, NULL);

    if (!NT_SUCCESS(Status))
    {
      reset();

      SPDLOG_ERROR("FspFileSystemSetMountPoint: error {:08X}", Status);
      return false;
    }

    disk_letter = FspFileSystemMountPoint(m_fsp_fs);

    FspFileSystemSetDebugLog(m_fsp_fs, fsp_loglvl);

    return true;
  }

  bool load_archives()
  {
    //scope_timer st("load_archive loop");

    for (const auto& dirent: std::filesystem::directory_iterator(content_path))
    {
      auto fname = dirent.path().filename();
      auto sfname = fname.string();
      if (fname.extension() == ".archive")
      {
        if (redx::starts_with(sfname, "lang_"))
        {
          if (!redx::starts_with(sfname, "lang_en"))
          {
            continue;
          }
        }
    
        tfs.load_archive(dirent.path().string());
      }
    }

    return true;
  }

  bool start()
  {
    if (!m_fsp_fs || m_started)
    {
      return false;
    }

    NTSTATUS Status = FspFileSystemStartDispatcher(m_fsp_fs, 0);
    if (!NT_SUCCESS(Status))
    {
      SPDLOG_ERROR("FspFileSystemStartDispatcher: error {:08X}", Status);
      return false;
    }

    m_started = true;
    return true;
  }

  void shutdown()
  {
    if (m_started)
    {
      assert(m_fsp_fs);
      FspFileSystemStopDispatcher(m_fsp_fs);
      m_started = false;
    }
  }

  size_t get_total_size()
  {
    return tfs.get_total_size();
  }

  inline bool is_started() const
  {
    return m_started;
  }

  std::wstring disk_letter;
  std::wstring volume_label;

  std::filesystem::path content_path;
  redx::depot::treefs tfs;
  std::shared_mutex mtx;

private:

  bool m_started = false;
  FSP_FILE_SYSTEM* m_fsp_fs = nullptr;
  FSP_FSCTL_VOLUME_PARAMS m_volume_params;
};

