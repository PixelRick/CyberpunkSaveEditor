#include "csav.hpp"

bool csav::open_with_progress(std::filesystem::path path, float& progress)
{
  uint32_t chunkdescs_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;

  std::vector<compressed_chunk_desc> chunk_descs;
  std::vector<char> nodedata;

  progress = 0.05f;

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

  ifs.read((char*)&magic, 4);
  if (magic != 'CSAV' && magic != 'SAVE')
    return false;

  progress = 0.1f;

  ifs.read((char*)&v1, 4);
  ifs.read((char*)&v2, 4);
  if (v1 > 193 or v2 > 9 or v1 < 125)
    return false;
  suk = read_str(ifs);
  // there is a weird if v1 >= 5 check, but previous if already ensured it
  ifs.read((char*)&uk0, 4);
  ifs.read((char*)&uk1, 4);
  if (v1 <= 168 and v2 == 4)
    return false;

  v3 = 192;
  if (v1 >= 83)
  {
    ifs.read((char*)&v3, 4);
    if (v1 > 195)
      return false;
  }
  chunkdescs_start = (uint32_t)ifs.tellg();

  progress = 0.15f;

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  ifs.seekg(-8, ifs.end);
  uint64_t footer_start = (uint64_t)ifs.tellg();
  ifs.read((char*)&nodedescs_start, 4);
  ifs.read((char*)&magic, 4);
  if (magic != 'DONE')
    return false;

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  // check node start tag
  ifs.seekg(nodedescs_start);
  ifs.read((char*)&magic, 4);
  if (magic != 'NODE')
    return false;

  progress = 0.2f;

  // now read node descs
  uint32_t nd_cnt = (uint32_t)read_packed_int(ifs);
  node_descs.resize(nd_cnt);
  for (uint32_t i = 0; i < nd_cnt; ++i)
  {
    ifs >> node_descs[i];
    progress = 0.2f + 0.1f * i / (float)nd_cnt;
  }
  if ((uint32_t)ifs.tellg() != footer_start)
    return false;

  progress = 0.3f;

  // --------------------------------------------------------
  //  COMPRESSED CHUNKS
  // --------------------------------------------------------

  // descriptors (padded with 0 until actual first chunk)

  ifs.seekg(chunkdescs_start, ifs.beg);
  magic = 0;
  ifs.read((char*)&magic, 4);
  if (magic != 'CLZF')
    return false;
  uint32_t cd_cnt = 0;
  ifs.read((char*)&cd_cnt, 4);
  chunk_descs.resize(cd_cnt);
  for (uint32_t i = 0; i < cd_cnt; ++i)
  {
    ifs >> chunk_descs[i];
    progress = 0.3f + 0.1f * i / (float)cd_cnt;
  }

  progress = 0.4f;

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
      progress = 0.4f + 0.1f * i / (float)cd_cnt;
    }
    nodedata_size = data_offset;
  }

  progress = 0.5f;

  // --------------------------------------------------------
  //  DECOMPRESSION from compressed chunks to nodedata
  // --------------------------------------------------------

  // we are not concerned by ram nor time, let's decompress the whole node
  std::vector<char> tmp;
  tmp.resize(XLZ4_CHUNK_SIZE);
  nodedata.clear();
  nodedata.resize(nodedata_size);
  for (int i = 0; i < chunk_descs.size(); ++i)
  {
    auto& cd = chunk_descs[i];

    ifs.seekg(cd.offset, ifs.beg);
    ifs.read((char*)&magic, 4);
    if (magic != 'XLZ4')
      return false;

    uint32_t data_size = 0;
    ifs.read((char*)&data_size, 4);
    if (data_size != cd.data_size)
      return false;

    size_t csize = cd.size-8;
    if (csize > tmp.size())
      tmp.resize(csize);
    ifs.read(tmp.data(), csize);

    int res = LZ4_decompress_safe(tmp.data(), nodedata.data() + cd.data_offset, (int)csize, cd.data_size);
    if (res != cd.data_size)
      return false;

    progress = 0.5f + 0.3f * i / (float)cd_cnt;
  }

  if (ifs.fail())
    return false;
  ifs.close();
  progress = 0.8f;

  // check that each blob starts with its node index (dword)
  i = 0;
  for (auto& nd : node_descs)
  {
    if (*(uint32_t*)(nodedata.data() + nd.data_offset) != i++)
      return false;
  }

  // --------------------------------------------------------
  //  UNFLATTENING of node tree
  // --------------------------------------------------------

  // fake descriptor, our buffer should be prefixed with zeroes so the *data==idx will pass..
  uint32_t data_size = (uint32_t)nodedata.size() - chunks_start;
  node_desc root_desc {"root", NULL_NODE_IDX, 0, chunks_start-4, data_size+4};
  root_node = read_node(nodedata, root_desc, ROOT_NODE_IDX);
  if (!root_node)
    return false;

  auto tree_size = root_node->calcsize();
  if (tree_size != data_size) // check the unflattening worked
    return false;

  progress = 1.f;
  return true;
}

