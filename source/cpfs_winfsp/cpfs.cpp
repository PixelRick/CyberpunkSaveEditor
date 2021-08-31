#include <cpfs_winfsp/cpfs.hpp>

#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#include <codecvt>

#include <filesystem>
#include <cctype>
#include <cwchar>
#include <algorithm>

#include <redx/core.hpp>

#include <cpfs_winfsp/winfsp.hpp>
#include <redx/core/windowz.hpp>

//#define PRINT_CALL_ARGS

struct win_handle
{
  win_handle() = default;

  ~win_handle()
  {
    if (m_handle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(m_handle);
      m_handle = INVALID_HANDLE_VALUE;
    }
  }

  win_handle(const win_handle&) = delete;

  win_handle(win_handle&& other) noexcept
    : m_handle(INVALID_HANDLE_VALUE)
  {
    swap(other);
  }

  explicit win_handle(HANDLE h) noexcept
    : m_handle(h) {}

  win_handle& operator=(const win_handle&) = delete;

  win_handle& operator=(win_handle&& other) noexcept
  {
    swap(other);
    return *this;
  }

  void reset(HANDLE h)
  {
    *this = win_handle(h);
  }

  void reset()
  {
    *this = win_handle();
  }

  HANDLE get() const
  {
    return m_handle;
  }

  void swap(win_handle& other) noexcept
  {
    std::swap(m_handle, other.m_handle);
  }

  operator bool() const
  {
    return m_handle != INVALID_HANDLE_VALUE;
  }

private:

  HANDLE m_handle = INVALID_HANDLE_VALUE;
};

inline cpfs* fs_from_ffs(FSP_FILE_SYSTEM* FileSystem)
{
  return reinterpret_cast<cpfs*>(FileSystem->UserContext);
}


void fill_fsp_info(FSP_FSCTL_FILE_INFO& fsp_finfo, const redx::filesystem::directory_entry& de)
{
  fsp_finfo.FileAttributes  = de.is_file() ? FILE_ATTRIBUTE_ARCHIVE : FILE_ATTRIBUTE_DIRECTORY;

  //fsp_finfo.ReparseTag      = 0;

  const auto& finfo         = de.get_file_info();

  fsp_finfo.AllocationSize  = (finfo.size + 4096) / 4096 * 4096;//finfo.disk_size;
  fsp_finfo.FileSize        = finfo.size;
  
  fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch;
  fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch;
  fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch;
  fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch;

  //fsp_finfo.IndexNumber     = 0;
  //fsp_finfo.HardLinks       = 0;
  //fsp_finfo.EaSize          = 0;
}

// cp depot paths are ascii-only, let's avoid the overhead of converters
void set_fsp_dir_info_ascii_name(FSP_FSCTL_DIR_INFO& dir_info, std::string_view name)
{
  dir_info.Size = static_cast<UINT16>(offsetof(FSP_FSCTL_DIR_INFO, FileNameBuf) + name.length() * 2);
  std::copy(name.begin(), name.end(), dir_info.FileNameBuf);
}

