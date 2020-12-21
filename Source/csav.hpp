#include <filesystem>
#include <iostream>
#include <fstream>
#include <numeric>
#include <cassert>
#include "xlz4/lz4.h"

#define XLZ4_CHUNK_SIZE 0x40000

int64_t read_packed_int(std::istream& is);
void write_packed_int(std::ostream& os, int64_t value);

std::string read_str(std::istream& is);
void write_str(std::ostream& os, const std::string& s);

// let's keep it simple for now
// children must keep the original ordering, except for arrays (items)
struct node_t
{
  // empty name means blob
  std::string name;
  std::vector<std::shared_ptr<node_t>> children;
  std::vector<char> data;
  int32_t idx;

  size_t calcsize() const
  {
    size_t base_size = data.size() + (idx >= 0 ? 4 : 0);
    return std::accumulate(
      children.begin(), children.end(), base_size,
      [](size_t cnt, auto& node){ return cnt + node->calcsize(); }
    );
  }

  uint32_t treecount() const
  {
    if (idx >= 0) {
      return std::accumulate(
        children.begin(), children.end(), (uint32_t)1,
        [](uint32_t cnt, auto& node){ return cnt + node->treecount(); }
      );
    }
    return 0;
  }
};

struct node_desc
{
  std::string name;
  int32_t next_idx, child_idx;
  uint32_t data_offset, data_size;

  friend std::istream& operator>>(std::istream& is, node_desc& ed)
  {
    ed.name = read_str(is);
    is.read((char*)&ed.next_idx, 16);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const node_desc& ed)
  {
    write_str(os, ed.name);
    os.write((char*)&ed.next_idx, 16);
    return os;
  }
};

struct compressed_chunk_desc
{
  static const size_t serialized_size = 12;

  // data_size is uncompressed size
  uint32_t offset, size, data_size, data_offset;

  friend std::istream& operator>>(std::istream& is, compressed_chunk_desc& cd)
  {
    is.read((char*)&cd.offset, 12);
    cd.data_offset = 0;
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const compressed_chunk_desc& cd)
  {
    os.write((char*)&cd.offset, 12);
    return os;
  }
};

class csav
{
public:
  std::filesystem::path filepath;
  uint32_t v1, v2, v3, uk0, uk1;
  std::string suk;
  std::vector<compressed_chunk_desc> chunk_descs;
  std::vector<char> nodedata;
  std::vector<node_desc> node_descs;

  std::shared_ptr<node_t> root_node;

public:
  bool open_with_progress(std::filesystem::path path, float& progress)
  {
    uint32_t chunkdescs_start = 0;
    uint32_t nodedescs_start = 0;
    uint32_t magic = 0;
    uint32_t i = 0;

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
    node_desc root_desc {"root", -1, 0, chunks_start-4, data_size+4};
    root_node = read_node(root_desc, 0);
    if (!root_node)
      return false;
    root_node->idx = -1;

    auto tree_size = root_node->calcsize();
    if (tree_size != data_size) // check the unflattening worked
      return false;

    progress = 1.f;
    return true;
  }

protected:
  std::shared_ptr<node_t> make_blob_node(uint32_t start_offset, uint32_t end_offset)
  {
    auto node = std::make_shared<node_t>();
    node->name = "datablob";
    node->idx = -1;

    node->data.assign(
      nodedata.begin() + start_offset,
      nodedata.begin() + end_offset
    );

    return node;
  }

  std::shared_ptr<node_t> read_node(node_desc& desc, uint32_t idx)
  {
    uint32_t cur_offset = desc.data_offset + 4;
    uint32_t end_offset = desc.data_offset + desc.data_size;

    if (end_offset > nodedata.size())
      return nullptr;
    if (*(uint32_t*)(nodedata.data() + desc.data_offset) != idx)
      return nullptr;

    auto node = std::make_shared<node_t>();
    node->name = desc.name;
    node->idx = idx;

    if (desc.child_idx >= 0)
    {
      int last, i = desc.child_idx;
      while (i >= 0)
      {
        last = i;
        if (i >= node_descs.size()) // corruption ?
          return nullptr;

        auto& childdesc = node_descs[i];

        if (childdesc.data_offset > cur_offset) {
          node->children.push_back(
            make_blob_node(cur_offset, childdesc.data_offset)
          );
        }

        auto childnode = read_node(childdesc, i);
        if (!childnode) // something went wrong
          return nullptr;
        node->children.push_back(childnode);

        cur_offset = childdesc.data_offset + childdesc.data_size;
        i = childdesc.next_idx;
      }

      if (cur_offset < end_offset) {
        node->children.push_back(
          make_blob_node(cur_offset, end_offset)
        );
      }
    }
    else if (cur_offset < end_offset)
    {
      node->data.assign(
        nodedata.begin() + cur_offset,
        nodedata.begin() + end_offset
      );
    }

    return node;
  }

public:
  bool save_with_progress(std::filesystem::path path, float& progress)
  {
    if (!root_node)
      return false;

    uint32_t chunkdescs_start = 0;
    uint32_t chunks_start = 0;
    uint32_t nodedescs_start = 0;
    uint32_t magic = 0;
    uint32_t i = 0;

    chunk_descs.clear();
    nodedata.clear();
    node_descs.clear();

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

    root_node->idx = 0;
    uint32_t nd_cnt = root_node->treecount() - 1;
    root_node->idx = -1;

    node_descs.resize(nd_cnt);
    uint32_t next_idx = 0;
    write_node_children(root_node, next_idx);

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
    write_packed_int(ofs, (int64_t)nd_cnt);
    for (uint32_t i = 0; i < nd_cnt; ++i)
    {
      ofs << node_descs[i];
      progress = 0.85f + 0.1f * i / (float)nd_cnt;
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

    progress = 1.f;
    return true;
  }

protected:
  node_desc* write_node_visitor(const std::shared_ptr<node_t>& node, uint32_t& next_idx)
  {
    if (node->idx >= 0)
    {
      const uint32_t idx = next_idx++;
      node->idx = idx;
      auto& nd = node_descs[idx];
      nd.name = node->name;
      nd.data_offset = (uint32_t)nodedata.size();
      nd.child_idx = node->children.empty() ? -1 : next_idx;

      char* pIdx = (char*)&node->idx;
      std::copy(pIdx, pIdx + 4, std::back_inserter(nodedata));
      std::copy(node->data.begin(), node->data.end(), std::back_inserter(nodedata));

      write_node_children(node, next_idx);

      nd.next_idx = next_idx < node_descs.size() ? next_idx : -1;
      nd.data_size = (uint32_t)nodedata.size() - nd.data_offset;
      return &nd;
    }
    else
    {
      // data blob
      std::copy(node->data.begin(), node->data.end(), std::back_inserter(nodedata));
    }
    return nullptr;
  }

  void write_node_children(const std::shared_ptr<node_t>& node, uint32_t& next_idx)
  {
    node_desc* last_child_desc = nullptr;
    for (auto& c : node->children)
    {
      auto cnd = write_node_visitor(c, next_idx);
      if (cnd != nullptr)
        last_child_desc = cnd;
    }
    if (last_child_desc)
      last_child_desc->next_idx = -1;
  }
};

