#include "csav.hpp"
#include <inttypes.h>
#include "serializers.hpp"

bool csav::load_stree(std::filesystem::path path, bool dump_decompressed_data)
{
  uint32_t chunkdescs_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;

  std::vector<compressed_chunk_desc> chunk_descs;
  std::vector<char>& nodedata = stree.nodedata;

  filepath = path;
  std::ifstream ifs;
  ifs.open(path, ifs.in | ifs.binary);
  if (ifs.fail())
  {
    std::string err = strerror(errno);
    std::cerr << "Error: " << err;
    return false;
  }

  // --------------------------------------------------------
  //  HEADER (magic, version..)
  // --------------------------------------------------------

  ifs >> cbytes_ref(magic);
  if (magic != 'CSAV' && magic != 'SAVE')
    return false;

  ifs >> cbytes_ref(ver.v1);
  ifs >> cbytes_ref(ver.v2);

  // DISABLED VERSION TEST
  //if (v1 > 193 or v2 > 9 or v1 < 125)
  //  return false;

  ifs >> cp_plstring_ref(suk);

  // there is a weird if v1 >= 5 check, but previous if already ensured it
  ifs >> cbytes_ref(uk0);
  ifs >> cbytes_ref(uk1);

  if (ver.v1 <= 168 and ver.v2 == 4)
    return false;
  ver.v3 = 192;
  if (ver.v1 >= 83)
  {
    ifs.read((char*)&ver.v3, 4);
    if (ver.v3 > 195) // will change soon i guess
      return false;
  }

  chunkdescs_start = (uint32_t)ifs.tellg();

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  ifs.seekg(-8, ifs.end);
  uint64_t footer_start = (uint64_t)ifs.tellg();
  ifs >> cbytes_ref(nodedescs_start);
  ifs >> cbytes_ref(magic);
  if (magic != 'DONE')
    return false;

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  // check node start tag
  ifs.seekg(nodedescs_start);
  ifs >> cbytes_ref(magic);
  if (magic != 'NODE')
    return false;

  // now read node descs
  size_t nd_cnt = 0;
  ifs >> cp_packedint_ref((int64_t&)nd_cnt);
  stree.descs.resize(nd_cnt);
  for (size_t i = 0; i < nd_cnt; ++i)
  {
    ifs >> stree.descs[i];
  }
  if ((size_t)ifs.tellg() != footer_start)
    return false;

  // --------------------------------------------------------
  //  COMPRESSED CHUNKS
  // --------------------------------------------------------

  // descriptors (padded with 0 until actual first chunk)

  ifs.seekg(chunkdescs_start, ifs.beg);
  ifs >> cbytes_ref(magic);
  if (magic != 'CLZF')
    return false;

  uint32_t cd_cnt = 0;
  ifs.read((char*)&cd_cnt, 4);
  chunk_descs.resize(cd_cnt);
  for (uint32_t i = 0; i < cd_cnt; ++i)
  {
    ifs >> chunk_descs[i];
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

  bool is_ps4 = false;
  for (int i = 0; i < chunk_descs.size(); ++i)
  {
    auto& cd = chunk_descs[i];

    ifs.seekg(cd.offset, ifs.beg);
    ifs >> cbytes_ref(magic);
    if (magic != 'XLZ4')
    {
      if (i > 0)
        return false;

      is_ps4 = true;
      break;
    }

    uint32_t data_size = 0;
    ifs >> cbytes_ref(data_size);
    if (data_size != cd.data_size)
      return false;

    size_t csize = cd.size-8;
    if (csize > tmp.size())
      tmp.resize(csize);
    ifs.read(tmp.data(), csize);

    int res = LZ4_decompress_safe(tmp.data(), nodedata.data() + cd.data_offset, (int)csize, cd.data_size);
    if (res != cd.data_size)
      return false;
  }

  if (is_ps4)
  {
    size_t offset = chunk_descs[0].offset;
    ifs.seekg(offset, ifs.beg);
    ifs.read(nodedata.data() + offset, nodedata_size - offset);
  }

  if (ifs.fail())
    return false;
  ifs.close();

  // --------------------------------------------------------
  //  UNFLATTENING of node tree
  // --------------------------------------------------------

  root_node = stree.to_node(chunks_start);
  if (!root_node)
    return false;

  const uint32_t data_size = (uint32_t)nodedata.size() - chunks_start;
  auto tree_size = root_node->calcsize();
  if (tree_size != data_size) // check that the unflattening worked
    return false;

  if (dump_decompressed_data)
  {
    std::ofstream ofs;
    auto dump_path = path;
    dump_path.replace_filename(L"decompressed_blob_in.bin");
    ofs.open(dump_path, ofs.binary | ofs.trunc);
    ofs.write(stree.nodedata.data(), stree.nodedata.size());
    ofs.close();
  }

  return true;
}

bool csav::save_stree(std::filesystem::path path, bool dump_decompressed_data, bool ps4_weird_format)
{
  if (!root_node)
    return false;

  uint32_t chunkdescs_start = 0;
  uint32_t chunks_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;

  std::vector<compressed_chunk_desc> chunk_descs;

  // make a backup (when there isn't one, oldest wins for safety reasons)
  if (std::filesystem::exists(path))
  {
    std::filesystem::path oldpath = path;
    oldpath.replace_extension(L".old");
    if (!std::filesystem::exists(oldpath)) 
      std::filesystem::copy(path, oldpath);
  }

  filepath = path;
  std::ofstream ofs;
  ofs.open(path, ofs.out | ofs.binary | ofs.trunc);
  if (ofs.fail())
  {
    std::string err = strerror(errno);
    std::cerr << "Error: " << err;
    return false;
  }

  // --------------------------------------------------------
  //  HEADER (magic, version..)
  // --------------------------------------------------------

  magic = 'CSAV';
  ofs << cbytes_ref(magic);

  ofs << cbytes_ref(ver.v1);
  ofs << cbytes_ref(ver.v2);
  ofs << cp_plstring_ref(suk);
  ofs << cbytes_ref(uk0);
  ofs << cbytes_ref(uk1);

  if (ver.v1 >= 83)
    ofs << cbytes_ref(ver.v3);

  // --------------------------------------------------------
  //  WEIRD PREP
  // --------------------------------------------------------

  chunkdescs_start = (uint32_t)ofs.tellp();

  uint32_t expected_raw_size = (uint32_t)root_node->calcsize();
  size_t max_chunkcnt = LZ4_compressBound(expected_raw_size) / XLZ4_CHUNK_SIZE + 2; // tbl should fit in 1 extra XLZ4_CHUNK_SIZE 
  size_t chunktbl_maxsize = max_chunkcnt * compressed_chunk_desc::serialized_size + 8;

  std::vector<char> tmp;
  tmp.resize(XLZ4_CHUNK_SIZE);

  // allocate tbl
  ofs.write(tmp.data(), std::max(chunktbl_maxsize, 0xC21 - (size_t)chunkdescs_start));
  chunks_start = (uint32_t)ofs.tellp();

  // --------------------------------------------------------
  //  FLATTENING of node tree
  // --------------------------------------------------------

  if (!stree.from_node(root_node, chunks_start))
    return false;

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
    chunk_desc.offset = (uint32_t)ofs.tellp();

    int srcsize = (int)(pend - pcur);

    if (ps4_weird_format)
    {
      srcsize = std::min(srcsize, XLZ4_CHUNK_SIZE);
      // write decompressed chunk
      ofs.write(pcur, srcsize);
      chunk_desc.size = srcsize;
    }
    else
    {
      int csize = LZ4_compress_destSize(pcur, ptmp, &srcsize, XLZ4_CHUNK_SIZE);
      if (csize < 0)
        return false;

      // write magic
      magic = 'XLZ4';
      ofs << cbytes_ref(magic);
      // write decompressed size
      uint32_t data_size = 0;
      ofs << cbytes_ref(srcsize);
      // write compressed chunk
      ofs.write(ptmp, csize);

      chunk_desc.size = csize+8;
    }

    chunk_desc.data_size = srcsize;
    pcur += srcsize;
  }
  if (pcur > pend)
    return false;

  nodedescs_start = (uint32_t)ofs.tellp();

  // descriptors

  ofs.seekp(chunkdescs_start);

  magic = 'CLZF';
  ofs << cbytes_ref(magic);
  uint32_t cd_cnt = (uint32_t)chunk_descs.size();
  ofs << cbytes_ref(cd_cnt);

  for (uint32_t i = 0; i < cd_cnt; ++i)
  {
    ofs << chunk_descs[i];
  }

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  ofs.seekp(nodedescs_start);


  // experiment: would the game accept big forged file ?
  // std::vector<char> zerobuf(0x1000000);
  // ofs.write(zerobuf.data(), zerobuf.size());
  // nodedescs_start = ofs.tellp();

  magic = 'NODE';
  ofs << cbytes_ref(magic);

  // now write node descs
  const uint32_t node_cnt = (uint32_t)stree.descs.size();
  int64_t node_cnt_i64 = (int64_t)node_cnt;
  ofs << cp_packedint_ref(node_cnt_i64);
  for (uint32_t i = 0; i < node_cnt; ++i)
  {
    ofs << stree.descs[i];
  }

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  ofs << cbytes_ref(nodedescs_start);
  magic = 'DONE';
  ofs << cbytes_ref(magic);

  if (ofs.fail())
    return false;
  ofs.close();

  // --------------------------------------------------------
  //  save decompressed blob (request)
  // --------------------------------------------------------

  if (dump_decompressed_data)
  {
    auto dump_path = path;
    dump_path.replace_filename(L"decompressed_blob_out.bin");
    ofs.open(dump_path, ofs.binary | ofs.trunc);
    ofs.write(stree.nodedata.data(), stree.nodedata.size());
    ofs.close();
  }

  return true;
}

