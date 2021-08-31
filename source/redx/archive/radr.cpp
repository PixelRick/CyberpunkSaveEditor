#include <cpinternals/archive/radr.hpp>

#include <cpinternals/common.hpp>

namespace cp::radr {

void metadata::serialize(streambase& st, bool check_crc)
{
  const bool reading = st.is_reader();

  uint32_t tbls_offset = 8;
  uint32_t tbls_size = 0;
  uint32_t files_cnt = 0;
  uint32_t segments_cnt = 0;
  uint32_t dependencies_cnt = 0;
  uint64_t crc = 0;

  if (!reading)
  {
    files_cnt = static_cast<uint32_t>(records.size());
    segments_cnt = static_cast<uint32_t>(segments.size());
    dependencies_cnt = static_cast<uint32_t>(dependencies.size());

    crc = compute_tbls_crc64();
  }

  auto start_spos = st.tell();
  st << tbls_offset;
  st << tbls_size;

  // todo: is there stuff in between sometimes ?

  auto tbls_spos = st.tell();
  st << crc;
  st << files_cnt;
  st << segments_cnt;
  st << dependencies_cnt;

  // note: records are ordered by hashes

  if (reading)
  {
    if (tbls_offset != 8)
    {
      st.set_error("metadata: tbls_offset is expected to be 8");
      return;
    }

    records.resize(files_cnt);
    segments.resize(segments_cnt);
    dependencies.resize(dependencies_cnt);
  }

  st.serialize_pods_array_raw(records.data(), files_cnt);
  st.serialize_pods_array_raw(segments.data(), segments_cnt);
  st.serialize_pods_array_raw(dependencies.data(), dependencies_cnt);

  if (reading)
  {
    size_t read_size = st.tell() - tbls_spos;
    if (tbls_size != st.tell() - tbls_spos)
    {
      st.set_error(fmt::format("metadata: unexpected tbls_size {} for read_size {}", tbls_size, read_size));
      return;
    }

    if (st.has_error())
    {
      records.clear();
      segments.clear();
      dependencies.clear();
      return;
    }

    uint64_t ccrc = compute_tbls_crc64();
    if (crc != ccrc)
    {
      st.set_error(fmt::format("metadata: crc mismatch {:016X} {:016X}", crc, ccrc));
      return;
    }
  }
  else // writing
  {
    auto cur_spos = st.tell();
    tbls_size = static_cast<uint32_t>(tbls_spos - cur_spos);

    st.seek(start_spos);
    st << tbls_offset;
    st << tbls_size;
    st.seek(cur_spos);
  }
}

uint64_t metadata::compute_tbls_crc64()
{
  uint32_t cnt = 0;
  crc64_builder b;

  b.init();

  cnt = static_cast<uint32_t>(records.size());
  b.update(&cnt, 4);
  const size_t records_size = cnt * sizeof(file_record);

  cnt = static_cast<uint32_t>(segments.size());
  b.update(&cnt, 4);
  const size_t segments_size = cnt * sizeof(segment_descriptor);

  cnt = static_cast<uint32_t>(dependencies.size());
  b.update(&cnt, 4);
  const size_t dependencies_size = cnt * sizeof(dependency);

  b.update(records.data(), records_size);
  b.update(segments.data(), segments_size);
  b.update(dependencies.data(), dependencies_size);

  return b.finalize();
}

} // namespace cp::radr

