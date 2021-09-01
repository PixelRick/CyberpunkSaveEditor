//#pragma once
//#include <cpfs_winfsp/winfsp.h>
//
//#include <redx/core.hpp>
//#include <redx/filesystem/archive_file_stream.hpp>
//#include <redx/filesystem/treefs.hpp>
//#include <cpfs_winfsp/utils.hpp>
//
//struct file_context
//{
//  file_context(std::wstring_view wrel_path)
//    : m_wrel_path(wrel_path) {}
//
//  virtual ~file_context() = default;
//
//  file_context(const file_context&) = delete;
//  file_context& operator=(const file_context&) = delete;
//
//  virtual NTSTATUS fill_fsp_info(FSP_FSCTL_FILE_INFO& fsp_finfo) = 0;
//
//  static inline const security_desc& default_secdesc()
//  {
//    static security_desc instance(
//      "O:BA"
//      "G:BA"
//      "D:P"
//      "(A;;FA;;;SY)"
//      "(A;;FA;;;BA)"
//      "(A;;FA;;;WD)"
//    );
//
//    return instance;
//  }
//
//protected:
//
//  std::wstring m_wrel_path;
//};
//
////winfsp_directory_buffer m_dir_buffer;
//
//
//struct cpfs_file_context
//  : public file_context
//{
//  cpfs_file_context() = default;
//  ~cpfs_file_context() override = default;
//
//  NTSTATUS fill_fsp_info(FSP_FSCTL_FILE_INFO& fsp_finfo) override
//  {
//    fsp_finfo = {};
//
//    fsp_finfo.FileAttributes  = m_dirent.is_file() ? FILE_ATTRIBUTE_ARCHIVE : FILE_ATTRIBUTE_DIRECTORY;
//
//    //fsp_finfo.ReparseTag    = 0;
//
//    const auto& finfo         = m_dirent.get_file_info();
//
//    fsp_finfo.AllocationSize  = (finfo.size + 4096) / 4096 * 4096;//finfo.disk_size;
//    fsp_finfo.FileSize        = finfo.size;
//    
//    fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch;
//    fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch;
//    fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch;
//    fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch;
//
//    //fsp_finfo.IndexNumber     = 0;
//    //fsp_finfo.HardLinks       = 0;
//    //fsp_finfo.EaSize          = 0;
//
//    return STATUS_SUCCESS;
//  }
//
//private:
//
//  cp::filesystem::directory_entry m_dirent;
//  cp::archive_file_stream m_afs;
//  mutable std::mutex m_afs_mtx;
//
//  std::unordered_set<cp::filesystem::path_id> overridden_pids;
//};
//
//struct win_file_context
//  : public file_context
//{
//  win_file_context() = default;
//  ~win_file_context() override = default;
//
//  NTSTATUS fill_fsp_info(FSP_FSCTL_FILE_INFO& fsp_finfo) override
//  {
//    fsp_finfo = {};
//
//    if (is_tfs_file)
//    {
//      ::fill_fsp_info(fsp_finfo, dirent);
//    }
//    else
//    {
//      BY_HANDLE_FILE_INFORMATION finfo{};
//      if (!GetFileInformationByHandle(handle.get(), &finfo))
//      {
//        DWORD dwErr = GetLastError();
//        SPDLOG_ERROR("GetFileInformationByHandle: {}", cp::windowz::format_error(dwErr));
//        return FspNtStatusFromWin32(dwErr);
//      }
//
//      FILE_STANDARD_INFO fstdinfo{};
//      if (!GetFileInformationByHandleEx(handle.get(), FileStandardInfo, &fstdinfo, sizeof(fstdinfo)))
//      {
//        DWORD dwErr = GetLastError();
//        SPDLOG_ERROR("GetFileInformationByHandleEx: {}", cp::windowz::format_error(dwErr));
//        return FspNtStatusFromWin32(dwErr);
//      }
//
//      fsp_finfo.FileAttributes  = finfo.dwFileAttributes;
//
//      fsp_finfo.FileSize        = fstdinfo.EndOfFile.QuadPart;
//      fsp_finfo.AllocationSize  = fstdinfo.AllocationSize.QuadPart;
//
//      fsp_finfo.CreationTime    = filetime_to_fsp_time(finfo.ftCreationTime);
//      fsp_finfo.LastAccessTime  = filetime_to_fsp_time(finfo.ftLastAccessTime);
//      fsp_finfo.LastWriteTime   = filetime_to_fsp_time(finfo.ftLastWriteTime);
//      fsp_finfo.ChangeTime      = fsp_finfo.LastWriteTime;
//    }
//
//    return STATUS_SUCCESS;
//  }
//
//
//  static inline const security_desc& sdesc()
//  {
//    static security_desc instance(
//      "O:BA"
//      "G:BA"
//      "D:P"
//      "(A;;FA;;;SY)"
//      "(A;;FA;;;BA)"
//      "(A;;FA;;;WD)");
//
//    return instance;
//  }
//
//  std::wstring wrel_path;
//
//  bool is_tfs_file = false;
//  cp::filesystem::directory_entry dirent;
//  cp::archive_file_stream afs;
//  mutable std::mutex m_afs_mtx;
//  bool m_symlink = false;
//
//  bool is_diff_only = false; // path cannot be converted to a cp path
//  win_handle handle;
//  
//  PVOID dir_buffer = nullptr;
//  std::unordered_set<cp::filesystem::path_id> overridden_pids;
//};
//
