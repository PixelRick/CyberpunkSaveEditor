#include <redx/archive/archive.hpp>

#include <fstream>
#include <set>

#include <redx/os/file_reader.hpp>
#include <redx/io/stdstream_wrapper.hpp>
#include <redx/io/memory_istream.hpp>
#include <redx/oodle/oodle.hpp>
//#include <redx/radr/fdesc.hpp>

namespace redx {

// check_for_corruption not implemented yet
std::shared_ptr<archive> archive::load(const std::filesystem::path& path)
{
  os::file_reader freader;

  freader.open(path);

  if (!freader.is_open())
  {
    SPDLOG_ERROR("archive file not found at {}", path.string());
    return nullptr;
  }

  bool ok{};

  redx::radr::header hdr;
  ok = freader.read(std::span<char>((char*)&hdr, sizeof(hdr))); // todo: reader stream wrapper
  if (!ok)
  {
    return nullptr;
  }

  if (!hdr.is_magic_ok())
  {
    SPDLOG_ERROR("archive file has wrong magic");
    return nullptr;
  }

  ok = freader.seek(hdr.metadata_offset);
  if (!ok)
  {
    return nullptr;
  }

  // buffering is disabled, let's fetch the whole metadata block

  auto metadata_block = std::make_unique<char[]>(hdr.metadata_size);
  std::span<char> metadata_block_span(metadata_block.get(), hdr.metadata_size);

  ok = freader.read(metadata_block_span);
  if (!ok)
  {
    return nullptr;
  }

  memory_istream stmeta(metadata_block_span);

  redx::radr::metadata md;
  md.serialize(stmeta, true);

  if (stmeta.has_error())
  {
    SPDLOG_ERROR("stream error, {}", stmeta.error());
    return nullptr;
  }

  return std::make_shared<archive>(path, std::move(md), std::move(freader), create_tag{});
}

archive::archive(const std::filesystem::path& p, radr::metadata&& md, os::file_reader&& freader, create_tag&&)
  : m_path(p)
  , m_freader(std::move(freader))
{
  std::swap(m_dependencies, md.dependencies);
  std::swap(m_segments, md.segments);

  m_records.reserve(md.records.size());
  for (auto& record: md.records)
  {
    m_records.emplace_back(record);
  }
}

bool archive::read_file(uint32_t idx, const std::span<char>& dst) const
{
  if (idx >= m_records.size())
  {
    SPDLOG_ERROR("idx out of range");
    return false;
  }

  const auto& rec = m_records[idx];

  if (rec.segs_irange.empty())
  {
    SPDLOG_ERROR("no segments");
    return false;
  }

  // read and decompress first chunks

  redx::radr::segment_descriptor sd0 = m_segments[rec.segs_irange.beg()];

  const size_t std0_size = sd0.size;

  if (std0_size > dst.size())
  {
    SPDLOG_ERROR("dst overflow");
    return false;
  }

  if (!read_segment(sd0, dst.subspan(0, std0_size), sd0.is_segment_compressed()))
  {
    SPDLOG_ERROR("couldn't decompress first segment");
    return false;
  }

  // read remaining chunks

  bool success = read_segments_raw(rec.segs_irange.subrange(1), dst.subspan(std0_size));

  return success;
}

archive::file_info archive::get_file_info(uint32_t index) const
{
  file_info ret;

  if (index >= m_records.size())
  {
    SPDLOG_CRITICAL("index out of range");
    DEBUG_BREAK();
  }

  const file_record rec = m_records[index];
  
  ret.id = rec.fid;
  ret.time = rec.ftime;
  
  auto segspan = rec.segs_irange.slice(m_segments);
  auto segit = segspan.begin();
  
  ret.size = segit->size;
  ret.disk_size = segit->disk_size;

  while (++segit != segspan.end())
  {
    const size_t disk_size = segit->disk_size;
    ret.size += disk_size;
    ret.disk_size += disk_size;
  }

  return ret;
}

// protected

bool archive::read_segments_raw(u32range segs_irange, const std::span<char>& dst) const
{
  if (!is_valid_segments_irange(segs_irange))
  {
    SPDLOG_ERROR("invalid segs_irange");
    return false;
  }

  auto segspan = segs_irange.slice(m_segments);

  size_t seg_offset = 0;

  for (auto seg_it = segspan.begin(); seg_it != segspan.end(); /**/)
  {
    redx::radr::segment_descriptor bulk_sd = *seg_it++;
    bulk_sd.size = 0; // unused

    while (seg_it != segspan.end())
    {
      redx::radr::segment_descriptor seg = *seg_it++;
      if (seg.offset_in_archive == bulk_sd.end_offset_in_archive())
      {
        bulk_sd.disk_size += seg.disk_size;
      }
      else
      {
        break;
      }
    }

    if (seg_offset + bulk_sd.disk_size > dst.size())
    {
      SPDLOG_ERROR("dst overflow");
      return false;
    }

    if (!read_segment(bulk_sd, dst.subspan(seg_offset, bulk_sd.disk_size), false))
    {
      SPDLOG_ERROR("couldn't read segment");
      return false;
    }

    seg_offset += bulk_sd.disk_size;
  }

  if (seg_offset < dst.size())
  {
    SPDLOG_ERROR("dst underflow");
    return false;
  }

  return true;
}

bool archive::read_segment(const redx::radr::segment_descriptor& sd, const std::span<char>& dst, bool decompress) const
{
  decompress = decompress && sd.is_segment_compressed();

  size_t expected_size = sd.disk_size;

  if (decompress)
  {
    expected_size = sd.size;

    if (!oodle::is_available())
    {
      SPDLOG_ERROR("can't decompress, oodle is not available");
      return false;
    }
  }

  if (dst.size() != expected_size)
  {
    SPDLOG_ERROR("dst size doesn't match segment size");
    return false;
  }

  std::span<char> readbuf = dst;

  std::unique_ptr<char[]> intermediate_buf{};
  if (decompress)
  {
    intermediate_buf = std::make_unique<char[]>(sd.disk_size);
    readbuf = std::span<char>{intermediate_buf.get(), sd.disk_size};
  }

  if (!read(sd.offset_in_archive, readbuf))
  {
    SPDLOG_ERROR("couldn't read segment");
    return false;
  }

  if (decompress)
  {
    if (!oodle::decompress(readbuf, dst, false))
    {
      SPDLOG_ERROR("failed to decompress segment");
      return false;
    }
  }

  return true;
}

bool archive::read(size_t offset, const std::span<char>& dst) const
{
  std::scoped_lock<std::mutex> lk(m_freader_mtx);
  //
  //m_ifs.seekg(offset);
  //m_ifs.read(dst.data(), dst.size());

  if (!m_freader.is_open())
  {
    return false;
  }

  bool ok = true;
  ok &= m_freader.seek(offset);
  ok &= m_freader.read(dst);

  return ok;
}

} // namespace redx

