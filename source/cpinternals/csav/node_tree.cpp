#include "node_tree.hpp"

#include <xlz4/lz4.h>
#include <cpinternals/stream/fstream.hpp>
#include <cpinternals/csav/serial_tree.hpp>

#define XLZ4_CHUNK_SIZE 0x40000

namespace cp::csav {

struct compressed_chunk_desc
{
  static const size_t serialized_size = 12;

  // data_size is uncompressed size
  uint32_t offset, size, data_size, data_offset;

  friend streambase& operator<<(streambase& ar, compressed_chunk_desc& cd)
  {
    return ar << cd.offset << cd.size << cd.data_size;
  }
};

op_status node_tree::load(std::filesystem::path path)
{
  ifstream ar(path);
  serialize_in(ar);

  return op_status(ar.error());
}

op_status node_tree::save(std::filesystem::path path)
{
  // make a backup (when there isn't one, oldest wins for safety reasons)
  if (std::filesystem::exists(path))
  {
    std::filesystem::path oldpath = path;
    oldpath.replace_extension(L".old");
    if (!std::filesystem::exists(oldpath)) 
      std::filesystem::copy(path, oldpath);
  }

  ofstream ar(path);
  serialize_out(ar);

  return op_status(ar.error());
}

void node_tree::serialize_in(streambase& ar)
{
  if (!ar.is_reader())
  {
    ar.set_error("serialize_in cannot be used with output stream");
    return;
  }

  uint32_t chunkdescs_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;


  serial_tree stree;
  std::vector<compressed_chunk_desc> chunk_descs;
  std::vector<char>& nodedata = stree.nodedata;

  // --------------------------------------------------------
  //  HEADER (magic, m_ver..)
  // --------------------------------------------------------

  ar << magic;
  if (magic != 'CSAV' && magic != 'SAVE')
  {
    ar.set_error("csav file has wrong magic");
    return;
  }

  ar << m_ver.v1;
  ar << m_ver.v2;

  // DISABLED VERSION TEST
  //if (v1 > 193 or v2 > 9 or v1 < 125)
  //  return false;

  ar.serialize_str_lpfxd(m_ver.suk);

  // there is a weird if v1 >= 5 check, but previous if already ensured it
  ar << m_ver.uk0;
  ar << m_ver.uk1;

  if (m_ver.v1 <= 168 and m_ver.v2 == 4)
  {
    ar.set_error("unsuppported csav m_ver.v1/v2");
    return;
  }

  m_ver.v3 = 192;
  if (m_ver.v1 >= 83)
  {
    ar << m_ver.v3;
    if (m_ver.v3 > 195) // will change soon i guess
    {
      ar.set_error("unsuppported csav m_ver.v3");
      return;
    }
  }

  chunkdescs_start = (uint32_t)ar.tell();

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  ar.seek(-8, std::ios_base::end);
  uint64_t footer_start = (uint64_t)ar.tell();
  ar << nodedescs_start;
  ar << magic;
  if (magic != 'DONE')
  {
    ar.set_error("missing 'DONE' tag");
    return;
  }

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  // check node start tag
  ar.seek(nodedescs_start);
  ar << magic;
  if (magic != 'NODE')
  {
    ar.set_error("missing 'NODE' tag");
    return;
  }

  // now read node descs
  size_t nd_cnt = 0;
  ar.serialize_int_packed(nd_cnt);
  stree.descs.resize(nd_cnt);
  for (size_t i = 0; i < nd_cnt; ++i)
  {
    ar << stree.descs[i];
  }
  if ((size_t)ar.tell() != footer_start)
  {
    ar.set_error("unexpected footer position");
    return;
  }

  // --------------------------------------------------------
  //  COMPRESSED CHUNKS
  // --------------------------------------------------------

  // descriptors (padded with 0 until actual first chunk)

  ar.seek(chunkdescs_start);
  ar << magic;
  if (magic != 'CLZF')
  {
    ar.set_error("missing 'CLZF' tag");
    return;
  }

  uint32_t cd_cnt = 0;
  ar << cd_cnt;
  chunk_descs.resize(cd_cnt);
  for (uint32_t i = 0; i < cd_cnt; ++i)
  {
    ar << chunk_descs[i];
  }

  // actual chunks

  std::sort(chunk_descs.begin(), chunk_descs.end(),
    [](auto& a, auto& b){return a.offset < b.offset; });
  uint64_t nodedata_size = 0;
  uint32_t chunks_start = 0;

  if (chunk_descs.size())
  {
    chunks_start = chunk_descs[0].offset;
    uint32_t data_offset = chunks_start; // that's how they do, minimal offset in file..
    for (int i = 0; i < chunk_descs.size(); ++i)
    {
      auto& cd = chunk_descs[i];
      cd.data_offset = data_offset;
      data_offset += cd.data_size;
    }
    nodedata_size = data_offset;
  }

  // --------------------------------------------------------
  //  DECOMPRESSION from compressed chunks to nodedata
  // --------------------------------------------------------

  // we are not concerned by ram nor time, let's decompress the whole node
  std::vector<char> tmp;
  tmp.resize(XLZ4_CHUNK_SIZE);
  nodedata.clear();
  nodedata.resize(nodedata_size);

  m_ver.ps4w = false;
  for (int i = 0; i < chunk_descs.size(); ++i)
  {
    auto& cd = chunk_descs[i];

    ar.seek(cd.offset);
    ar << magic;
    if (magic != 'XLZ4')
    {
      if (i > 0)
      {
        ar.set_error("missing 'XLZ4' tag");
        return;
      }

      m_ver.ps4w = true;
      break;
    }

    uint32_t data_size = 0;
    ar << data_size;
    if (data_size != cd.data_size)
    {
      ar.set_error("data size prefix differs from descriptor's value");
      return;
    }

    size_t csize = cd.size-8;
    if (csize > tmp.size())
      tmp.resize(csize);
    ar.serialize(tmp.data(), csize);

    int res = LZ4_decompress_safe(tmp.data(), nodedata.data() + cd.data_offset, (int)csize, cd.data_size);
    if (res != cd.data_size)
    {
      ar.set_error("unexpected lz4 decompressed size");
      return;
    }
  }

  if (m_ver.ps4w)
  {
    size_t offset = chunk_descs[0].offset;
    ar.seek(offset);
    ar.serialize(nodedata.data() + offset, nodedata_size - offset);
  }

  if (ar.has_error())
  {
    return;
  }

  // --------------------------------------------------------
  //  UNFLATTENING of node tree
  // --------------------------------------------------------

  root = stree.to_tree(chunks_start);
  if (!root)
  {
    ar.set_error("couldn't lift a tree from serial_tree");
    return;
  }

  const uint32_t data_size = (uint32_t)nodedata.size() - chunks_start;
  auto tree_size = root->calcsize();

  // Check that the unflattening worked.
  if (tree_size != data_size)
  {
    ar.set_error("lift tree size differs from serial_tree size");
    return;
  }

  // --------------------------------------------------------
  //  MISC
  // --------------------------------------------------------

  original_descs = stree.descs;
}

void node_tree::serialize_out(streambase& ar)
{
  if (ar.is_reader())
  {
    ar.set_error("serialize_out cannot be used with input stream");
    return;
  }

  if (!root)
    return;

  std::vector<serial_node_desc> descs;

  uint32_t chunkdescs_start = 0;
  uint32_t chunks_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;

  std::vector<compressed_chunk_desc> chunk_descs;

  // --------------------------------------------------------
  //  HEADER (magic, m_ver..)
  // --------------------------------------------------------

  magic = 'CSAV';
  ar << magic;

  ar << m_ver.v1;
  ar << m_ver.v2;
  ar.serialize_str_lpfxd(m_ver.suk);
  ar << m_ver.uk0;
  ar << m_ver.uk1;

  if (m_ver.v1 >= 83)
    ar << m_ver.v3;

  // --------------------------------------------------------
  //  WEIRD PREP
  // --------------------------------------------------------

  chunkdescs_start = (uint32_t)ar.tell();

  uint32_t expected_raw_size = (uint32_t)root->calcsize();
  size_t max_chunkcnt = LZ4_compressBound(expected_raw_size) / XLZ4_CHUNK_SIZE + 2; // tbl should fit in 1 extra XLZ4_CHUNK_SIZE 
  size_t chunktbl_maxsize = max_chunkcnt * compressed_chunk_desc::serialized_size + 8;

  std::vector<char> tmp;
  tmp.resize(XLZ4_CHUNK_SIZE);

  // allocate tbl
  ar.serialize(tmp.data(), std::max(chunktbl_maxsize, 0xC21 - (size_t)chunkdescs_start));
  chunks_start = (uint32_t)ar.tell();

  // --------------------------------------------------------
  //  FLATTENING of node tree
  // --------------------------------------------------------

  serial_tree stree;
  if (!stree.from_tree(root, chunks_start))
  {
    ar.set_error("couldn't flatten node tree.");
    return;
  }

  // --------------------------------------------------------
  //  COMPRESSION from nodedata to compressed chunks
  // --------------------------------------------------------

  // chunks

  char* const ptmp = tmp.data();
  char* const prealbeg = stree.nodedata.data();
  char* const pbeg = prealbeg + chunks_start; // compression starts at min_offset!
  char* const pend = prealbeg + stree.nodedata.size();
  char* pcur = pbeg;

  i = 1;
  while (pcur < pend)
  {
    auto& chunk_desc = chunk_descs.emplace_back();

    chunk_desc.data_offset = (uint32_t)(pcur - prealbeg);
    chunk_desc.offset = (uint32_t)ar.tell();

    int srcsize = (int)(pend - pcur);

    if (m_ver.ps4w)
    {
      srcsize = std::min(srcsize, XLZ4_CHUNK_SIZE);
      // write decompressed chunk
      ar.serialize(pcur, srcsize);
      chunk_desc.size = srcsize;
    }
    else
    {
      int csize = LZ4_compress_destSize(pcur, ptmp, &srcsize, XLZ4_CHUNK_SIZE);
      if (csize < 0)
      {
        ar.set_error("lz4 compression failed");
        return;
      }

      // write magic
      magic = 'XLZ4';
      ar << magic;
      // write decompressed size
      uint32_t data_size = 0;
      ar << srcsize;
      // write compressed chunk
      ar.serialize(ptmp, csize);

      chunk_desc.size = csize+8;
    }

    chunk_desc.data_size = srcsize;
    pcur += srcsize;
  }
  if (pcur > pend)
  {
    ar.set_error("pcur > pend");
    return;
  }

  nodedescs_start = (uint32_t)ar.tell();

  // descriptors

  ar.seek(chunkdescs_start);

  magic = 'CLZF';
  ar << magic;
  uint32_t cd_cnt = (uint32_t)chunk_descs.size();
  ar << cd_cnt;

  for (uint32_t i = 0; i < cd_cnt; ++i)
  {
    ar << chunk_descs[i];
  }

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  ar.seek(nodedescs_start);

  // experiment: would the game accept big forged file ?
  // std::vector<char> zerobuf(0x1000000);
  // ofs.write(zerobuf.data(), zerobuf.size());
  // nodedescs_start = ofs.tellp();

  magic = 'NODE';
  ar << magic;

  // now write node descs
  const uint32_t node_cnt = (uint32_t)stree.descs.size();
  int64_t node_cnt_i64 = (int64_t)node_cnt;
  ar.serialize_int_packed(node_cnt_i64);
  for (uint32_t i = 0; i < node_cnt; ++i)
  {
    ar << stree.descs[i];
  }

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  ar << nodedescs_start;
  magic = 'DONE';
  ar << magic;
}

} // namespace cp::csav

