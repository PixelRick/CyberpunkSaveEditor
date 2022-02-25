#pragma once
#define WIN32_NO_STATUS
#define NOMINMAX
#include <winfsp/winfsp.h>
#include <sddl.h>

#include <redx/core.hpp>
#include <redx/os/platform_utils.hpp>

struct perf_timer
{
  perf_timer() noexcept = default;

  void start()
  {
    QueryPerformanceFrequency(&m_frequency); 
    QueryPerformanceCounter(&m_start);
  }

  double elapsed_seconds()
  {
    LARGE_INTEGER li = {};
    QueryPerformanceCounter(&li);
    li.QuadPart -= m_start.QuadPart;
    li.QuadPart *= 1000000;
    li.QuadPart /= m_frequency.QuadPart;
    return double(li.QuadPart) / 1000000.f;
  }

private:

  LARGE_INTEGER m_start;
  LARGE_INTEGER m_frequency;
};

struct security_desc
{
  security_desc() = default;

  ~security_desc()
  {
    clear();
  }

  security_desc(const std::string& sddl)
  {
    set_sddl(sddl);
  }

  void clear()
  {
    if (m_psecdesc)
    {
      LocalFree(m_psecdesc);
      m_psecdesc = nullptr;
      m_secdesc_size = 0;
    }
  }

  // root: "O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";
  bool set_sddl(const std::string& sddl)
  {
    clear();

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, &m_psecdesc, &m_secdesc_size))
    {
      SPDLOG_ERROR("security_desc::set_sddl: ", redx::os::last_error_string());
      return false;
    }

    return true;
  }

  PSECURITY_DESCRIPTOR get() const
  {
    return m_psecdesc;
  }

  ULONG size() const
  {
    return m_secdesc_size;
  }

private:

  PSECURITY_DESCRIPTOR m_psecdesc = nullptr;
  ULONG m_secdesc_size = 0;
};

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

inline constexpr uint64_t filetime_to_winfsp_time(const FILETIME& ft)
{
  return ULARGE_INTEGER{ft.dwLowDateTime, ft.dwHighDateTime}.QuadPart;
}

inline constexpr LARGE_INTEGER winfsp_time_to_largeinteger(uint64_t fspt)
{
  LARGE_INTEGER tmp{};
  tmp.QuadPart = fspt;
  return tmp;
}

bool set_file_times(HANDLE file_handle, redx::file_time creation, redx::file_time last_access, redx::file_time last_write, redx::file_time change);

struct winfsp_directory_buffer
{
  winfsp_directory_buffer() noexcept = default;

  ~winfsp_directory_buffer()
  {
    if (m_buffer != NULL)
    {
      FspFileSystemDeleteDirectoryBuffer(&m_buffer);
      m_buffer = NULL;
    }
  }

  inline void fill(PWSTR marker, PVOID buffer, ULONG length, PULONG p_bytes_transferred)
  {
    FspFileSystemReadDirectoryBuffer(&m_buffer, marker, buffer, length, p_bytes_transferred);
  }

  NTSTATUS acquire(bool reset)
  {
    NTSTATUS status;
    if (!FspFileSystemAcquireDirectoryBuffer(&m_buffer, reset, &status))
    {
      SPDLOG_ERROR("FspFileSystemAcquireDirectoryBuffer failed ({})", status);
    }
    return status;
  }

  inline void release()
  {
    FspFileSystemReleaseDirectoryBuffer(&m_buffer);
  }
 
  inline NTSTATUS append(FSP_FSCTL_DIR_INFO& dir_info)
  {
    NTSTATUS status;
    FspFileSystemFillDirectoryBuffer(&m_buffer, &dir_info, &status);
    return status;
  }

  inline void read(PWSTR marker, PVOID buffer, ULONG length, PULONG p_bytes_transferred)
  {
    FspFileSystemReadDirectoryBuffer(&m_buffer, marker, buffer, length, p_bytes_transferred);
  }

private:

  PVOID m_buffer = NULL;
};

