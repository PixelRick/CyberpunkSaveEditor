// Thanks to novomurdo who reversed the tweakdb.bin format

#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <numeric>
#include "cpinternals/common.hpp"
#include "cpinternals/ctypes.hpp"
#include "cpinternals/scripting.hpp"



namespace cp::tdb {

struct tweakdb
{
  tweakdb()
  {
    // Add some types to the CNames
    auto& resolver = CName_resolver::get();
    resolver.register_name("String");
    resolver.register_name("Quaternion");
    resolver.register_name("array:Bool");
    resolver.register_name("array:CName");
    resolver.register_name("array:Int32");
    resolver.register_name("array:raRef:CResource");
    resolver.register_name("array:Vector2");
    resolver.register_name("array:String");
    resolver.register_name("array:Float");
    resolver.register_name("array:Vector3");
    resolver.register_name("array:TweakDBID");
    resolver.register_name("Vector3");
    resolver.register_name("EulerAngles");
    resolver.register_name("Vector2");
    resolver.register_name("TweakDBID");
    resolver.register_name("CName");
    resolver.register_name("raRef:CResource");
    resolver.register_name("gamedataLocKeyWrapper");
    resolver.register_name("Float");
    resolver.register_name("Int32");
    resolver.register_name("Bool");
  }

  bool open(std::filesystem::path path)
  {
    std::ifstream ifs;
    ifs.open(path, ifs.in | ifs.binary);
    if (ifs.fail())
    {
      std::string err = strerror(errno);
      std::cerr << "Error: " << err;
      return false;
    }

    ifs.seekg(0, std::ios_base::end);
    uint32_t blob_size = (uint32_t)ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);

    if (!serialize_in(ifs, blob_size))
      return false;

    opened = true;
    return true;
  }

protected:
  static std::string object_name_getter(const CObjectSPtr& item)
  {
    static std::string tmp;
    tmp = item->ctypename().str();
    return tmp;
  };
  int selected = -1;

public:
  bool imgui_draw()
  {
    bool modified = false;

    if (opened)
    {
      ImGui::Begin("tweadb test", &opened);

      ImGui::BeginGroup();
      {
        ImGui::Text("pool descs:");
        ImGui::ListBox("pool descs", &selected, [](const pool_desc_t& pd) -> std::string {
          return fmt::format("{}[{}]", pd.ctypename.str(), pd.len);
        }, m_pools_descs);
      }
      ImGui::EndGroup();

      ImGui::End();
    }

    return modified;
  }

protected:
  struct header_t
  {
    uint32_t magic                = 0;
    uint32_t five                 = 0;
    uint32_t four                 = 0;
    uint32_t version_hash         = 0; // Hash of all typenames in groups order
    uint32_t variables_offset     = 0;
    uint32_t groups_offset        = 0;
    uint32_t inlgroups_offset     = 0;
    uint32_t packages_offset      = 0;
  };

#pragma pack (push, 1)
  struct alignas(4) pool_desc_t
  {
    CName ctypename;
    uint32_t len;
  };
#pragma pack (pop)

  static_assert(sizeof(pool_desc_t) == 0xC);
  static_assert(alignof(pool_desc_t) == 0x4);

public:
  bool serialize_in(std::istream& reader, size_t blob_size)
  {
    // let's get our header start position
    size_t start_pos = (size_t)reader.tellg();

    reader >> cbytes_ref(m_header);

    auto blob_spos = reader.tellg();

    // check header
    if (m_header.five != 5 || m_header.four != 4)
      return false;
    if (m_header.variables_offset > m_header.groups_offset)
      return false;
    if (m_header.groups_offset > m_header.inlgroups_offset)
      return false;
    if (m_header.inlgroups_offset > m_header.packages_offset)
      return false;
    if (m_header.packages_offset > blob_size)
      return false;

    uint32_t arrays_cnt = 0;
    reader >> cbytes_ref(arrays_cnt);

    m_pools_descs.resize(arrays_cnt);
    reader.read((char*)m_pools_descs.data(), arrays_cnt * sizeof(pool_desc_t));

    // pools data (prefixed again with size)
    // TODO

    size_t new_pos = (size_t)reader.tellg();

    return true;
  }

private:
  bool opened = false;

  header_t m_header;
  std::vector<pool_desc_t> m_pools_descs;
};

} // namespace cp::tdb