bool csav::save_with_progress(std::filesystem::path path, float& progress)
{
  if (!root_node)
    return false;

  uint32_t chunkdescs_start = 0;
  uint32_t chunks_start = 0;
  uint32_t nodedescs_start = 0;
  uint32_t magic = 0;
  uint32_t i = 0;

  std::vector<compressed_chunk_desc> chunk_descs;
  std::vector<char> nodedata;

  progress = 0.05f;

  if (std::filesystem::exists(path))
  {
    auto oldpath = path;
    oldpath.replace_extension(L".old");
    if (!std::filesystem::exists(oldpath)) // only make one when there isn't one, oldest wins for safety reasons
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
  ofs.write((char*)&magic, 4);

  progress = 0.1f;

  ofs.write((char*)&v1, 4);
  ofs.write((char*)&v2, 4);
  write_str(ofs, suk);
  ofs.write((char*)&uk0, 4);
  ofs.write((char*)&uk1, 4);

  if (v1 >= 83)
    ofs.write((char*)&v3, 4);

  progress = 0.15f;

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
  ofs.write(tmp.data(), chunktbl_maxsize);
  chunks_start = (uint32_t)ofs.tellp();

  // --------------------------------------------------------
  //  FLATTENING of node tree
  // --------------------------------------------------------

  // yes that looks dumb, but cdpred use first data_offset = min_offset
  // so before creating the node descriptors i fill the buffer to min_offset
  nodedata.resize(chunks_start);

  auto& nc_root = root_node->nonconst();
  uint32_t node_cnt = root_node->treecount();

  node_descs.resize(node_cnt);
  uint32_t next_idx = 0;
  write_node_children(nodedata, *root_node, next_idx);

  // check that each blob starts with its node index (dword)
  i = 0;
  for (auto& ed : node_descs)
  {
    if (*(uint32_t*)(nodedata.data() + ed.data_offset) != i++)
      return false;
  }

  progress = 0.35f;

  // --------------------------------------------------------
  //  COMPRESSION from nodedata to compressed chunks
  // --------------------------------------------------------

  // chunks

  char* const ptmp = tmp.data();
  char* const prealbeg = nodedata.data();
  char* const pbeg = prealbeg + chunks_start; // compression starts at min_offset!
  char* const pend = prealbeg + nodedata.size();
  char* pcur = pbeg;

  i = 1;
  while (pcur < pend)
  {
    auto& chunk_desc = chunk_descs.emplace_back();
    chunk_desc.data_offset = (uint32_t)(pcur - prealbeg);
    chunk_desc.offset = (uint32_t)ofs.tellp();

    int srcsize = (int)(pend - pcur);
    int csize = LZ4_compress_destSize(pcur, ptmp, &srcsize, XLZ4_CHUNK_SIZE);
    if (csize < 0)
      return false;

    pcur += srcsize;

    magic = 'XLZ4';
    ofs.write((char*)&magic, 4);

    uint32_t data_size = 0;
    ofs.write((char*)&srcsize, 4);

    ofs.write(ptmp, csize);

    chunk_desc.size = csize+8;
    chunk_desc.data_size = srcsize;

    progress = 0.35f + ((float)i / max_chunkcnt) * 0.4f;
  }
  if (pcur > pend)
    return false;

  progress = 0.75f;

  nodedescs_start = (uint32_t)ofs.tellp();

  // descriptors

  ofs.seekp(chunkdescs_start);

  magic = 'CLZF';
  ofs.write((char*)&magic, 4);
  uint32_t cd_cnt = (uint32_t)chunk_descs.size();
  ofs.write((char*)&cd_cnt, 4);

  for (uint32_t i = 0; i < cd_cnt; ++i)
  {
    ofs << chunk_descs[i];
    progress = 0.75f + 0.1f * i / (float)cd_cnt;
  }

  progress = 0.85f;

  // --------------------------------------------------------
  //  NODE DESCRIPTORS
  // --------------------------------------------------------

  ofs.seekp(nodedescs_start);

  magic = 'NODE';
  ofs.write((char*)&magic, 4);

  // now write node descs
  write_packed_int(ofs, (int64_t)node_cnt);
  for (uint32_t i = 0; i < node_cnt; ++i)
  {
    ofs << node_descs[i];
    progress = 0.85f + 0.1f * i / (float)node_cnt;
  }

  progress = 0.95f;

  // --------------------------------------------------------
  //  FOOTER (offset of 'NODE', 'DONE' tag)
  // --------------------------------------------------------

  // end stuff
  ofs.write((char*)&nodedescs_start, 4);
  magic = 'DONE';
  ofs.write((char*)&magic, 4);

  if (ofs.fail())
    return false;
  ofs.close();

  // --------------------------------------------------------
  //  save decompressed blob (request)
  // --------------------------------------------------------

  auto dump_path = path;
  dump_path.replace_filename(L"decompressed_blob.bin");
  ofs.open(dump_path, ofs.binary | ofs.trunc);
  ofs.write(nodedata.data(), nodedata.size());
  ofs.close();

  progress = 1.f;
  return true;
}

