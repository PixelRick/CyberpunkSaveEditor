#include <redx/radr/radr_format.hpp>

#include <redx/core.hpp>

namespace redx::radr::format {

bool metadata::serialize_in(ibstream& st, bool check_crc)
{
  metadata_trampoline trp;
  st >> trp;

  if (trp.tbls_offset != 8)
  {
    STREAM_LOG_AND_SET_ERROR(st, "unexpected tbls_offset");
    return false;
  }

  auto tbls_spos = st.tellg();
  metadata_tbls_header hdr;
  st >> hdr;

  records.resize(hdr.files_cnt);
  segments.resize(hdr.segments_cnt);
  dependencies.resize(hdr.dependencies_cnt);

  st.read_array(records);
  st.read_array(segments);
  st.read_array(dependencies);

  size_t read_size = st.tellg() - tbls_spos;
  if (trp.tbls_size != read_size)
  {
    STREAM_LOG_AND_SET_ERROR(st, "unexpected tbls_size {} for read_size {}", trp.tbls_size, read_size);
    return false;
  }

  if (!st)
  {
    records.clear();
    segments.clear();
    dependencies.clear();
    return false;
  }

  if (check_crc)
  {
    uint64_t ccrc = compute_tbls_crc64();
    if (hdr.crc != ccrc)
    {
      STREAM_LOG_AND_SET_ERROR(st, "crc mismatch hdr:{:016X} computed:{:016X}", hdr.crc, ccrc);
      return false;
    }
  }
  
  return true;
}


bool metadata::serialize_out(obstream& st) const
{
  metadata_trampoline trp;
  metadata_tbls_header hdr;

  hdr.files_cnt = numeric_cast<uint32_t>(records.size());
  hdr.segments_cnt = numeric_cast<uint32_t>(segments.size());
  hdr.dependencies_cnt = numeric_cast<uint32_t>(dependencies.size());
  hdr.crc = compute_tbls_crc64();

  trp.tbls_offset = 8;
  auto trp_spos = st.tellp();
  st << trp;

  // todo: is there stuff in between sometimes ?

  auto tbls_spos = st.tellp();
  st << hdr;

  // note: records are ordered by hashes

  st.write_array(records);
  st.write_array(segments);
  st.write_array(dependencies);

  auto end_spos = st.tellp();
  trp.tbls_size = numeric_cast<uint32_t>(end_spos - tbls_spos);

  if (!st) // let's check before calling seek
  {
    return false;
  }

  st.seekp(trp_spos);
  st << trp;

  if (!st) // let's check before calling seek
  {
    return false;
  }

  st.seekp(end_spos);

  return !!st;
}

uint64_t metadata::compute_tbls_crc64() const
{
  uint32_t cnt = 0;
  crc64_builder b;

  b.init();

  cnt = numeric_cast<uint32_t>(records.size());
  b.update(&cnt, 4);
  const size_t records_bsize = cnt * sizeof(file_record);

  cnt = numeric_cast<uint32_t>(segments.size());
  b.update(&cnt, 4);
  const size_t segments_bsize = cnt * sizeof(segment_descriptor);

  cnt = numeric_cast<uint32_t>(dependencies.size());
  b.update(&cnt, 4);
  const size_t dependencies_bsize = cnt * sizeof(dependency);

  b.update(records.data(), records_bsize);
  b.update(segments.data(), segments_bsize);
  b.update(dependencies.data(), dependencies_bsize);

  return b.finalize();
}

} // namespace redx::radr::format

