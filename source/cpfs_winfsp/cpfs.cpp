#include <cpfs_winfsp/cpfs.hpp>

#include <filesystem>
#include <cctype>
#include <cwchar>
#include <algorithm>
#include <random>

#include <redx/core.hpp>
#include <redx/os/platform_utils.hpp>
#include <redx/depot/directory_entry.hpp>
#include <redx/depot/directory_iterator.hpp>
#include <cpfs_winfsp/utils.hpp>


#define PRINT_CALL_ARGS


inline cpfs* fs_from_ffs(FSP_FILE_SYSTEM* FileSystem)
{
  return reinterpret_cast<cpfs*>(FileSystem->UserContext);
}

NTSTATUS fill_winfsp_file_info(FSP_FSCTL_FILE_INFO& fsp_finfo, const cp::depot::directory_entry& de)
{
  fsp_finfo = {};

  fsp_finfo.FileAttributes  = de.is_file() ? FILE_ATTRIBUTE_ARCHIVE : FILE_ATTRIBUTE_DIRECTORY;

  //fsp_finfo.ReparseTag      = 0;

  const auto& finfo         = de.get_file_info();

  fsp_finfo.AllocationSize  = (finfo.size + 4096) / 4096 * 4096;//finfo.disk_size;
  fsp_finfo.FileSize        = finfo.size;
  
  fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch.count();
  fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch.count();
  fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch.count();
  fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch.count();

  //fsp_finfo.IndexNumber     = 0;
  //fsp_finfo.HardLinks       = 0;
  //fsp_finfo.EaSize          = 0;

  return STATUS_SUCCESS;
}

// unused
NTSTATUS fill_winfsp_file_info(FSP_FSCTL_FILE_INFO& fsp_finfo, HANDLE handle)
{
  fsp_finfo = {};

  BY_HANDLE_FILE_INFORMATION finfo{};
  if (!GetFileInformationByHandle(handle, &finfo))
  {
    DWORD dwErr = GetLastError();
    SPDLOG_ERROR("GetFileInformationByHandle: {}", cp::os::format_error(dwErr));
    return FspNtStatusFromWin32(dwErr);
  }

  FILE_STANDARD_INFO fstdinfo{};
  if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &fstdinfo, sizeof(fstdinfo)))
  {
    DWORD dwErr = GetLastError();
    SPDLOG_ERROR("GetFileInformationByHandleEx: {}", cp::os::format_error(dwErr));
    return FspNtStatusFromWin32(dwErr);
  }

  fsp_finfo.FileAttributes  = finfo.dwFileAttributes;

  // todo: support diffdir reparse points
  //fsp_finfo.ReparseTag    = 0;

  fsp_finfo.AllocationSize  = fstdinfo.AllocationSize.QuadPart;
  fsp_finfo.FileSize        = fstdinfo.EndOfFile.QuadPart;
  
  fsp_finfo.CreationTime    = filetime_to_winfsp_time(finfo.ftCreationTime);
  fsp_finfo.LastAccessTime  = filetime_to_winfsp_time(finfo.ftLastAccessTime);
  fsp_finfo.LastWriteTime   = filetime_to_winfsp_time(finfo.ftLastWriteTime);
  fsp_finfo.ChangeTime      = fsp_finfo.LastWriteTime;

  //fsp_finfo.IndexNumber     = 0;
  //fsp_finfo.HardLinks       = 0;
  //fsp_finfo.EaSize          = 0;

  return STATUS_SUCCESS;
}

// cp depot paths are ascii-only, let's avoid the overhead of converters
void set_winfsp_dir_info_ascii_name(FSP_FSCTL_DIR_INFO& dir_info, std::string_view name)
{
  dir_info.Size = static_cast<UINT16>(offsetof(FSP_FSCTL_DIR_INFO, FileNameBuf) + name.length() * 2);
  std::copy(name.begin(), name.end(), dir_info.FileNameBuf);
}

void set_winfsp_dir_info_wname(FSP_FSCTL_DIR_INFO& dir_info, std::wstring_view wname)
{
  dir_info.Size = static_cast<UINT16>(offsetof(FSP_FSCTL_DIR_INFO, FileNameBuf) + wname.length() * 2);
  std::copy(wname.begin(), wname.end(), dir_info.FileNameBuf);
}

std::string ws_to_ascii(std::wstring_view ws, bool& success)
{
  std::string ret(ws.length(), 0);
  auto write_it = ret.begin();
  for (auto read_it = ws.begin(); read_it != ws.end(); ++read_it, ++write_it)
  {
    const wchar_t wc = *read_it;
    if (wc & 0xFF80)
    {
      success = false;
      return "";
    }
    *write_it = static_cast<char>(wc);
  }
  success = true;
  return ret;
}

struct file_context
{
  file_context() = default;
  ~file_context() = default;

  file_context(const file_context&) = delete;
  file_context& operator=(const file_context&) = delete;

  static inline const security_desc& sdesc()
  {
    static security_desc instance(
      "O:BA"
      "G:BA"
      "D:P"
      "(A;;FA;;;SY)"
      "(A;;FA;;;BA)"
      "(A;;FA;;;WD)");

    return instance;
  }

