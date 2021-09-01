#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>
#include <fileapi.h>

#include <filesystem>
#include <memory>

#include <redx/core.hpp>
#include <redx/io/file_access.hpp>
#include <redx/os/platform_utils.hpp>

namespace redx {

// this keeps a local position in sync with the kernel object's
// to spare a few SetFilePointer calls
struct file_access_win
  : std_file_access
{
  file_access_win() noexcept
  {
    m_fptr.QuadPart = -1;
  }

  file_access_win(const std::filesystem::path& p, std::ios::openmode mode)
  {
    m_fptr.QuadPart = -1;
    open(p, mode);
  }

  ~file_access_win()
  {
    close();
  }

  bool is_open() const override
  {
    return m_h != INVALID_HANDLE_VALUE;
  }

  bstreampos tell() const override
  {
    return std::streamsize(m_fptr.QuadPart);
  }

  std::streamsize size() const override
  {
    LARGE_INTEGER fsize{};
    fsize.QuadPart = -1;

    if (is_open() && !GetFileSizeEx(m_h, &fsize))
    {
      SPDLOG_ERROR("GetFileSizeEx failed");
      return std::streamsize(-1);
    }

    return reliable_numeric_cast<std::streamsize>(fsize.QuadPart);
  }

  bool seekpos(bstreampos pos) override
  {
    return is_open() && set_file_pointer(pos);
  }

  // read max_count bytes (or until EOF)
  inline bool read(char* dst, size_t max_count, size_t& read_size) override
  {
    read_size = 0;

    if (!is_open() || !(m_mode & std::ios::in))
    {
      return false;
    }

    while (max_count > 0)
    {
      DWORD dwcnt = static_cast<DWORD>(std::min(max_count, 0x40000000ull));
      DWORD rsize{};

      if (!ReadFile(m_h, dst, dwcnt, &rsize, nullptr))
      {
        SPDLOG_ERROR("error: {}", os::last_error_string());
        LARGE_INTEGER li0{};
        SetFilePointerEx(m_h, li0, &m_fptr, FILE_CURRENT); // because it's not clear what it becomes on error
        return false;
      }

      m_fptr.QuadPart += rsize;
      read_size += rsize;
      dst += rsize;
      max_count -= rsize;

      // EOF isn't an error
      if (rsize != dwcnt)
      {
        break;
      }
    }

    return true;
  }

  // write count bytes
  inline bool write(const char* src, size_t count, size_t& write_size) override
  {
    write_size = 0;

    if (!is_open() || !!(m_mode & std::ios::out))
    {
      return false;
    }

    // allow null writes (updates timestamp)
    if (count == 0)
    {
      DWORD wsize{};
      return WriteFile(m_h, nullptr, 0, &wsize, nullptr);
    }

    do
    {
      DWORD dwcnt = static_cast<DWORD>(std::min(count, 0x40000000ull));
      DWORD wsize{};

      if (!WriteFile(m_h, src, dwcnt, &wsize, nullptr))
      {
        SPDLOG_ERROR("error: {}", os::last_error_string());
        LARGE_INTEGER li0{};
        SetFilePointerEx(m_h, li0, &m_fptr, FILE_CURRENT); // because it's not clear what it becomes on error
        return false;
      }

      m_fptr.QuadPart += wsize;
      write_size += wsize;
      src += wsize;
      count -= wsize;

      if (wsize != dwcnt)
      {
        // is it always an error ?
        SPDLOG_ERROR("under-write");
        return false;
      }

    } while (count > 0);

    return true;
  }

  bool open_impl(const std::filesystem::path& p, std::ios::openmode mode) override
  {
    if (is_open())
    {
      return false;
    }

    m_mode = mode & (std::ios::in | std::ios::out | std::ios::trunc | std::ios::ate);

    if (m_mode != mode)
    {
      SPDLOG_ERROR("unsupported openmode:", int(mode ^ m_mode));
    }

    DWORD desired_access = FILE_READ_ATTRIBUTES;
    DWORD share_mode = 0;
    DWORD creation_disposition = OPEN_EXISTING; // r
    DWORD flags_and_attributes = FILE_ATTRIBUTE_NORMAL;

    if (mode & std::ios::in) // read
    {
      share_mode |= FILE_SHARE_READ;
      desired_access |= FILE_GENERIC_READ;

      if (mode & std::ios::out) // read-write
      {
        desired_access |= FILE_GENERIC_WRITE;

        if (mode & std::ios::trunc)
        {
          creation_disposition = CREATE_ALWAYS; // w+
        }
        else
        {
          creation_disposition = OPEN_ALWAYS; // r+
        }
      }
      else // read-only
      {
        //creation_disposition = OPEN_EXISTING; // r
      }
    }
    else if (mode & std::ios::out) // write-only
    {
      desired_access |= FILE_GENERIC_WRITE;
      creation_disposition = CREATE_ALWAYS; // w
    }

    m_h = CreateFileW(
      p.c_str(),
      desired_access,
      share_mode,
      nullptr,
      creation_disposition,
      flags_and_attributes,
      nullptr);

    if (!is_open())
    {
      return false;
    }

    if (mode & std::ios::ate)
    {
      if (!seekoff(0, std::ios::end))
      {
        SPDLOG_ERROR("couldn't satisfy openmode::ate, failed seek");
        return false;
      }
    }

    return true;
  }

  inline bool close() override
  {
    if (is_open())
    {
      CloseHandle(m_h);
      m_h = INVALID_HANDLE_VALUE;
      m_fptr.QuadPart = -1;
      return true;
    }

    return false;
  }

  static inline size_t get_best_buffer_size()
  {
    static size_t s_best_buffer_size = []() -> size_t {

      SYSTEM_INFO sysinf{};
      GetSystemInfo(&sysinf);
      return std::max(sysinf.dwAllocationGranularity, 0x1000ul);

    }();

    return s_best_buffer_size;
  }

private:

  // calls SetFilePointerEx only if needed
  // m_cur_pos is left unchanged
  // assumes is_open() == true
  inline bool set_file_pointer(size_t pos)
  {
    if (pos != m_fptr.QuadPart)
    {
      LARGE_INTEGER li{};
      li.QuadPart = pos;
      if (!SetFilePointerEx(m_h, li, &m_fptr, FILE_BEGIN))
      {
        SPDLOG_ERROR("error: {}", os::last_error_string());
      }
    }

    return true;
  }

  HANDLE m_h = INVALID_HANDLE_VALUE;
  std::ios::openmode m_mode;
  LARGE_INTEGER m_fptr;
};


// I did some tests about reading perfs using different techniques (on the 11GB archive segments).
// 
// Results with fresh cache (removed file from standby list)
// fread                                                    (segments)          : 36231 ms
// std::ifstream                                            (segments)          : 53833 ms
// std::ifstream with pubsetbuf(0, 0)                       (segments)          : 10746 ms -> goes to ~45s in some conditions..
// std::ifstream with pubsetbuf(p, alloc_granularity)       (segments)          : 44565 ms
// std::ifstream with pubsetbuf(p, alloc_granularity)       (sequential)        : 27718 ms
// ReadFile                                                 (segments)          : 27392 ms
// ReadFile with intermediate buffer                        (sequential)        : 27588 ms
// MapViewOfFile(+copy)                                     (segments)          : 54904 ms
//                                                                                      
// Results file already in standby list
// std::ifstream                                            (segments)          :  8444 ms
// std::ifstream with pubsetbuf(0, 0)                       (segments)          : 10996 ms
// std::ifstream with pubsetbuf(p, alloc_granularity)       (segments)          :  3611 ms
// std::ifstream with pubsetbuf(p, alloc_granularity)       (sequential)        :  2896 ms
// ReadFile                                                 (segments)          :  2547 ms
// ReadFile with intermediate buffer                        (sequential)        :  2631 ms
// MapViewOfFile(+copy)                                     (segments)          :  5705 ms
//
// Conclusion: probably better to impl our own file_bstreambuf on top of file_access
//

std::unique_ptr<std_file_access> std_file_access::create()
{
  return std::make_unique<file_access_win>();
}

std::unique_ptr<std_file_access> std_file_access::create(const std::filesystem::path& p, std::ios::openmode mode)
{
  return std::make_unique<file_access_win>(p, mode);
}

} // namespace redx

