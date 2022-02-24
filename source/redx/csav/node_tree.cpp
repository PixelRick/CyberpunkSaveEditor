#include "node_tree.hpp"

#include <xlz4/lz4.h>
#include <redx/io/bstream.hpp>
#include <redx/io/file_bstream.hpp>
#include <redx/csav/serial_tree.hpp>

#define XLZ4_CHUNK_SIZE 0x40000

namespace redx::csav {

struct compressed_chunk_desc
  : trivially_serializable<compressed_chunk_desc>
{
  // data_size is uncompressed size
  uint32_t offset, size, data_size;
};

struct compressed_chunk_desc2
  : compressed_chunk_desc
{
  uint32_t data_offset;

  friend inline ibstream& operator>>(ibstream& st, compressed_chunk_desc2& x)
  {
    return st.read_bytes((char*)&x, sizeof(compressed_chunk_desc));
  }

  friend inline obstream& operator<<(obstream& st, const compressed_chunk_desc2& x)
  {
    return st.write_bytes((const char*)&x, sizeof(compressed_chunk_desc));
  }
};

static_assert(sizeof(compressed_chunk_desc2) == 16);

op_status node_tree::load(std::filesystem::path path)
{
  std_file_ibstream st(path);
  serialize_in(st);

  return op_status(st.get_fail_msg());
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

  std_file_obstream st(path);
  serialize_out(st);

  return op_status(st.get_fail_msg());
}

void node_tree::serialize_in(ibstream& st)
{
  uint32_t chunkdescs_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;


  serial_tree stree;
  std::vector<compressed_chunk_desc2> chunk_descs;
  std::vector<char>& nodedata = stree.nodedata;

  // --------------------------------------------------------
  //  HEADER (magic, m_ver..)
  // --------------------------------------------------------

  st >> magic;
  if (magic != 'CSAV' && magic != 'SAVE')
  {
    STREAM_LOG_AND_SET_ERROR(st, "csav file has wrong magic");
    return;
  }

  st >> m_ver.v1;
  st >> m_ver.v2;

  // DISABLED VERSION TEST
  //if (v1 > 193 or v2 > 9 or v1 < 125)
  //  return false;

  st.read_str_lpfxd(m_ver.suk);

  // there is a weird if v1 >= 5 check, but previous if already ensured it
  st >> m_ver.uk0;
  st >> m_ver.uk1;

  if (m_ver.v1 <= 168 and m_ver.v2 == 4)
  {
    STREAM_LOG_AND_SET_ERROR(st, "unsuppported csav m_ver.v1/v2");
    return;
  }

  m_ver.v3 = 192;
  if (m_ver.v1 >= 83)
  {
    st >> m_ver.v3;
    if (m_ver.v3 > 195) // will change soon i guess
    {
      STREAM_LOG_AND_SET_ERROR(st, "unsuppported csav m_ver.v3");
      return;
    }
  }

  chunkdescs_start = static_cast<uint32_t>(st.tellg());

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  st.seekg(-8, std::ios_base::end);
  uint64_t footer_start = static_cast<uint64_t>(st.tellg());
  st >> nodedescs_start;
  st >> magic;
  if (magic != 'DONE')
  {
    STREAM_LOG_AND_SET_ERROR(st, "missing 'DONE' tag");
    return;
  }

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  // check node start tag
  st.seekg(nodedescs_start);
  st >> magic;
  if (magic != 'NODE')
  {
    STREAM_LOG_AND_SET_ERROR(st, "missing 'NODE' tag");
    return;
  }

  // now read node descs
  size_t nd_cnt = 0;
  st.read_int_packed(nd_cnt);
  stree.descs.resize(nd_cnt);
  st.read_array(stree.descs);

  if (!st)
  {
    STREAM_LOG_AND_SET_ERROR(st, "couldn't read descs");
    return;
  }

  if (static_cast<uint64_t>(st.tellg()) != footer_start)
  {
    STREAM_LOG_AND_SET_ERROR(st, "unexpected footer position");
    return;
  }

  // --------------------------------------------------------
  //  COMPRESSED CHUNKS
  // --------------------------------------------------------

  // descriptors (padded with 0 until actual first chunk)

  st.seekg(chunkdescs_start);
  st >> magic;
  if (magic != 'CLZF')
  {
    STREAM_LOG_AND_SET_ERROR(st, "missing 'CLZF' tag");
    return;
  }

  st.read_vec_lpfxd(chunk_descs);

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

    st.seekg(cd.offset);
    st >> magic;
    if (magic != 'XLZ4')
    {
      if (i > 0)
      {
        STREAM_LOG_AND_SET_ERROR(st, "missing 'XLZ4' tag");
        return;
      }

      m_ver.ps4w = true;
      break;
    }

    uint32_t data_size = 0;
    st >> data_size;
    if (data_size != cd.data_size)
    {
      STREAM_LOG_AND_SET_ERROR(st, "data size prefix differs from descriptor's value");
      return;
    }

    size_t csize = cd.size-8;
    if (csize > tmp.size())
      tmp.resize(csize);
    st.read_bytes(tmp.data(), csize);

    int res = LZ4_decompress_safe(tmp.data(), nodedata.data() + cd.data_offset, (int)csize, cd.data_size);
    if (res != cd.data_size)
    {
      STREAM_LOG_AND_SET_ERROR(st, "unexpected lz4 decompressed size");
      return;
    }
  }