void set_fsp_dir_info_wname(FSP_FSCTL_DIR_INFO& dir_info, std::wstring_view wname)
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

  ~file_context()
  {
    if (dir_buffer != nullptr)
    {
      FspFileSystemDeleteDirectoryBuffer(&dir_buffer);
    }
  }

  file_context(const file_context&) = delete;
  file_context& operator=(const file_context&) = delete;

  NTSTATUS fill_fsp_info(FSP_FSCTL_FILE_INFO& fsp_finfo)
  {
    fsp_finfo = {};

    if (is_tfs_file)
    {
      ::fill_fsp_info(fsp_finfo, dirent);
    }
    else
    {
      BY_HANDLE_FILE_INFORMATION finfo{};
      if (!GetFileInformationByHandle(handle.get(), &finfo))
      {
        DWORD dwErr = GetLastError();
        SPDLOG_ERROR("GetFileInformationByHandle: {}", redx::windowz::format_error(dwErr));
        return FspNtStatusFromWin32(dwErr);
      }

      FILE_STANDARD_INFO fstdinfo{};
      if (!GetFileInformationByHandleEx(handle.get(), FileStandardInfo, &fstdinfo, sizeof(fstdinfo)))
      {
        DWORD dwErr = GetLastError();
        SPDLOG_ERROR("GetFileInformationByHandleEx: {}", redx::windowz::format_error(dwErr));
        return FspNtStatusFromWin32(dwErr);
      }

      fsp_finfo.FileAttributes  = finfo.dwFileAttributes;

      fsp_finfo.FileSize        = fstdinfo.EndOfFile.QuadPart;
      fsp_finfo.AllocationSize  = fstdinfo.AllocationSize.QuadPart;

      fsp_finfo.CreationTime    = filetime_to_fsp_time(finfo.ftCreationTime);
      fsp_finfo.LastAccessTime  = filetime_to_fsp_time(finfo.ftLastAccessTime);
      fsp_finfo.LastWriteTime   = filetime_to_fsp_time(finfo.ftLastWriteTime);
      fsp_finfo.ChangeTime      = fsp_finfo.LastWriteTime;
    }

    return STATUS_SUCCESS;
  }


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

  std::wstring wrel_path;

  bool is_tfs_file = false;
  redx::filesystem::directory_entry dirent;
  redx::archive_file_stream afs;
  mutable std::mutex m_afs_mtx;
  bool m_symlink = false;

  bool is_diff_only = false; // path cannot be converted to a cp path
  win_handle handle;
  
  PVOID dir_buffer = nullptr;
  std::unordered_set<redx::filesystem::path_id> overridden_pids;
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
  //scope_timer stimr("GetSecurityByName");

#ifdef PRINT_CALL_ARGS
  wprintf(L"GetSecurityByName %s\n", FileName);
