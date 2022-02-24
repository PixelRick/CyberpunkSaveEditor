#include <redx/radr/srxl/srxl_format.hpp>

#include <redx/core.hpp>
#include <redx/io/mem_bstream.hpp>
#include <redx/io/oodle/oodle.hpp>

namespace redx::radr::srxl::format {

bool srxl::serialize_in(ibstream& st, bool check_crc)
{
  if (!oodle::is_available())
  {
    STREAM_LOG_AND_SET_ERROR(st, "oodle is not available");
    return false;
  }

  header hdr;
  st >> hdr;

  if (hdr.ver.v1 != 1)
  {
    STREAM_LOG_AND_SET_ERROR(st, "unsupported srxl version");
    return false;
  }

  if (!hdr.is_magic_ok())
  {
    STREAM_LOG_AND_SET_ERROR(st, "srxl buffer has wrong magic");
    return false;
  }

  if (!st)
  {
    return false;
  }

  std::vector<char> block(hdr.disk_size, 0);
  std::span<const char> srxl_blk(block);

  std::vector<char> decompressed_block;
  if (hdr.mem_size != hdr.disk_size)
  {
    decompressed_block.resize(hdr.mem_size);

    if (!oodle::decompress_noheader(block, decompressed_block, true))
    {
      STREAM_LOG_AND_SET_ERROR(st, "couldn't decompress srxl buffer");
      return false;
    }

    srxl_blk = {decompressed_block};
  }

  mem_ibstream mst(srxl_blk);

  //svec.serialize_in(mst, 0, hdr.svec_descs_size, hdr.svec_data_size);

  mst.read_array(records);

  if (!mst)
  {
    //svec.clear();
    records.clear();
    return false;
  }
  
  return true;
}

bool srxl::serialize_out(obstream& st) const
{
  if (!oodle::is_available())
  {
    STREAM_LOG_AND_SET_ERROR(st, "oodle is not available");
    return false;
  }

  header hdr = {};

  return false;
  //todo: requires memory_ostream
  /*const auto& block = sp.memory_block();
  auto compressed_block = oodle::compress_noheader(block, oodle::compression_level::best2);
  const std::vector<char>* disk_block = compressed_block.empty() ? &block : &compressed_block;

  hdr.entries_cnt = sp.size();
  hdr.mem_size = static_cast<uint32_t>(block.size());
  hdr.disk_size = static_cast<uint32_t>(disk_block->size());

  st << hdr;
  st.serialize_bytes(const_cast<char*>(disk_block->data()), disk_block->size());*/

}


} // namespace redx::radr::srxl::format






//std::vector<fs_gname> names; // directory names are first
//  names.reserve(hdr.names_cnt);
//
//  std::string name;
//  for (size_t i = 0; i < hdr.names_cnt; ++i)
//  {
//    ifs.serialize_str_lpfxd(name);
//    names.emplace_back(name);
//  }
//
//  std::vector<ardb_record> recs(hdr.entries_cnt);
//  ifs.serialize_pods_array_raw(recs.data(), hdr.entries_cnt);
//
//  std::vector<int32_t> entry_indices;
//
//  for (size_t i = 0; i < hdr.entries_cnt; ++i)
//  {
//    const ardb_record& rec = recs[i];
//    const bool is_file = (rec.name_idx >= hdr.dirnames_cnt);
//
//    int32_t parent_entry_idx;
//    if (rec.parent_idx == ardb_root_idx)
//    {
//      parent_entry_idx = root_idx;
//    }
//    else if (rec.parent_idx >= i)
//    {
//      SPDLOG_ERROR("unordered record found");
//      return false;
//    }
//    else
//    {
//      parent_entry_idx = entry_indices[rec.parent_idx];
//    }
//
//    fs_gname name = names[rec.name_idx];
//
//    // temporary, ardbs have the root entry for some reason..
//    if (name.strv() == "")
//    {
//      assert(parent_entry_idx == root_idx);
//      entry_indices.emplace_back(root_idx);
//      continue;
//    }
//
//    int32_t entry_index = insert_child_entry(parent_entry_idx, name, is_file ? entry_kind::reserved_for_file : entry_kind::directory, true).first;
//    if (entry_index < 0)
//    {
//      SPDLOG_ERROR("failed to insert entry for {}", name.strv());
//      return false;
//    }
//
//    entry_indices.emplace_back(entry_index);
//  }
//