  bool opened_as_symlink = false;
  //bool is_diffdir_only = false; // path cannot be converted to a cp path (non ascii characters)

  std::wstring wrel_path;

  cp::depot::directory_entry cp_dirent;
  cp::archive_file_istream afs;
  mutable std::mutex afs_mtx;
  
  winfsp_directory_buffer dirbuf;
};


NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM* FileSystem, FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
  cpfs* fs = fs_from_ffs(FileSystem);

  VolumeInfo->TotalSize = fs->get_total_size();
  VolumeInfo->FreeSize = 0;

  // it's not length but size
  VolumeInfo->VolumeLabelLength = static_cast<UINT16>(2 * std::min(fs->volume_label.size(), size_t(32)));
  std::memcpy(VolumeInfo->VolumeLabel, fs->volume_label.data(), VolumeInfo->VolumeLabelLength);

  return STATUS_SUCCESS;
}

NTSTATUS SetVolumeLabel_(FSP_FILE_SYSTEM* FileSystem, PWSTR VolumeLabel, FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
  cpfs* fs = fs_from_ffs(FileSystem);

  fs->volume_label = std::wstring_view(VolumeLabel, wcsnlen(VolumeLabel, 32));
  
  return GetVolumeInfo(FileSystem, VolumeInfo);
}