#endif

  cpfs* fs = fs_from_ffs(FileSystem);

  std::wstring_view wfilepath = FileName;

  bool tfs_compatible{};
  redx::filesystem::path tfs_path(wfilepath, tfs_compatible);

  win_handle fh;

  if (fs->has_diffdir)
  {
    auto diff_path = fs->diffdir_path / FileName;

    fh = win_handle(
      CreateFileW(
        diff_path.c_str(),
        FILE_READ_ATTRIBUTES | READ_CONTROL, 0, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0));

    DWORD dwErr = GetLastError();

    if (fh)
    {
      // retrieve file attributes

      if (PFileAttributes != nullptr)
      {
        FILE_ATTRIBUTE_TAG_INFO atinfo;
        if (!GetFileInformationByHandleEx(fh.get(), FileAttributeTagInfo, &atinfo, sizeof(atinfo)))
        {
          dwErr = GetLastError();
          SPDLOG_ERROR("couldn't get file info; {}", redx::windowz::format_error(dwErr));
          return FspNtStatusFromWin32(dwErr);
        }
        *PFileAttributes = atinfo.FileAttributes;
      }

      // retrieve security attributes

      if (PSecurityDescriptorSize != nullptr)
      {
        DWORD security_desc_size;
        if (!GetKernelObjectSecurity(
          fh.get(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
          SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &security_desc_size))
        {
          *PSecurityDescriptorSize = security_desc_size;
          dwErr = GetLastError();
          SPDLOG_ERROR("couldn't get file security; {}", redx::windowz::format_error(dwErr));
          return FspNtStatusFromWin32(dwErr);
        }
        *PSecurityDescriptorSize = security_desc_size;
      }

      return STATUS_SUCCESS;
    }
    else if (dwErr != ERROR_FILE_NOT_FOUND && dwErr != ERROR_PATH_NOT_FOUND)
    {
      SPDLOG_ERROR(
        "couldn't open diff file {}; reason: ({}){}",
        diff_path.string(), dwErr, redx::windowz::format_error(dwErr)
      );
      return FspNtStatusFromWin32(dwErr);;
    }
  }

  // else if we are not using diff dir or file wasn't found in there
  // we now look into the tfs

  if (tfs_compatible)
  {
    redx::filesystem::directory_entry entry(fs->tfs, tfs_path);
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
  cpfs* fs = fs_from_ffs(FileSystem);
  auto rel_path = std::filesystem::weakly_canonical(FileName).relative_path();

  SPDLOG_DEBUG("Create(.., \"{}\", ..)", rel_path.string());

  if (!fs->has_diffdir)
  {
    // todo: check that it conforms with the standard
    return STATUS_ACCESS_DENIED;
  }

  // we want to prevent overrides of files with directories
  // todo: let's wait to see if WinFSP does this for us

  auto diff_path = fs->diffdir_path / rel_path;

  file_context* fctx = new file_context();

  return STATUS_ACCESS_DENIED;

  SECURITY_ATTRIBUTES SecurityAttributes;
  SecurityAttributes.nLength = sizeof(SecurityAttributes);
  SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
  SecurityAttributes.bInheritHandle = FALSE;

  ULONG CreateFlags;

  CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;

  if (CreateOptions & FILE_DELETE_ON_CLOSE)
  {
    CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;
  }

  if (CreateOptions & FILE_DIRECTORY_FILE)
  {
    CreateFlags |= FILE_FLAG_POSIX_SEMANTICS;
    FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
  }
  else
  {
    FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
  }

  if (!FileAttributes)
  {
    FileAttributes = FILE_ATTRIBUTE_NORMAL;
  }

  // now try create

  fctx->handle = win_handle(
    CreateFileW(
      diff_path.c_str(), GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      &SecurityAttributes, CREATE_NEW, CreateFlags | FileAttributes, 0));

  // if failed to open, destroy context and return error
  if (!fctx->handle)
  {
    delete fctx;
    fctx = nullptr;
    DWORD dwErr = GetLastError();
    SPDLOG_ERROR("CreateFileW: {}", redx::windowz::format_error(dwErr));
    return FspNtStatusFromWin32(dwErr);
  }

  *PFileContext = fctx;

  if (FileInfo != nullptr)
  {
    return fctx->fill_fsp_info(*FileInfo);
  }

  return STATUS_SUCCESS;
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
  redx::filesystem::path tfs_path(wfilepath, tfs_compatible);

  //SPDLOG_DEBUG("Open(.., \"{}\", ..)", tfs_path.strv());

  file_context* fctx = new file_context();
  fctx->wrel_path = wfilepath.substr(1);
  fctx->is_diff_only = !tfs_compatible;

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
  else if (tfs_compatible)
  {
    // check in tfs
    fctx->dirent.assign(fs->tfs, tfs_path);

    if (fctx->dirent.is_file())
    {
      fctx->afs.open(fs->tfs.get_file_handle(fctx->dirent.pid()));
      assert(fctx->afs.is_open());
    }

    if (!fctx->dirent.exists() || !(fctx->dirent.is_file() || fctx->dirent.is_directory()))
    {
      delete fctx;
      fctx = nullptr;

      SPDLOG_DEBUG("\"{}\" not found", tfs_path.strv());

      // TODO: same as in GetSecurityByName
      return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    fctx->is_tfs_file = true;
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
    if (fctx->dirent.is_pidlink() && (CreateOptions & FILE_OPEN_REPARSE_POINT))
    {
      fctx->m_symlink = true;

      auto& fsp_finfo = *FileInfo;
      fsp_finfo = {};

      fsp_finfo.FileAttributes  = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ARCHIVE;
      
      fsp_finfo.ReparseTag      = IO_REPARSE_TAG_SYMLINK;
      
      const auto& finfo         = fctx->dirent.get_file_info();
      
      fsp_finfo.AllocationSize  = 0;
      fsp_finfo.FileSize        = 0;
      
      fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch;
      fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch;
      fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch;
      fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch;
    }
    else
    {
      return fctx->fill_fsp_info(*FileInfo);
    }
  }

  return STATUS_SUCCESS;
}

NTSTATUS Overwrite(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  cpfs* fs = fs_from_ffs(FileSystem);
  auto fctx = reinterpret_cast<file_context*>(FileContext);

  if (fctx->is_tfs_file)
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
  }
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

  if (fctx->is_tfs_file)
  {
    if (fctx->dirent.is_file())
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
          std::scoped_lock<std::mutex> sl(fctx->m_afs_mtx);
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
    
    return STATUS_INVALID_HANDLE;
  }
  else
  {
    // TODO: ASYNC IO

    OVERLAPPED Overlapped = { 0 };

    Overlapped.Offset = (DWORD)Offset;
    Overlapped.OffsetHigh = (DWORD)(Offset >> 32);

    if (!ReadFile(fctx->handle.get(), Buffer, Length, PBytesTransferred, &Overlapped))
    {
      DWORD dwErr = GetLastError();
      SPDLOG_ERROR("ReadFile: {}", redx::windowz::format_error(dwErr));
      return FspNtStatusFromWin32(dwErr);
    }
  }

  return STATUS_SUCCESS;
}

static NTSTATUS Write(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
  BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
  PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);

  if (fctx->is_tfs_file)
  {
    SPDLOG_ERROR("trying to write an archive file");
    return STATUS_ACCESS_DENIED;
  }
  else
  {
    // TODO: ASYNC IO

    const HANDLE Handle = fctx->handle.get();
    LARGE_INTEGER FileSize;
    OVERLAPPED Overlapped = { 0 };

    if (ConstrainedIo)
    {
      if (!GetFileSizeEx(Handle, &FileSize))
      {
        DWORD dwErr = GetLastError();
        SPDLOG_ERROR("GetFileSizeEx: {}", redx::windowz::format_error(dwErr));
        return FspNtStatusFromWin32(dwErr);
      }

      if (Offset >= (UINT64)FileSize.QuadPart)
        return STATUS_SUCCESS;
      if (Offset + Length > (UINT64)FileSize.QuadPart)
        Length = (ULONG)((UINT64)FileSize.QuadPart - Offset);
    }

    Overlapped.Offset = (DWORD)Offset;
    Overlapped.OffsetHigh = (DWORD)(Offset >> 32);

    if (!WriteFile(Handle, Buffer, Length, PBytesTransferred, &Overlapped))
    {
      DWORD dwErr = GetLastError();
      SPDLOG_ERROR("WriteFile: {}", redx::windowz::format_error(dwErr));
      return FspNtStatusFromWin32(dwErr);
    }

    return fctx->fill_fsp_info(*FileInfo);
  }

  return STATUS_SUCCESS;
}