  if (m_ver.ps4w)
  {
    size_t offset = chunk_descs[0].offset;
    st.seekg(offset);
    st.read_bytes(nodedata.data() + offset, nodedata_size - offset);
  }

  if (!st)
  {
    return;
  }

  // --------------------------------------------------------
  //  UNFLATTENING of node tree
  // --------------------------------------------------------

  root = stree.to_tree(chunks_start);
  if (!root)
  {
    STREAM_LOG_AND_SET_ERROR(st, "couldn't lift a tree from serial_tree");
    return;
  }

  const uint32_t data_size = (uint32_t)nodedata.size() - chunks_start;
  auto tree_size = root->calcsize();

  // Check that the unflattening worked.
  if (tree_size != data_size)
  {
    STREAM_LOG_AND_SET_ERROR(st, "lift tree size differs from serial_tree size");
    return;
  }

  // --------------------------------------------------------
  //  MISC
  // --------------------------------------------------------

  original_descs = stree.descs;
}

void node_tree::serialize_out(obstream& st)
{
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
  st << magic;

  st << m_ver.v1;
  st << m_ver.v2;
  st.write_str_lpfxd(m_ver.suk);
  st << m_ver.uk0;
  st << m_ver.uk1;

  if (m_ver.v1 >= 83)
  {
    st << m_ver.v3;
  }

  // --------------------------------------------------------
  //  WEIRD PREP
  // --------------------------------------------------------

  chunkdescs_start = reliable_numeric_cast<uint32_t>(st.tellp());

  uint32_t expected_raw_size = (uint32_t)root->calcsize();
  size_t max_chunkcnt = LZ4_compressBound(expected_raw_size) / XLZ4_CHUNK_SIZE + 2; // tbl should fit in 1 extra XLZ4_CHUNK_SIZE 
  size_t chunktbl_maxsize = (max_chunkcnt * sizeof(compressed_chunk_desc)) + 8;

  std::vector<char> tmp;
  tmp.resize(XLZ4_CHUNK_SIZE);

  // allocate tbl
  st.write_bytes(tmp.data(), std::max(chunktbl_maxsize, 0xC21 - (size_t)chunkdescs_start));
  chunks_start = (uint32_t)st.tellp();

  // --------------------------------------------------------
  //  FLATTENING of node tree
  // --------------------------------------------------------

  serial_tree stree;
  if (!stree.from_tree(root, chunks_start))
  {
    STREAM_LOG_AND_SET_ERROR(st, "couldn't flatten node_t tree.");
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

    //chunk_desc.data_offset = reliable_numeric_cast<uint32_t>(pcur - prealbeg);
    chunk_desc.offset = reliable_numeric_cast<uint32_t>(st.tellp());

    int srcsize = (int)(pend - pcur);

    if (m_ver.ps4w)
    {
      srcsize = std::min(srcsize, XLZ4_CHUNK_SIZE);
      // write decompressed chunk
      st.write_bytes(pcur, srcsize);
      chunk_desc.size = srcsize;
    }
    else
    {
      int csize = LZ4_compress_destSize(pcur, ptmp, &srcsize, XLZ4_CHUNK_SIZE);
      if (csize < 0)
      {
        STREAM_LOG_AND_SET_ERROR(st, "lz4 compression failed");
        return;
      }

      // write magic
      magic = 'XLZ4';
      st << magic;
      // write decompressed size
      uint32_t data_size = 0;
      st << srcsize;
      // write compressed chunk
      st.write_bytes(ptmp, csize);

      chunk_desc.size = csize+8;
    }

    chunk_desc.data_size = srcsize;
    pcur += srcsize;
  }
  if (pcur > pend)
  {
    STREAM_LOG_AND_SET_ERROR(st, "pcur > pend");
    return;
  }

  nodedescs_start = reliable_numeric_cast<uint32_t>(st.tellp());

  // descriptors

  st.seekp(chunkdescs_start);

  magic = 'CLZF';
  st << magic;

  st.write_vec_lpfxd(chunk_descs);

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  st.seekp(nodedescs_start);

  // experiment: would the game accept big forged file ?
  // std::vector<char> zerobuf(0x1000000);
  // ofs.write(zerobuf.data(), zerobuf.size());
  // nodedescs_start = ofs.tellp();

  magic = 'NODE';
  st << magic;

  // now write node descs
  const uint32_t node_cnt = reliable_numeric_cast<uint32_t>(stree.descs.size());
  st.write_int_packed(node_cnt);
  st.write_array(stree.descs);

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  st << nodedescs_start;
  magic = 'DONE';
  st << magic;
}

} // namespace redx::csav

