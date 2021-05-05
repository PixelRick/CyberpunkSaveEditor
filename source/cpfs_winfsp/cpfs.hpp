#pragma once
#include <cpfs_winfsp/winfsp.hpp>

#include <filesystem>

#include <cpinternals/common.hpp>
#include <cpinternals/filesystem/archive.hpp>
#include <cpinternals/filesystem/treefs.hpp>

extern FSP_FILE_SYSTEM_INTERFACE s_cpfs_interface;

struct scope_timer
{
  scope_timer(std::string_view name)
    : name(name)
  {
    QueryPerformanceFrequency(&Frequency); 
    QueryPerformanceCounter(&StartingTime);
  }

  ~scope_timer()
  {
    QueryPerformanceCounter(&EndingTime);
    ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
    ElapsedMicroseconds.QuadPart *= 1000000;
    ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
    double time_taken = (double)ElapsedMicroseconds.QuadPart / 1000000.f;
    // should we change spdlog formatting before printing ?
    SPDLOG_INFO("{} took {:.9f}s", name, time_taken);
  }

  LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
  LARGE_INTEGER Frequency;
  std::string name;
};

struct cpfs
{
  cpfs()
  {
    m_volume_params = {};
    m_volume_params.SectorSize = 1;
    m_volume_params.SectorsPerAllocationUnit = 1;
    m_volume_params.VolumeCreationTime = cp::file_time(cp::clock::now()).hns_since_win_epoch;
    m_volume_params.VolumeSerialNumber = 0;
    m_volume_params.FileInfoTimeout = 1000;
    m_volume_params.ReparsePointsAccessCheck = 1;
    m_volume_params.ReparsePoints = 1;
    //m_volume_params.AllowOpenInKernelMode = 1;
    m_volume_params.CaseSensitiveSearch = 0;
    m_volume_params.CasePreservedNames = 1;
    m_volume_params.UnicodeOnDisk = 1;
    m_volume_params.PersistentAcls = 1;
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

  bool has_diffdir = false;
  std::filesystem::path diffdir_path;
  std::wstring disk_letter;
  std::wstring volume_label;
  cp::filesystem::treefs tfs;

private:

  bool m_started = false;
  FSP_FILE_SYSTEM* m_fsp_fs = nullptr;
  FSP_FSCTL_VOLUME_PARAMS m_volume_params;
};