NTSTATUS Flush(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);

  if (fctx->is_tfs_file)
  {
    SPDLOG_ERROR("trying to write an archive file");
    return STATUS_ACCESS_DENIED;
  }
  else
  {
    const HANDLE Handle = fctx->handle.get();

    /* we do not flush the whole volume, so just return SUCCESS */
    if (Handle == 0)
    {
      return STATUS_SUCCESS;
    }

    if (!FlushFileBuffers(Handle))
    {
      DWORD dwErr = GetLastError();
      SPDLOG_ERROR("FlushFileBuffers: {}", redx::windowz::format_error(dwErr));
      return FspNtStatusFromWin32(dwErr);
    }

    return fctx->fill_fsp_info(*FileInfo);
  }

  return STATUS_SUCCESS;
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

  NTSTATUS ret = fctx->fill_fsp_info(*FileInfo);

  if (fctx->m_symlink)
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
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (fctx->is_tfs_file)
  {
    return STATUS_ACCESS_DENIED;
  }
  else
  {
    const HANDLE Handle = fctx->handle.get();
    FILE_BASIC_INFO BasicInfo{};

    if (FileAttributes == INVALID_FILE_ATTRIBUTES)
    {
      FileAttributes = 0;
    }
    else if (FileAttributes == 0)
    {
      FileAttributes = FILE_ATTRIBUTE_NORMAL;
    }

    BasicInfo.FileAttributes = FileAttributes;
    BasicInfo.CreationTime.QuadPart = CreationTime;
    BasicInfo.LastAccessTime.QuadPart = LastAccessTime;
    BasicInfo.LastWriteTime.QuadPart = LastWriteTime;
    //BasicInfo.ChangeTime = ChangeTime;

    if (!SetFileInformationByHandle(Handle, FileBasicInfo, &BasicInfo, sizeof BasicInfo))
    {
      DWORD dwErr = GetLastError();
      SPDLOG_ERROR("SetFileInformationByHandle: {}", redx::windowz::format_error(dwErr));
      return FspNtStatusFromWin32(dwErr);
    }

    return fctx->fill_fsp_info(*FileInfo);
  }

  return STATUS_SUCCESS;
}

