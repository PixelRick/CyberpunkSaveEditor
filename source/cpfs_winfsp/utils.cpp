#include <cpfs_winfsp/utils.hpp>
#include <redx/core.hpp>

// Microsoft said:
// Do not cast a pointer to a FILETIME structure to either a ULARGE_INTEGER* or __int64* value because it can cause alignment faults on 64-bit Windows.

bool set_file_times(HANDLE file_handle, redx::file_time creation, redx::file_time last_access, redx::file_time last_write, redx::file_time change)
{
  FILE_BASIC_INFO fbinfo{};
  if (!GetFileInformationByHandleEx(file_handle, FileBasicInfo, &fbinfo, sizeof(fbinfo)))
  {
    SPDLOG_ERROR("set_file_times: {}", redx::os::last_error_string());
    return false;
  }

  fbinfo.CreationTime   = winfsp_time_to_largeinteger(creation.hns_since_win_epoch.count());
  fbinfo.LastAccessTime = winfsp_time_to_largeinteger(last_access.hns_since_win_epoch.count());
  fbinfo.LastWriteTime  = winfsp_time_to_largeinteger(last_write.hns_since_win_epoch.count());
  fbinfo.ChangeTime     = winfsp_time_to_largeinteger(change.hns_since_win_epoch.count());

  if (!SetFileInformationByHandle(file_handle, FileBasicInfo, &fbinfo, sizeof(fbinfo)))
  {
    SPDLOG_ERROR("set_file_times: {}", redx::os::last_error_string());
    return false;
  }

  return true;
}

