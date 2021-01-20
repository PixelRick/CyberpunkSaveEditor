#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <numeric>
#include "cpinternals/scripting/csystem.hpp"


class archive_test
{
  bool opened = false;

public:
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

  int selected = -1;

  static std::string object_name_getter(const CObjectSPtr& item)
  {
    return std::string(item->ctypename().strv());
  };

  bool imgui_draw()
  {
    bool modified = false;

    if (opened)
    {
      ImGui::Begin("archive_test", &opened);
      ImGuiWindow* window = ImGui::GetCurrentWindow();
      if (window->SkipItems)
        return false;

      auto& objects = m_objects;

      ImGui::Text("embedded filename: %s", m_filename.c_str());

      ImGui::BeginGroup();
      {
        ImGui::Text("embedded cruids:");
        for (auto& cruid : m_ids)
          ImGui::Text(fmt::format(" {:016X}", cruid).c_str());
      }
      {
        ImGui::Text("u32 array:");
        for (auto& u32 : m_u32_array)
          ImGui::Text(fmt::format(" {:08X}", u32).c_str());
      }
      ImGui::EndGroup();
      ImGui::SameLine();

      static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
        | ImGuiTableFlags_Resizable;

      ImVec2 size = ImVec2(-FLT_MIN, ImGui::GetContentRegionAvail().y);
      if (ImGui::BeginTable("##props", 2, tbl_flags, size))
      {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("objects", ImGuiTableColumnFlags_WidthFixed, 230.f);
        ImGui::TableSetupColumn("selected object", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        //ImGui::PushItemWidth(150.f);
        ImGui::BeginChild("Objects", ImVec2(-FLT_MIN, 0));
        modified |= ImGui::ErasableListBox("##CSystem::objects", &selected, &object_name_getter, objects, true, true);
        ImGui::EndChild();
        //ImGui::PopItemWidth();

        ImGui::TableNextColumn();

        ImGui::BeginChild("Selected Object");

        int object_idx = selected;
        if (object_idx >= 0 && object_idx < objects.size())
        {
          auto& obj = objects[object_idx];
          ImGui::PushItemWidth(300.f);
          auto ctype = obj->ctypename();
          ImGui::Text("object type: %s", ctype.c_str());
          ImGui::PopItemWidth();
          modified |= obj->imgui_widget(ctype.c_str(), true);
        }
        else
          ImGui::Text("no selected object");

        ImGui::EndChild();

        ImGui::EndTable();
      }
      
      ImGui::End();
    }

    return modified;
  }

  struct header_t
  {
    uint16_t uk1                  = 0;
    uint16_t uk2                  = 0; // sections count apparently
    uint32_t ids_cnt              = 0;
    // test file has 7 sections
    uint32_t uk3                  = 0; // array of u32
    uint32_t filename_offset      = 0;
    uint32_t strpool_descs_offset = 0;
    uint32_t strpool_data_offset  = 0;
    uint32_t obj_descs_offset     = 0;
    // tricky: obj offsets are relative to the strpool base
    uint32_t objdata_offset       = 0; 
  };

  struct obj_desc_t
  {
    obj_desc_t() = default;

    obj_desc_t(uint32_t name_idx, uint32_t data_offset)
      : name_idx(name_idx), data_offset(data_offset) {}

    uint32_t name_idx     = 0;
    uint32_t data_offset  = 0;// relative to the end of the header in stream
  };

private:
  header_t m_header;

  std::vector<uint64_t> m_ids;
  std::vector<uint32_t> m_u32_array;
  std::string m_filename;

  CSystemSerCtx m_serctx; // strpool + handles

  std::vector<CObjectSPtr> m_objects;
  std::vector<CObjectSPtr> m_handle_objects;

public:
  const std::vector<CObjectSPtr>& objects() const { return m_objects; }
  std::vector<CObjectSPtr>& objects()       { return m_objects; }

public:
  bool serialize_in(std::istream& reader, size_t blob_size)
  {
    size_t start_pos = (size_t)reader.tellg();

    m_ids.clear();
    m_u32_array.clear();
    m_objects.clear();
    m_handle_objects.clear();

    // let's get our header start position
    auto blob_spos = reader.tellg();

    reader >> cbytes_ref(m_header);

    // check header
    if (m_header.uk3 > m_header.filename_offset)
      return false;
    if (m_header.filename_offset > m_header.strpool_descs_offset)
      return false;
    if (m_header.strpool_descs_offset > m_header.strpool_data_offset)
      return false;
    if (m_header.strpool_data_offset > m_header.obj_descs_offset)
      return false;
    if (m_header.obj_descs_offset > m_header.objdata_offset)
      return false;

    if (m_header.uk3 != 0)
      throw std::exception("m_header.uk3 != 0, not cool :(");


    uint16_t uk1 = 0;
    reader >> cbytes_ref(uk1);
    if (uk1 != 0)
      throw std::exception("uk1 != 0, cool :)");

    uint16_t ids_cnt = 0;
    reader >> cbytes_ref(ids_cnt);

    if (ids_cnt != m_header.ids_cnt)
      throw std::exception("ids_cnt != m_header.ids_cnt, not cool :(");

    m_ids.resize(ids_cnt);
    reader.read((char*)m_ids.data(), m_header.ids_cnt * 8);

    // end of header
    const size_t base_offset = (size_t)reader.tellg() - start_pos;

    // section 1: array of u32
    const size_t sec1_size = m_header.filename_offset - m_header.uk3;
    if (sec1_size % 4)
      throw std::exception("sec1_size % 4, not cool :(");
    const size_t sec1_len = sec1_size / 4;
    m_u32_array.resize(sec1_len);
    reader.read((char*)m_u32_array.data(), sec1_size);

    // section 2: filename
    const size_t sec2_size = m_header.strpool_descs_offset - m_header.filename_offset;
    m_filename.resize(sec2_size);
    reader.read(m_filename.data(), sec2_size);

    // section 3+4:  string pool
    const uint32_t strpool_descs_size = m_header.strpool_data_offset - m_header.strpool_descs_offset;
    const uint32_t strpool_data_size = m_header.obj_descs_offset - m_header.strpool_data_offset;

    //CStringPool strpool;
    CStringPool& strpool = m_serctx.strpool;
    if (!strpool.serialize_in(reader, strpool_descs_size, strpool_data_size, m_header.strpool_descs_offset))
      return false;

    // now let's read objects

    // we don't have the impl for all props
    // so we'll use the assumption that everything is serialized in
    // order and use the offset of next item as end of blob

    const size_t obj_descs_offset = m_header.obj_descs_offset;
    const size_t obj_descs_size = m_header.objdata_offset - obj_descs_offset;
    if (obj_descs_size % sizeof(obj_desc_t) != 0)
      return false;

    const size_t obj_descs_cnt = obj_descs_size / sizeof(obj_desc_t);
    if (obj_descs_cnt == 0)
      return true;

    std::vector<obj_desc_t> obj_descs(obj_descs_cnt);
    reader.read((char*)obj_descs.data(), obj_descs_size);

    // read objdata
    const size_t objdata_size = blob_size - (base_offset + m_header.objdata_offset); 

    if (base_offset + m_header.objdata_offset != (reader.tellg() - blob_spos))
      return false;

    std::vector<char> objdata(objdata_size);
    reader.read((char*)objdata.data(), objdata_size);

    if (blob_size != (reader.tellg() - blob_spos))
      return false;

    // prepare default initialized objects
    m_serctx.m_objects.clear();
    m_serctx.m_objects.reserve(obj_descs.size());
    for (auto it = obj_descs.begin(); it != obj_descs.end(); ++it)
    {
      const auto& desc = *it;

      // check desc is valid
      if (desc.data_offset < m_header.objdata_offset)
        return false;

      auto obj_ctypename = gname(strpool.from_idx(desc.name_idx));
      auto new_obj = std::make_shared<CObject>(obj_ctypename, true); // todo: static create method
      m_serctx.m_objects.push_back(new_obj);
    }

    // here the offsets relative to base_offset are converted to offsets relative to objdata
    size_t next_obj_offset = objdata_size;
    auto obj_it = m_serctx.m_objects.rbegin();
    for (auto it = obj_descs.rbegin(); it != obj_descs.rend(); ++it, ++obj_it)
    {
      const auto& desc = *it;

      const size_t offset = desc.data_offset - m_header.objdata_offset;
      if (offset > next_obj_offset)
        throw std::logic_error("CSystem: false assumption #2. please open an issue.");

      std::span<char> objblob((char*)objdata.data() + offset, next_obj_offset - offset);
      if (!(*obj_it)->serialize_in(objblob, m_serctx))
        return false;

      next_obj_offset = offset;
    }

    const auto& serobjs = m_serctx.m_objects;
    m_objects.assign(serobjs.begin(), serobjs.end());

    return true;
  }
};


