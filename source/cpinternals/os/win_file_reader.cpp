#include <cpinternals/os/file_reader.hpp>
#include <cpinternals/os/platform_utils.hpp>

#include <fileapi.h>

#include <filesystem>
#include <memory>

#include <cpinternals/common.hpp>

namespace cp::os {

struct win_file_reader
  : file_reader_impl
{
  ~win_file_reader() override
  {
    close();
  }

  bool open(const std::filesystem::path& p) override
  {
    if (is_open())
    {
      return false;
    }

    m_h = CreateFileW(
      p.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    return is_open();
  }

  bool is_open() const override
  {
    return m_h != INVALID_HANDLE_VALUE;
  }

  bool seek(size_t offset) override
  {
    if (!is_open())
    {
      return false;
    }

    LARGE_INTEGER lioff{}, out_lioff{};
    lioff.QuadPart = offset;
    bool ok = SetFilePointerEx(m_h, lioff, &out_lioff, FILE_BEGIN);

    if (!ok)
    {
      SPDLOG_ERROR("SetFilePointerEx failed: {}", os::last_error_string());
    }

    return ok && (lioff.QuadPart == out_lioff.QuadPart);
  }

  bool read(std::span<char> dst) override
  {
    if (!is_open())
    {
      SPDLOG_ERROR("!is_open()");
      return false;
    }

    constexpr size_t max_size = std::numeric_limits<DWORD>::max();

    DWORD read_cnt = 0;

    while (dst.size())
    {
      DWORD read_size = static_cast<DWORD>(std::min(dst.size(), max_size));

      size_t retries = 10;
      while (!ReadFile(m_h, dst.data(), read_size, &read_cnt, 0) && --retries)
      {
        DWORD dwErr = GetLastError();
        if (dwErr != ERROR_NOT_ENOUGH_MEMORY)
        {
          SPDLOG_ERROR("ReadFile failed: {}", os::format_error(dwErr));
          return false;
        }

        SPDLOG_WARN("ReadFile failed with ERROR_NOT_ENOUGH_MEMORY, retrying..", os::format_error(dwErr));
      }

      if (!retries)
      {
        SPDLOG_ERROR("ReadFile failed too many times");
        return false;
      }

      if (read_cnt != read_size)
      {
        SPDLOG_ERROR("ReadFile EOF");
        return false;
      }

      dst = dst.subspan(read_size);
    }

    return true;
  }

  bool close() override
  {
    if (is_open())
    {
      CloseHandle(m_h);
      m_h = INVALID_HANDLE_VALUE;
      return true;
    }

    return false;
  }

private:

  HANDLE m_h = INVALID_HANDLE_VALUE;
};


file_reader::file_reader()
{
  m_impl = std::make_unique<win_file_reader>();
}

file_reader::~file_reader()
{
}

} // namespace cp::os