NTSTATUS GetSecurityByName(
  FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, PUINT32 PFileAttributes,
  PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
#ifdef PRINT_CALL_ARGS
  wprintf(L"GetSecurityByName %s\n", FileName);
#endif

  cpfs* fs = fs_from_ffs(FileSystem);

  std::wstring_view wrelpath = FileName;

  bool tfs_compatible{};
  cp::path tfs_path(wrelpath, tfs_compatible);

  //if (fs->has_diffdir)
  //{
  //  std::wstring diff_path = fs->diffdir_path;
  //  diff_path.append(wrelpath);

  //  win_handle fh;

  //  fh = win_handle(
  //    CreateFileW(
  //      diff_path.c_str(),
  //      FILE_READ_ATTRIBUTES | READ_CONTROL, 0, 0,
  //      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0));

  //  DWORD dwErr = GetLastError();

  //  if (fh)
  //  {
  //    // retrieve file attributes

  //    if (PFileAttributes != nullptr)
  //    {
  //      FILE_ATTRIBUTE_TAG_INFO atinfo;
  //      if (!GetFileInformationByHandleEx(fh.get(), FileAttributeTagInfo, &atinfo, sizeof(atinfo)))
  //      {
  //        dwErr = GetLastError();
  //        SPDLOG_ERROR("couldn't get file info; {}", cp::windowz::format_error(dwErr));
  //        return FspNtStatusFromWin32(dwErr);
  //      }
  //      *PFileAttributes = atinfo.FileAttributes;
  //    }

  //    // retrieve security attributes

  //    if (PSecurityDescriptorSize != nullptr)
  //    {
  //      DWORD security_desc_size;
  //      if (!GetKernelObjectSecurity(
  //        fh.get(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
  //        SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &security_desc_size))
  //      {
  //        *PSecurityDescriptorSize = security_desc_size;
  //        dwErr = GetLastError();
  //        SPDLOG_ERROR("couldn't get file security; {}", cp::windowz::format_error(dwErr));
  //        return FspNtStatusFromWin32(dwErr);
  //      }
  //      *PSecurityDescriptorSize = security_desc_size;
  //    }

  //    return STATUS_SUCCESS;
  //  }
  //  else if (dwErr != ERROR_FILE_NOT_FOUND && dwErr != ERROR_PATH_NOT_FOUND)
  //  {
  //    SPDLOG_ERROR(
  //      "couldn't open diff file {}; reason: ({}){}",
  //      diff_path, dwErr, cp::windowz::format_error(dwErr)
  //    );
  //    return FspNtStatusFromWin32(dwErr);;
  //  }
  //}

  // else if we are not using diff dir or file wasn't found in there
  // we now look into the tfs

  if (tfs_compatible)
  {
    cp::depot::directory_entry entry(fs->tfs, tfs_path);
    if (!entry.exists() || !(entry.is_file() || entry.is_directory()))
    {
      // TODO: if not found, check for parent directory
      // and return STATUS_NOT_A_DIRECTORY if parent isn't a dir
      // or STATUS_OBJECT_PATH_NOT_FOUND if not found

      SPDLOG_INFO("no entry found for {}", tfs_path.strv());
      return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (PFileAttributes != nullptr)
    {
      if (entry.is_pidlink())
      {
        *PFileAttributes = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ARCHIVE;
      }
      else
      {
        *PFileAttributes = entry.is_file() ? FILE_ATTRIBUTE_ARCHIVE : FILE_ATTRIBUTE_DIRECTORY;
      }
    }

    if (PSecurityDescriptorSize != nullptr)
    {
      const size_t sd_size = file_context::sdesc().size();

      if (sd_size > *PSecurityDescriptorSize)
      {
        *PSecurityDescriptorSize = sd_size;
        return STATUS_BUFFER_OVERFLOW;
      }
      *PSecurityDescriptorSize = sd_size;

      if (SecurityDescriptor != nullptr)
      {
        std::memcpy(SecurityDescriptor, file_context::sdesc().get(), sd_size);
      }
    }
  }
  else
  {
    return STATUS_OBJECT_PATH_NOT_FOUND;
  }

  return STATUS_SUCCESS;
}

NTSTATUS Create(
  FSP_FILE_SYSTEM *FileSystem,
  PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
  UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
  PVOID* PFileContext, FSP_FSCTL_FILE_INFO* FileInfo)
{
  return STATUS_ACCESS_DENIED;

  //cpfs* fs = fs_from_ffs(FileSystem);
  //auto rel_path = std::filesystem::weakly_canonical(FileName).relative_path();
  //
  //SPDLOG_DEBUG("Create(.., \"{}\", ..)", rel_path.string());

  //if (!fs->has_diffdir)
  //{
  //  // todo: check that it conforms with the standard
  //  return STATUS_ACCESS_DENIED;
  //}

  // we want to prevent overrides of files with directories
  // todo: let's wait to see if WinFSP does this for us

  //auto diff_path = fs->diffdir_path / rel_path;

  //file_context* fctx = new file_context();

  //return STATUS_ACCESS_DENIED;

  //SECURITY_ATTRIBUTES SecurityAttributes;
  //SecurityAttributes.nLength = sizeof(SecurityAttributes);
  //SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
  //SecurityAttributes.bInheritHandle = FALSE;

  //ULONG CreateFlags;

  //CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;

  //if (CreateOptions & FILE_DELETE_ON_CLOSE)
  //{
  //  CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;
  //}

  //if (CreateOptions & FILE_DIRECTORY_FILE)
  //{
  //  CreateFlags |= FILE_FLAG_POSIX_SEMANTICS;
  //  FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
  //}
  //else
  //{
  //  FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
  //}

  //if (!FileAttributes)
  //{
  //  FileAttributes = FILE_ATTRIBUTE_NORMAL;
  //}

  //// now try create

  //fctx->handle = win_handle(
  //  CreateFileW(
  //    diff_path.c_str(), GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
  //    &SecurityAttributes, CREATE_NEW, CreateFlags | FileAttributes, 0));

  //// if failed to open, destroy context and return error
  //if (!fctx->handle)
  //{
  //  delete fctx;
  //  fctx = nullptr;
  //  DWORD dwErr = GetLastError();
  //  SPDLOG_ERROR("CreateFileW: {}", cp::windowz::format_error(dwErr));
  //  return FspNtStatusFromWin32(dwErr);
  //}

  //*PFileContext = fctx;

  //if (FileInfo != nullptr)
  //{
  //  return fill_winfsp_file_info(*FileInfo, fctx->handle.get());
  //}

  //return STATUS_SUCCESS;
}

NTSTATUS Open(
  FSP_FILE_SYSTEM *FileSystem,
  PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
  PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
  //scope_timer stimr("Open");

#ifdef PRINT_CALL_ARGS
  wprintf(L"Open %s CreateOptions:%X\n", FileName, CreateOptions);
#endif

  //TODO: create dirent for directories even if a diff one exists !

  cpfs* fs = fs_from_ffs(FileSystem);

  std::wstring_view wfilepath = FileName;

  bool tfs_compatible{};
  cp::path tfs_path(wfilepath, tfs_compatible);

  //SPDLOG_DEBUG("Open(.., \"{}\", ..)", tfs_path.strv());

  file_context* fctx = new file_context();
  fctx->wrel_path = wfilepath.substr(1);

  auto& dirent = fctx->cp_dirent;

  /*fctx->is_diff_only = !tfs_compatible;

  if (fs->has_diffdir)
  {
    auto diff_path = fs->diffdir_path / wfilepath;

    ULONG CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;

    if (CreateOptions & FILE_DELETE_ON_CLOSE)
    {
      CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;
    }

    if (CreateOptions & FILE_OPEN_REPARSE_POINT)
    {
      CreateFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    fctx->handle = win_handle(
      CreateFileW(
        diff_path.c_str(),
        GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, CreateFlags, 0));
  }

  if (fctx->handle)
  {
    *PFileContext = fctx;
  }
  else*/

  if (tfs_compatible)
  {
    // check in tfs
    dirent.assign(fs->tfs, tfs_path);

    if (dirent.is_file())
    {
      fs->tfs.open_archive_file_istream(fctx->afs, dirent.pid());
      assert(fctx->afs.is_open());
    }

    if (!dirent.exists() || !(dirent.is_file() || dirent.is_directory()))
    {
      delete fctx;
      fctx = nullptr;

      SPDLOG_DEBUG("\"{}\" not found", tfs_path.strv());

      // TODO: same as in GetSecurityByName
      return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    /*if (dirent.is_directory())
    {
      std::random_device rd;
      std::mt19937_64 e2(rd());
      std::uniform_int_distribution<uint64_t> dist;

      for (int i = 0; i < 2000; ++i)
      {
        fctx->overridden_pids.emplace(dist(e2));
      }
    }*/

    *PFileContext = fctx;
  }
  else
  {
    delete fctx;
    fctx = nullptr;
    return STATUS_OBJECT_NAME_NOT_FOUND;
  }

  // FILE_NON_DIRECTORY_FILE

  if (FileInfo != nullptr)
  {
    // FILE_OPEN_REPARSE_POINT means the caller doesn't want the target
    if (dirent.is_pidlink() && (CreateOptions & FILE_OPEN_REPARSE_POINT))
    {
      fctx->opened_as_symlink = true;

      auto& fsp_finfo = *FileInfo;
      fsp_finfo = {};

      fsp_finfo.FileAttributes  = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ARCHIVE;
      
      fsp_finfo.ReparseTag      = IO_REPARSE_TAG_SYMLINK;
      
      const auto& finfo         = dirent.get_file_info();
      
      fsp_finfo.AllocationSize  = 0;
      fsp_finfo.FileSize        = 0;
      
      fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch.count();
      fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch.count();
      fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch.count();
      fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch.count();
    }
    else
    {
      return fill_winfsp_file_info(*FileInfo, dirent);
    }
  }

  return STATUS_SUCCESS;
}

NTSTATUS Overwrite(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  return STATUS_ACCESS_DENIED;

  /*cpfs* fs = fs_from_ffs(FileSystem);
  auto fctx = reinterpret_cast<file_context*>(FileContext);

  if (fctx->is_cp_file)
  {
    SPDLOG_ERROR("NOT IMPLEMENTED");
    return STATUS_ABANDONED;
  }
  else
  {
    FILE_BASIC_INFO BasicInfo = { 0 };
    FILE_ALLOCATION_INFO AllocationInfo = { 0 };
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;

    if (ReplaceFileAttributes)
    {
      if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

      BasicInfo.FileAttributes = FileAttributes;
      if (!SetFileInformationByHandle(
        fctx->handle.get(),
        FileBasicInfo, &BasicInfo, sizeof BasicInfo))
        return FspNtStatusFromWin32(GetLastError());
    }
    else if (0 != FileAttributes)
    {
      if (!GetFileInformationByHandleEx(
        fctx->handle.get(),
        FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
        return FspNtStatusFromWin32(GetLastError());

      BasicInfo.FileAttributes = FileAttributes | AttributeTagInfo.FileAttributes;
      if (BasicInfo.FileAttributes ^ FileAttributes)
      {
        if (!SetFileInformationByHandle(
          fctx->handle.get(),
          FileBasicInfo, &BasicInfo, sizeof BasicInfo))
          return FspNtStatusFromWin32(GetLastError());
      }
    }

    if (!SetFileInformationByHandle(
      fctx->handle.get(),
      FileAllocationInfo, &AllocationInfo, sizeof AllocationInfo))
      return FspNtStatusFromWin32(GetLastError());

    return fctx->fill_fsp_info(*FileInfo);
  }*/
}

VOID Cleanup(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, PWSTR FileName, ULONG Flags)
{
  SPDLOG_DEBUG("Cleanup");
  //HANDLE Handle = HandleFromContext(FileContext);
  //
  //if (Flags & FspCleanupDelete)
  //{
  //  CloseHandle(Handle);
  //
  //  /* this will make all future uses of Handle to fail with STATUS_INVALID_HANDLE */
  //  HandleFromContext(FileContext) = INVALID_HANDLE_VALUE;
  //}
}

VOID Close(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext)
{
  SPDLOG_DEBUG("Close");
  delete reinterpret_cast<file_context*>(FileContext);
}

static NTSTATUS Read(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
  PULONG PBytesTransferred)
{
  cpfs* fs = fs_from_ffs(FileSystem);
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (fctx->cp_dirent.is_file())
  {
    auto& afs = fctx->afs;
    if (!afs.is_open())
    {
      SPDLOG_ERROR("!fctx->afs.is_open()");
      return STATUS_INVALID_HANDLE;
    }

    if (!afs.size())
    {
      SPDLOG_ERROR("!fctx->afs.size()");
      return STATUS_INVALID_HANDLE;
    }

    ULONG len = 0;

    if (Offset < afs.size())
    {
      len = std::min(ULONG(afs.size() - Offset), Length);

      if (Buffer)
      {
        std::scoped_lock<std::mutex> sl(fctx->afs_mtx);
        afs.seek(Offset);
        afs.read(std::span<char>((char*)Buffer, len));
      }
    }

    if (PBytesTransferred)
    {
      *PBytesTransferred = len;
    }

    return STATUS_SUCCESS;
  }

  return STATUS_ACCESS_DENIED;
}

static NTSTATUS Write(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
  BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
  PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo)
{
  return STATUS_ACCESS_DENIED;

  //auto fctx = reinterpret_cast<file_context*>(FileContext);

  //if (fctx->is_tfs_file)
  //{
  //  SPDLOG_ERROR("trying to write an archive file");
  //  return STATUS_ACCESS_DENIED;
  //}
  //else
  //{
  //  // TODO: ASYNC IO

  //  const HANDLE Handle = fctx->handle.get();
  //  LARGE_INTEGER FileSize;
  //  OVERLAPPED Overlapped = { 0 };

  //  if (ConstrainedIo)
  //  {
  //    if (!GetFileSizeEx(Handle, &FileSize))
  //    {
  //      DWORD dwErr = GetLastError();
  //      SPDLOG_ERROR("GetFileSizeEx: {}", cp::windowz::format_error(dwErr));
  //      return FspNtStatusFromWin32(dwErr);
  //    }

  //    if (Offset >= (UINT64)FileSize.QuadPart)
  //      return STATUS_SUCCESS;
  //    if (Offset + Length > (UINT64)FileSize.QuadPart)
  //      Length = (ULONG)((UINT64)FileSize.QuadPart - Offset);
  //  }

  //  Overlapped.Offset = (DWORD)Offset;
  //  Overlapped.OffsetHigh = (DWORD)(Offset >> 32);

  //  if (!WriteFile(Handle, Buffer, Length, PBytesTransferred, &Overlapped))
  //  {
  //    DWORD dwErr = GetLastError();
  //    SPDLOG_ERROR("WriteFile: {}", cp::windowz::format_error(dwErr));
  //    return FspNtStatusFromWin32(dwErr);
  //  }

  //  return fctx->fill_fsp_info(*FileInfo);
  //}

  //return STATUS_SUCCESS;
}

NTSTATUS Flush(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);

  return STATUS_ACCESS_DENIED;

  //if (fctx->is_tfs_file)
  //{
  //  SPDLOG_ERROR("trying to write an archive file");
  //  return STATUS_ACCESS_DENIED;
  //}
  //else
  //{
  //  const HANDLE Handle = fctx->handle.get();

  //  /* we do not flush the whole volume, so just return SUCCESS */
  //  if (Handle == 0)
  //  {
  //    return STATUS_SUCCESS;
  //  }

  //  if (!FlushFileBuffers(Handle))
  //  {
  //    DWORD dwErr = GetLastError();
  //    SPDLOG_ERROR("FlushFileBuffers: {}", cp::windowz::format_error(dwErr));
  //    return FspNtStatusFromWin32(dwErr);
  //  }

  //  return fctx->fill_fsp_info(*FileInfo);
  //}

  //return STATUS_SUCCESS;
}

NTSTATUS GetFileInfo(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  NTSTATUS ret = fill_winfsp_file_info(*FileInfo, fctx->cp_dirent);

  if (fctx->opened_as_symlink)
  {
    FileInfo->FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
  }

  return ret;
}

NTSTATUS SetBasicInfo(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, UINT32 FileAttributes,
  UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  return STATUS_ACCESS_DENIED;

  //auto fctx = reinterpret_cast<file_context*>(FileContext);
  //if (!fctx)
  //{
  //  SPDLOG_ERROR("fctx is null!");
  //  return STATUS_INVALID_HANDLE;
  //}

  //{
  //  const HANDLE Handle = fctx->handle.get();
  //  FILE_BASIC_INFO BasicInfo{};

  //  if (FileAttributes == INVALID_FILE_ATTRIBUTES)
  //  {
  //    FileAttributes = 0;
  //  }
  //  else if (FileAttributes == 0)
  //  {
  //    FileAttributes = FILE_ATTRIBUTE_NORMAL;
  //  }

  //  BasicInfo.FileAttributes = FileAttributes;
  //  BasicInfo.CreationTime.QuadPart = CreationTime;
  //  BasicInfo.LastAccessTime.QuadPart = LastAccessTime;
  //  BasicInfo.LastWriteTime.QuadPart = LastWriteTime;
  //  //BasicInfo.ChangeTime = ChangeTime;

  //  if (!SetFileInformationByHandle(Handle, FileBasicInfo, &BasicInfo, sizeof BasicInfo))
  //  {
  //    DWORD dwErr = GetLastError();
  //    SPDLOG_ERROR("SetFileInformationByHandle: {}", cp::windowz::format_error(dwErr));
  //    return FspNtStatusFromWin32(dwErr);
  //  }

  //  return fctx->fill_fsp_info(*FileInfo);
  //}

  //return STATUS_INVALID_HANDLE;
}

NTSTATUS SetFileSize(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  return STATUS_ACCESS_DENIED;

  /*auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (fctx->is_cp_file)
  {
    return STATUS_ACCESS_DENIED;
  }
  else
  {
    return STATUS_INVALID_HANDLE;
    if (SetAllocationSize)
    {
      FILE_ALLOCATION_INFO AllocationInfo{};
      AllocationInfo.AllocationSize.QuadPart = NewSize;

      if (!SetFileInformationByHandle(
        fctx->handle.get(),
        FileAllocationInfo, &AllocationInfo, sizeof AllocationInfo))
      {
        DWORD dwErr = GetLastError();
        SPDLOG_ERROR("SetFileInformationByHandle: {}", redx::windowz::format_error(dwErr));
        return FspNtStatusFromWin32(dwErr);
      }
    }
    else
    {
      FILE_END_OF_FILE_INFO EndOfFileInfo{};
      EndOfFileInfo.EndOfFile.QuadPart = NewSize;

      if (!SetFileInformationByHandle(
        fctx->handle.get(),
        FileEndOfFileInfo, &EndOfFileInfo, sizeof EndOfFileInfo))
      {
        DWORD dwErr = GetLastError();
        SPDLOG_ERROR("SetFileInformationByHandle: {}", redx::windowz::format_error(dwErr));
        return FspNtStatusFromWin32(dwErr);
      }
    }
  }

  return STATUS_INVALID_HANDLE;*/
}

//static NTSTATUS Rename(
//  FSP_FILE_SYSTEM* FileSystem,
//  PVOID FileContext,
//  PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
//{
//  cpfs* fs = fs_from_ffs(FileSystem);
//  auto rel_path = std::filesystem::weakly_canonical(FileName).relative_path();
//  auto diff_path = fs->diffdir_path / rel_path;
//
//  auto fctx = reinterpret_cast<file_context*>(FileContext);
//
//  if (fctx->is_tfs_file)
//  {
//    return STATUS_ACCESS_DENIED;
//  }
//  else
//  {
//  }
//
//  PTFS* Ptfs = (PTFS*)FileSystem->UserContext;
//  WCHAR FullPath[FULLPATH_SIZE], NewFullPath[FULLPATH_SIZE];
//
//  if (!ConcatPath(Ptfs, FileName, FullPath))
//    return STATUS_OBJECT_NAME_INVALID;
//
//  if (!ConcatPath(Ptfs, NewFileName, NewFullPath))
//    return STATUS_OBJECT_NAME_INVALID;
//
//  if (!MoveFileExW(FullPath, NewFullPath, ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0))
//  {
//    DWORD dwErr = GetLastError();
//    SPDLOG_ERROR("MoveFileExW: {}", redx::windowz::format_error(dwErr));
//    return FspNtStatusFromWin32(dwErr);
//  }
//
//  return STATUS_SUCCESS;
//}

NTSTATUS GetSecurity(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext,
  PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (PSecurityDescriptorSize != nullptr)
  {
    const size_t sd_size = file_context::sdesc().size();

    if (sd_size > *PSecurityDescriptorSize)
    {
      *PSecurityDescriptorSize = sd_size;
      return STATUS_BUFFER_OVERFLOW;
    }
    *PSecurityDescriptorSize = sd_size;

    if (SecurityDescriptor != nullptr)
    {
      std::memcpy(SecurityDescriptor, file_context::sdesc().get(), sd_size);
    }
  }

  return STATUS_SUCCESS;
 
  /*{
    DWORD SecurityDescriptorSizeNeeded;
    if (!GetKernelObjectSecurity(
      fctx->handle.get(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
      SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &SecurityDescriptorSizeNeeded))
    {
      *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
      DWORD dwErr = GetLastError();
      SPDLOG_ERROR("GetKernelObjectSecurity: {}", redx::windowz::format_error(dwErr));
      return FspNtStatusFromWin32(dwErr);
    }
    *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
    return STATUS_SUCCESS;
  }

  return STATUS_INVALID_HANDLE;*/
}

NTSTATUS SetSecurity(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext,
  SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
  return STATUS_ACCESS_DENIED;

  /*auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (fctx->is_cp_file)
  {
    return STATUS_ACCESS_DENIED;
  }
  else
  {
    if (!SetKernelObjectSecurity(fctx->handle.get(), SecurityInformation, ModificationDescriptor))
    {
      DWORD dwErr = GetLastError();
      SPDLOG_ERROR("SetKernelObjectSecurity: {}", redx::windowz::format_error(dwErr));
      return FspNtStatusFromWin32(dwErr);
    }
  }

  return STATUS_SUCCESS;*/
}

#define FULLPATH_SIZE                   (MAX_PATH + FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR))

// supports Pattern and Marker
NTSTATUS ReadDirectory(
  FSP_FILE_SYSTEM* FileSystem, PVOID FileContext,
  PWSTR Pattern, PWSTR Marker,
  PVOID Buffer, ULONG Length,
  PULONG PBytesTransferred)
{
  //scope_timer stimr("ReadDirectory");

  cpfs* fs = fs_from_ffs(FileSystem);
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

#ifdef PRINT_CALL_ARGS
  printf("ReadDirectory(.., \"%S\", \"%S\", \"%S\", .., %d, ..)\n", fctx->wrel_path.c_str(), Pattern, Marker, Length);
#endif

  const auto& dirent = fctx->cp_dirent;

  if (!dirent.is_directory())
  {
    SPDLOG_ERROR("read dir called on non-dir");
    return STATUS_NOT_A_DIRECTORY;
  }

  // windows folder copy does check each file by doing a read dir with filename as the pattern..
  // in this case it is way faster to only return only one entry
  bool single_entry = false;
  bool use_marker_for_tfs = false;
  bool tfs_marker_is_pattern = false;
  bool invalid_search = false;
  std::string tfs_marker;

  // note: the driver will do a pattern check itself too
  // can we do something fast enough that it is worth it.. ?
  if (Pattern != NULL)
  {
    const std::wstring_view wpattern_view = Pattern;

    // check pattern for wildcards
    const bool has_wildcards = std::find_if(
      wpattern_view.begin(), wpattern_view.end(),
      [](wchar_t pc){
        return pc == '*' || pc == '?' || pc == '>' || pc == '<' || pc == '"';
      }) != wpattern_view.end();

    if (!has_wildcards)
    {
      single_entry = true;

      bool converted{};
      tfs_marker = ws_to_ascii(wpattern_view, converted);

      // case insensitivity
      std::transform(tfs_marker.begin(), tfs_marker.end(), tfs_marker.begin(),
        [](char c) -> char { return std::tolower(c); });

      use_marker_for_tfs = converted;
      invalid_search = !converted;
      tfs_marker_is_pattern = true;

      if (Marker != NULL) // a weird case
      {
        // an even weirder sub-case
        if (_wcsicmp(Marker, Pattern))
        {
          FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
          return STATUS_SUCCESS;
        }

        SPDLOG_WARN("Pattern equals Marker");
        wprintf(L"Filename:%s Pattern:%s Marker:%s\n", fctx->wrel_path.c_str(), Pattern, Marker); 
      }
    }
  }
  else
  {
    Pattern = (PWSTR)L"*";
  }

  std::array<char, sizeof(FSP_FSCTL_DIR_INFO) + MAX_PATH * 2> dir_info_buf{};
  auto dir_info = reinterpret_cast<FSP_FSCTL_DIR_INFO*>(dir_info_buf.data());

  static_assert(offsetof(FSP_FSCTL_DIR_INFO, FileNameBuf) == sizeof(FSP_FSCTL_DIR_INFO));

  if (Marker != NULL)
  {
    // search continuation

    bool converted{};
    tfs_marker = ws_to_ascii(Marker, converted);
    use_marker_for_tfs = converted;
    invalid_search = !converted; // if pattern isn't tfs compatible, disable listing
  }

  size_t added_cnt = 0;
  bool buffer_is_full = false;

  if (!invalid_search)
  {
    if (dirent.has_parent())
    {
      if (Marker == 0)
      {
        // add "."
        // add ".."
      }
      else if (!wcscmp(Marker, L"."))
      {
        // add ".."
      }
    }

    cp::depot::directory_entry iter_dirent;

    if (use_marker_for_tfs)
    {
      cp::path marker_path = dirent.tfs_path() / cp::path(tfs_marker, cp::path::already_normalized_tag{});

      SPDLOG_DEBUG("dir:{} marker_path:{}", fctx->cp_dirent.tfs_path().strv(), marker_path.strv());

      iter_dirent.assign(fs->tfs, marker_path);

      if (iter_dirent.is_pidlink())
      {
        // special behavior with pidlinks
      
        dir_info_buf.fill(0);
        set_winfsp_dir_info_ascii_name(*dir_info, tfs_marker);
        
        auto& fsp_finfo = dir_info->FileInfo;
        
        fsp_finfo.FileAttributes  = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ARCHIVE;
        fsp_finfo.ReparseTag      = IO_REPARSE_TAG_SYMLINK;

        //fsp_finfo.FileAttributes  = FILE_ATTRIBUTE_ARCHIVE;
        //fsp_finfo.ReparseTag      = 0;

        const auto& finfo         = iter_dirent.get_file_info();
      
        fsp_finfo.AllocationSize  = 0;
        fsp_finfo.FileSize        = 0;

        //fsp_finfo.AllocationSize  = finfo.disk_size;
        //fsp_finfo.FileSize        = finfo.size;
        
        fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch.count();
        fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch.count();
        fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch.count();
        fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch.count();
      
        //fsp_finfo.IndexNumber     = 0;
        //fsp_finfo.HardLinks       = 0;
        //fsp_finfo.EaSize          = 0;
      
        buffer_is_full = !FspFileSystemAddDirInfo(dir_info, Buffer, Length, PBytesTransferred);
      
        ++added_cnt;
        single_entry = true;
      
        iter_dirent = cp::depot::directory_entry();
      }
      else if (!iter_dirent.exists())
      {
        SPDLOG_DEBUG("marker not found in tfs");
      }

      if (!tfs_marker_is_pattern)
      {
        // when marker is given, caller expects next entry
        iter_dirent.assign_next();
      }
    }
    else
    {
      iter_dirent = dirent;
      iter_dirent.assign_first_child();
    }

    while (iter_dirent.exists())
    {
      dir_info_buf.fill(0);
      set_winfsp_dir_info_ascii_name(*dir_info, iter_dirent.filename_strv());
      fill_winfsp_file_info(dir_info->FileInfo, iter_dirent);

      //SPDLOG_INFO("added {}", iter_dirent.filename_strv());

      buffer_is_full = !FspFileSystemAddDirInfo(dir_info, Buffer, Length, PBytesTransferred);

      if (buffer_is_full)
      {
        break;
      }

      ++added_cnt;
        
      if (single_entry)
      {
        break;
      }

      iter_dirent.assign_next();
    }
  }

  if (!buffer_is_full)
  {
    FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
  }

  SPDLOG_DEBUG("added {} entries, buffer_is_full:{} single_entry:{} use_marker_for_tfs:{}",
    added_cnt, buffer_is_full, single_entry, use_marker_for_tfs);

  return STATUS_SUCCESS;
}

bool try_fill_symlink_reparse_data(const cp::depot::directory_entry& dirent, void* buffer, size_t* inout_size)
{
  assert(dirent.path().strv().size() == 16);

  auto path = dirent.tfs_path();

  const auto& substitute_name = /*std::string("Z:\\") +*/ /*"\\" +*/ path.string();

  constexpr size_t header_size = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer);
  constexpr size_t path_offset = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer);

  const UINT16 substitute_name_len = static_cast<UINT16>(substitute_name.size());
  const UINT16 substitute_name_bsize = substitute_name_len * 2;
  const UINT16 buflen = static_cast<UINT16>(path_offset + substitute_name_bsize * 2);
  const UINT16 datalen = buflen - header_size;

  if (buflen > *inout_size)
  {
    *inout_size = 0;
    return false;
  }

  *inout_size = buflen;

  auto reparse_data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);

  memset(buffer, 0, buflen);

  reparse_data->ReparseTag = IO_REPARSE_TAG_SYMLINK;
  reparse_data->ReparseDataLength = datalen;
  reparse_data->Reserved = 0;
  reparse_data->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;

  WCHAR* path_buffer = reparse_data->SymbolicLinkReparseBuffer.PathBuffer;

  std::copy(substitute_name.begin(), substitute_name.end(), &path_buffer[0]);
  std::copy(substitute_name.begin(), substitute_name.end(), &path_buffer[substitute_name_len]);

  reparse_data->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
  reparse_data->SymbolicLinkReparseBuffer.SubstituteNameLength = substitute_name_bsize;
  reparse_data->SymbolicLinkReparseBuffer.PrintNameOffset = substitute_name_bsize;//path_bsize;
  reparse_data->SymbolicLinkReparseBuffer.PrintNameLength = substitute_name_bsize;//32;

  return true;
}