NTSTATUS SetFileSize(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
  FSP_FSCTL_FILE_INFO* FileInfo)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (fctx->is_tfs_file)
  {
    return STATUS_ACCESS_DENIED;
  }
  else
  {
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

  return fctx->fill_fsp_info(*FileInfo);
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

  if (fctx->is_tfs_file)
  {
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
  }

  return STATUS_SUCCESS;
}

NTSTATUS SetSecurity(
  FSP_FILE_SYSTEM* FileSystem,
  PVOID FileContext,
  SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
  auto fctx = reinterpret_cast<file_context*>(FileContext);
  if (!fctx)
  {
    SPDLOG_ERROR("fctx is null!");
    return STATUS_INVALID_HANDLE;
  }

  if (fctx->is_tfs_file)
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

  return STATUS_SUCCESS;
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

  const bool has_diffdir = fs->has_diffdir;

  const redx::filesystem::directory_entry& dirent = fctx->dirent;

  bool read_tfs = dirent.is_directory();

  if (!read_tfs && !fs->has_diffdir)
  {
    SPDLOG_ERROR("read dir called on non-dir");
    return STATUS_NOT_A_DIRECTORY;
  }

  // windows folder copy does check each file by doing a read dir with filename as the pattern..
  // in this case it is way faster to only return only one entry
  bool single_entry = false;
  bool use_marker_for_tfs = false;
  bool tfs_marker_is_pattern = false;
  std::string tfs_marker;

  // note: the driver will do a pattern check itself too
  // can we do something fast enough that it is worth it.. ?
  if (Pattern != NULL)
  {
    if (read_tfs)
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
        use_marker_for_tfs = converted;
        read_tfs = converted; // if pattern isn't tfs compatible, disable tfs listing
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

    if (read_tfs)
    {
      bool converted{};
      tfs_marker = ws_to_ascii(Marker, converted);
      use_marker_for_tfs = converted;
      read_tfs = converted; // if pattern isn't tfs compatible, disable tfs listing
    }
  }
  else
  {
    // fresh search

    if (has_diffdir)
    {
      // todo: prepare a fctx->dir_buffer and fctx->overridden_pids

      //auto diff_path = fs->diffdir_path / rel_path;
      //NTSTATUS Status = STATUS_SUCCESS;
  //if (!FspFileSystemAcquireDirectoryBuffer(&fctx->dir_buffer, Marker == 0, &Status))
  //{
  //  SPDLOG_ERROR("FspFileSystemAcquireDirectoryBuffer failed ({})", Status);
  //  return Status;
  //}
  /*if (Pattern == 0)
        Pattern = L"*";
      PatternLength = (ULONG)wcslen(Pattern);

      Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
      if (0 == Length)
      {
        DirBufferResult = FspNtStatusFromWin32(GetLastError());
      }
      else if (Length + 1 + PatternLength >= FULLPATH_SIZE)
      {
        DirBufferResult = STATUS_OBJECT_NAME_INVALID;
      }

      if (!NT_SUCCESS(DirBufferResult))
      {
        FspFileSystemReleaseDirectoryBuffer(&FileContext->DirBuffer);
        return DirBufferResult;
      }

      if (L'\\' != FullPath[Length - 1])
        FullPath[Length++] = L'\\';
      memcpy(FullPath + Length, Pattern, PatternLength * sizeof(WCHAR));
      FullPath[Length + PatternLength] = L'\0';

      FindHandle = FindFirstFileW(FullPath, &FindData);
      if (INVALID_HANDLE_VALUE != FindHandle)
      {
        do
        {
          memset(DirInfo, 0, sizeof * DirInfo);
          Length = (ULONG)wcslen(FindData.cFileName);
          DirInfo->Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + Length * sizeof(WCHAR));
          DirInfo->FileInfo.FileAttributes = FindData.dwFileAttributes;
          DirInfo->FileInfo.ReparseTag = 0;
          DirInfo->FileInfo.FileSize =
            ((UINT64)FindData.nFileSizeHigh << 32) | (UINT64)FindData.nFileSizeLow;
          DirInfo->FileInfo.AllocationSize = (DirInfo->FileInfo.FileSize + ALLOCATION_UNIT - 1)
            / ALLOCATION_UNIT * ALLOCATION_UNIT;
          DirInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&FindData.ftCreationTime)->QuadPart;
          DirInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&FindData.ftLastAccessTime)->QuadPart;
          DirInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&FindData.ftLastWriteTime)->QuadPart;
          DirInfo->FileInfo.ChangeTime = DirInfo->FileInfo.LastWriteTime;
          DirInfo->FileInfo.IndexNumber = 0;
          DirInfo->FileInfo.HardLinks = 0;
          memcpy(DirInfo->FileNameBuf, FindData.cFileName, Length * sizeof(WCHAR));

          if (!FspFileSystemFillDirectoryBuffer(&FileContext->DirBuffer, DirInfo, &DirBufferResult))
            break;
        } while (FindNextFileW(FindHandle, &FindData));

        FindClose(FindHandle);
      }*/
      /*
  HANDLE FindHandle;
  WIN32_FIND_DATAW FindData;
  auto pp = std::filesystem::path(L"D:\\Desktop\\cpsavedit\\BUF_FILES\\big_folder_test") / fctx->rel_path;

  HANDLE Handle = CreateFileW(pp.c_str(),
        FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG Length, PatternLength;
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
    union
    {
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.D;
    NTSTATUS DirBufferResult;

    DirBufferResult = STATUS_SUCCESS;
    if (FspFileSystemAcquireDirectoryBuffer(&fctx->dir_buffer, 0 == Marker, &DirBufferResult))
    {
        if (0 == Pattern)
            Pattern = (PWSTR)L"*";
        PatternLength = (ULONG)wcslen(Pattern);

        Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
        if (0 == Length)
            DirBufferResult = FspNtStatusFromWin32(GetLastError());
        else if (Length + 1 + PatternLength >= FULLPATH_SIZE)
            DirBufferResult = STATUS_OBJECT_NAME_INVALID;
        if (!NT_SUCCESS(DirBufferResult))
        {
            FspFileSystemReleaseDirectoryBuffer(&fctx->dir_buffer);
            return DirBufferResult;
        }

        if (L'\\' != FullPath[Length - 1])
            FullPath[Length++] = L'\\';
        memcpy(FullPath + Length, Pattern, PatternLength * sizeof(WCHAR));
        FullPath[Length + PatternLength] = L'\0';

        FindHandle = FindFirstFileW(FullPath, &FindData);
        if (INVALID_HANDLE_VALUE != FindHandle)
        {
            do
            {
                memset(DirInfo, 0, sizeof *DirInfo);
                Length = (ULONG)wcslen(FindData.cFileName);
                DirInfo->Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + Length * sizeof(WCHAR));
                DirInfo->FileInfo.FileAttributes = FindData.dwFileAttributes;
                DirInfo->FileInfo.ReparseTag = 0;
                DirInfo->FileInfo.FileSize =
                    ((UINT64)FindData.nFileSizeHigh << 32) | (UINT64)FindData.nFileSizeLow;
                DirInfo->FileInfo.AllocationSize = (DirInfo->FileInfo.FileSize + 4096 - 1)
                    / 4096 * 4096;
                DirInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&FindData.ftCreationTime)->QuadPart;
                DirInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&FindData.ftLastAccessTime)->QuadPart;
                DirInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&FindData.ftLastWriteTime)->QuadPart;
                DirInfo->FileInfo.ChangeTime = DirInfo->FileInfo.LastWriteTime;
                DirInfo->FileInfo.IndexNumber = 0;
                DirInfo->FileInfo.HardLinks = 0;
                memcpy(DirInfo->FileNameBuf, FindData.cFileName, Length * sizeof(WCHAR));

                if (!FspFileSystemFillDirectoryBuffer(&fctx->dir_buffer, DirInfo, &DirBufferResult))
                    break;
            } while (FindNextFileW(FindHandle, &FindData));

            FindClose(FindHandle);
        }

        FspFileSystemReleaseDirectoryBuffer(&fctx->dir_buffer);
    }

    if (!NT_SUCCESS(DirBufferResult))
        return DirBufferResult;

    FspFileSystemReadDirectoryBuffer(&fctx->dir_buffer,
        Marker, Buffer, BufferLength, PBytesTransferred);

    return STATUS_SUCCESS;*/
    }
  }

  size_t added_cnt = 0;
  bool buffer_is_full = false;

  if (read_tfs)
  {
    const redx::filesystem::directory_entry& dirent = fctx->dirent;

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

    redx::filesystem::directory_entry iter_dirent;

    if (use_marker_for_tfs)
    {
      redx::filesystem::path marker_path = dirent.tfs_path() / tfs_marker;

      SPDLOG_DEBUG("dir:{} marker_path:{}", fctx->dirent.tfs_path().strv(), marker_path.strv());

      iter_dirent.assign(fs->tfs, marker_path);

      if (iter_dirent.is_pidlink())
      {
        // special behavior with pidlinks
      
        dir_info_buf.fill(0);
        set_fsp_dir_info_ascii_name(*dir_info, tfs_marker);
        
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
        
        fsp_finfo.CreationTime    = finfo.time.hns_since_win_epoch;
        fsp_finfo.LastAccessTime  = finfo.time.hns_since_win_epoch;
        fsp_finfo.LastWriteTime   = finfo.time.hns_since_win_epoch;
        fsp_finfo.ChangeTime      = finfo.time.hns_since_win_epoch;
      
        //fsp_finfo.IndexNumber     = 0;
        //fsp_finfo.HardLinks       = 0;
        //fsp_finfo.EaSize          = 0;
      
        buffer_is_full = !FspFileSystemAddDirInfo(dir_info, Buffer, Length, PBytesTransferred);
      
        ++added_cnt;
        single_entry = true;
      
        iter_dirent = redx::filesystem::directory_entry();
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
      set_fsp_dir_info_ascii_name(*dir_info, iter_dirent.filename_strv());
      fill_fsp_info(dir_info->FileInfo, iter_dirent);

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

  if (!buffer_is_full && !(single_entry && added_cnt > 0) && has_diffdir)
  {
    // todo
  }

  if (!buffer_is_full)
  {
    FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
  }

  SPDLOG_DEBUG("added {} entries, buffer_is_full:{} single_entry:{} use_marker_for_tfs:{}",
    added_cnt, buffer_is_full, single_entry, use_marker_for_tfs);

  return STATUS_SUCCESS;

  //if (Marker == 0 || (L'.' == Marker[0] && L'\0' == Marker[1]))

  

  //wcscpy_s(dir_info->FileNameBuf, MAX_PATH, L".");
  //dir_info->Size = sizeof(FSP_FSCTL_DIR_INFO) + 2;


  //fill_fsp_info(dir_info->FileInfo, dirent);
  //if (!FspFileSystemFillDirectoryBuffer(&fctx->dir_buffer, dir_info, &Status))
  //{
  //  SPDLOG_ERROR("FspFileSystemFillDirectoryBuffer failed");
  //  goto early_stop;
  //}
  //
  //if (path.has_parent_path())
  //{
  //  redx::filesystem::directory_entry parent_dirent(fs->tfs, path.parent_path().string());
  //  if (parent_dirent.is_directory())
  //  {
  //    memset(dir_info, 0, sizeof(FSP_FSCTL_DIR_INFO));
  //    wcscpy(dir_info->FileNameBuf, L"..");
  //    dir_info->Size = sizeof(FSP_FSCTL_DIR_INFO) + 4;
  //    fill_fsp_info(dir_info->FileInfo, parent_dirent);
  //    if (!FspFileSystemFillDirectoryBuffer(&fctx->dir_buffer, dir_info, &Status))
  //    {
  //      SPDLOG_ERROR("FspFileSystemFillDirectoryBuffer failed");
  //      goto early_stop;
  //    }
  //  }
  //}

  //redx::filesystem::directory_iterator dirit(fs->tfs, fctx->rel_path);
  //for (const auto& it : dirit)
  //{
  //  if (it.is_file() || it.is_directory())
  //  {
  //    memset(dir_info, 0, sizeof(FSP_FSCTL_DIR_INFO));

  //    auto filename = it.filename(); // TODO: add name accessor to dirent

  //    int len = MultiByteToWideChar(CP_UTF8, 0, filename.data(), int(filename.size()), dir_info->FileNameBuf, int(MAX_PATH));
  //    if (len <= 0)
  //    {
  //      SPDLOG_ERROR("MultiByteToWideChar failed");
  //      goto early_stop;
  //    }
  //    dir_info->FileNameBuf[len] = 0;

  //    // todo: check len
  //    dir_info->Size = sizeof(FSP_FSCTL_DIR_INFO) + UINT16(2 * len);
  //    fill_fsp_info(dir_info->FileInfo, it);
  //    if (!FspFileSystemFillDirectoryBuffer(&fctx->dir_buffer, dir_info, &Status))
  //    {
  //      SPDLOG_INFO("!FspFileSystemFillDirectoryBuffer");
  //      goto early_stop;
  //    }
  //  }
  //}

  //early_stop:
  //
  //  FspFileSystemReleaseDirectoryBuffer(&fctx->dir_buffer);
  //
  //  if (!NT_SUCCESS(Status))
  //    return Status;

  //FspFileSystemReadDirectoryBuffer(&fctx->dir_buffer, Marker, Buffer, BufferLength, PBytesTransferred);
  
}

bool try_fill_symlink_reparse_data(const redx::filesystem::directory_entry& dirent, void* buffer, size_t* inout_size)
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
  redx::filesystem::path tfs_path(wfilepath, tfs_compatible);

  if (tfs_compatible)
  {
    redx::filesystem::directory_entry dirent(fs->tfs, tfs_path);
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


  if (fctx->is_tfs_file && fctx->dirent.is_pidlink())
  {
    if (!try_fill_symlink_reparse_data(fctx->dirent, Buffer, PSize))
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