NTSTATUS ResolveReparsePoints(
  FSP_FILE_SYSTEM* FileSystem,
  PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
  PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
  cpfs* fs = fs_from_ffs(FileSystem);
  auto reparse_data = reinterpret_cast<REPARSE_DATA_BUFFER*>(Buffer);

#ifdef PRINT_CALL_ARGS
  wprintf(L"ResolveReparsePoints %s ReparsePointIndex:%d ResolveLastPathComponent:%d *PSize:%lld\n", FileName, ReparsePointIndex, ResolveLastPathComponent, *PSize);
#endif

  std::wstring_view wfilepath = FileName;

  bool tfs_compatible{};
  cp::path tfs_path(wfilepath, tfs_compatible);

  if (tfs_compatible)
  {
    cp::depot::directory_entry dirent(fs->tfs, tfs_path);
    if (dirent.is_pidlink())
    {
      if (!ResolveLastPathComponent)
      {
        return STATUS_REPARSE_POINT_NOT_RESOLVED;
      }

      if (!try_fill_symlink_reparse_data(dirent, Buffer, PSize))
      {
        return STATUS_REPARSE_POINT_NOT_RESOLVED;
      }

      PIoStatus->Status = STATUS_REPARSE;
      PIoStatus->Information = IO_REPARSE_TAG_SYMLINK;
      return STATUS_REPARSE;
    }
  }

  *PSize = 0;
  return STATUS_REPARSE_POINT_NOT_RESOLVED;
}

// e.g. "Z:\0af6d3d6f362bf06"

NTSTATUS GetReparsePoint(
  FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
  PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
{
  cpfs* fs = fs_from_ffs(FileSystem);
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

#ifdef PRINT_CALL_ARGS
  wprintf(L"GetReparsePoint %s *PSize:%lld\n", fctx->wrel_path.c_str(), *PSize);
#endif


  if (fctx->cp_dirent.is_pidlink())
  {
    if (!try_fill_symlink_reparse_data(fctx->cp_dirent, Buffer, PSize))
    {
      return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
  }

  return STATUS_NOT_A_REPARSE_POINT;
}

FSP_FILE_SYSTEM_INTERFACE s_cpfs_interface =
{
    GetVolumeInfo,
    SetVolumeLabel_,
    GetSecurityByName,
    Create,
    Open,
    Overwrite,
    Cleanup,
    Close,
    Read,
    Write,
    Flush,
    GetFileInfo,
    SetBasicInfo,
    SetFileSize,
    0,//CanDelete,
    0,//Rename,
    GetSecurity,
    SetSecurity,
    ReadDirectory,
    ResolveReparsePoints,
    GetReparsePoint,
    0,//SetReparsePoint,
    0,//DeleteReparsePoint,
    0,
    0,//GetDirInfoByName,
    0,
    0,
    0,
    0,
    0,
    0,
};






