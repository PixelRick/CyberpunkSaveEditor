#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <numeric>
#include <list>
#include <appbase/widgets/cpinternals.hpp>
#include "cpinternals/scripting/csystem.hpp"
#include <cpinternals/common.hpp>
#include <cpinternals/io/stdstream_wrapper.hpp>

class archive_test
{
  bool opened = false;

  std::filesystem::path m_path;
  bool saved = false;
  bool save_failed = false;

public:
  bool open(std::filesystem::path path)
  {
    opened = false;
    saved = false;
    m_path = path;

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

  bool save()
  {
    saved = true;
    if (!opened)
      return false;

    std::ofstream ofs;
    ofs.open(m_path, ofs.out | ofs.binary);
    if (ofs.fail())
    {
      std::string err = strerror(errno);
      std::cerr << "Error: " << err;
      return false;
    }

    if (!serialize_out(ofs))
      return false;

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

      if (ImGui::Button("SAVE"))
      {
        save_failed = !save();
      }
      if (saved)
      {
        ImGui::SameLine();
        ImGui::Text(save_failed ? "failed" : "ok");
      }

      //ImGui::Text("section 2 (sometimes filename): %s", m_filename.c_str());
      ImGui::Text(fmt::format("m_uk1: {:04X}", m_uk1).c_str());


      ImGui::BeginGroup();

      {
        ImGui::Text("embedded cruids:");
        for (auto& cruid : m_ids)
          ImGui::Text(fmt::format(" {:016X}", cruid).c_str());
      }

      ImGui::EndGroup();
      ImGui::SameLine();
      ImGui::BeginGroup();

      static ImGuiTabBarFlags tab_bar_flags =
      //ImGuiTabBarFlags_Reorderable |
      //ImGuiTabBarFlags_AutoSelectNewTabs |
      ImGuiTabBarFlags_FittingPolicyResizeDown;

      if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
      {
        
        if (ImGui::BeginTabItem("rarefs", 0, ImGuiTabItemFlags_None))
        {
          //ImGui::BeginChild("current editor", ImVec2(0, 0), false, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse);

          static auto name_fn = [](const cp::cname& y) { return y.string(); };
          imgui_list_tree_widget(m_rarefs, name_fn, &CName_widget::draw_, 0, true, true);

          //ImGui::EndChild();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("objects", 0, ImGuiTabItemFlags_None))
        {
          static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
        | ImGuiTableFlags_Resizable;

          ImVec2 size = ImVec2(-FLT_MIN, -FLT_MIN);
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

          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

      ImGui::EndGroup();

      ImGui::End();
    }

    return modified;
  }

  struct header_t
  {
    uint16_t uk1                    = 0;
    uint16_t uk2                    = 0; // sections count apparently
    uint32_t ids_cnt                = 0;
    // test file has 7 sections
    uint32_t rarefpool_descs_offset = 0;
    uint32_t rarefpool_data_offset  = 0;
    uint32_t strpool_descs_offset   = 0;
    uint32_t strpool_data_offset    = 0;
    uint32_t obj_descs_offset       = 0;
    // tricky: obj offsets are relative to the strpool base
    uint32_t objdata_offset         = 0; 
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
  uint16_t m_uk1;

  std::vector<uint64_t> m_ids;

  std::list<cname> m_rarefs;

  CSystemSerCtx m_serctx; // strpool + handles

  std::vector<CObjectSPtr> m_objects;
  std::vector<CObjectSPtr> m_handle_objects;

public:
  const std::vector<CObjectSPtr>& objects() const { return m_objects; }
  std::vector<CObjectSPtr>& objects()       { return m_objects; }

public:

  // this is redundant, should make stringpool serializable with template descriptor..
  class path_desc
  {
    uint32_t m_meta = 0;
  
  public:
    constexpr static size_t serial_size = sizeof(uint32_t);
  
    path_desc() = default;
  
    path_desc(uint32_t offset, uint32_t len)
    {
      this->offset(offset);
      this->len(len);
    }
  
    uint32_t offset() const { return m_meta & 0xFFFF; }
    uint32_t len() const { return (m_meta >> 23) & 0x1FF; }
  
    uint32_t end_offset() const { return offset() + len(); }
  
    void offset(uint32_t value)
    {
      if (value > 0xFFFF)
        throw std::range_error("path_desc: offset is too big");
      m_meta = value | (m_meta & 0xFFFF0000);
    }
  
    void len(uint32_t value)
    {
      if (value > 0x1FF)
        throw std::range_error("path_desc: len is too big");
      m_meta = (value << 23) | (m_meta & 0x007FFFFF);
    }
  
    uint32_t as_u32() const { return m_meta; }
  };

  bool serialize_in(std::istream& reader, size_t blob_size)
  {
    size_t start_pos = (size_t)reader.tellg();

    m_ids.clear();
    m_rarefs.clear();
    m_objects.clear();
    m_handle_objects.clear();

    // let's get our header start position
    auto blob_spos = reader.tellg();

    reader >> cbytes_ref(m_header);

    // check header
    if (m_header.rarefpool_descs_offset > m_header.rarefpool_data_offset)
      return false;
    if (m_header.rarefpool_data_offset > m_header.strpool_descs_offset)
      return false;
    if (m_header.strpool_descs_offset > m_header.strpool_data_offset)
      return false;
    if (m_header.strpool_data_offset > m_header.obj_descs_offset)
      return false;
    if (m_header.obj_descs_offset > m_header.objdata_offset)
      return false;

    if (m_header.rarefpool_descs_offset != 0)
      throw std::exception("m_header.u32arr_offset != 0, not cool :(");


    reader >> cbytes_ref(m_uk1);
    //if (uk1 != 0)
    //  throw std::exception("uk1 != 0, cool :)");

    uint16_t ids_cnt = 0;
    reader >> cbytes_ref(ids_cnt);

    if (ids_cnt != m_header.ids_cnt)
      throw std::exception("ids_cnt != m_header.ids_cnt, not cool :(");

    m_ids.resize(ids_cnt);
    reader.read((char*)m_ids.data(), m_header.ids_cnt * 8);

    // end of header
    const size_t base_offset = (size_t)reader.tellg() - start_pos;

    // rarefs
    
    const uint32_t rarefpool_descs_size = m_header.rarefpool_data_offset - m_header.rarefpool_descs_offset;
    const uint32_t rarefpool_data_size = m_header.strpool_descs_offset - m_header.rarefpool_data_offset;

    cp::stdstream_wrapper<std::istream> ar(reader);

    // std::vector<std::string> m_rarefs_strs;
    // std::vector<CName> m_rarefs_hashes;

    if (m_uk1 == 0) // cname as strings
    {
      cnameset np;
      np.serialize_in<9>(ar, m_header.rarefpool_descs_offset, rarefpool_descs_size, rarefpool_data_size);

      //m_rarefs.reserve(np.size());
      for (auto& cn : np)
      {
        m_rarefs.emplace_back(cn);
      }
    }
    else // cname as hashes (but pooled.. lol)
    {
      const size_t descs_cnt = rarefpool_descs_size / 4;
      std::vector<uint32_t> descs(descs_cnt);
      ar.serialize_pods_array_raw(descs.data(), descs_cnt);

      std::vector<uint64_t> hashes(descs_cnt);
      ar.serialize_pods_array_raw(hashes.data(), descs_cnt);

      for (auto& hash : hashes)
      {
        cname cn(hash);
        m_rarefs.emplace_back(hash);
      }
    }

    // section 3+4: string pool
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
      return m_header.objdata_offset + base_offset == blob_size; // could be empty

    //std::vector<obj_desc_t> obj_descs(obj_descs_cnt);
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

  bool serialize_out(std::ostream& writer) const
  {
    // let's not do too many copies

    auto start_spos = writer.tellp();
    uint32_t blob_size = 0; // we don't know it yet
    
    writer << cbytes_ref(m_header); 

    header_t new_header = m_header;

    // ----------------------------------------------

    writer << cbytes_ref(m_uk1);

    uint16_t ids_cnt = (uint16_t)m_ids.size();
    writer << cbytes_ref(ids_cnt);
    writer.write((char*)m_ids.data(), ids_cnt * 8);

    new_header.ids_cnt = ids_cnt;

    // end of header

    const size_t base_spos = (size_t)writer.tellp();
    const size_t base_offset = (size_t)writer.tellp() - start_spos;

    // rarefs

    new_header.rarefpool_descs_offset = 0;

    cp::stdstream_wrapper<std::ostream> ar(writer);

    uint32_t rarefpool_descs_size = 0;
    uint32_t rarefpool_data_size = 0;

    cnameset np;

    for (auto& cn : m_rarefs)
    {
      np.insert(cn);
    }

    if (m_uk1 == 0)
    {
      np.serialize_out<9>(ar, new_header.rarefpool_descs_offset, rarefpool_descs_size, rarefpool_data_size);
    }
    else // cname as hashes (but pooled.. lol)
    {
      const uint32_t descs_cnt = static_cast<uint32_t>(m_rarefs.size());
      rarefpool_descs_size = descs_cnt * 4;
      rarefpool_data_size = descs_cnt * 8;

      std::vector<uint32_t> descs; descs.reserve(descs_cnt);
      std::vector<uint64_t> hashes; hashes.reserve(descs_cnt);

      uint32_t off = new_header.rarefpool_descs_offset + rarefpool_descs_size;

      for (auto& cn : m_rarefs)
      {
        constexpr uint32_t desc_size_component = (8 << (32 - 9));
        descs.emplace_back(desc_size_component | off);
        off += 8;
        hashes.emplace_back(cn.hash);
      }

      ar.serialize_pods_array_raw(descs.data(), descs_cnt);
      ar.serialize_pods_array_raw(hashes.data(), descs_cnt);
    }

    new_header.rarefpool_data_offset = new_header.rarefpool_descs_offset + rarefpool_descs_size;

    // section 3+4: string pool
    const uint32_t strpool_descs_offset = (uint32_t)((size_t)writer.tellp() - base_spos);
    new_header.strpool_descs_offset = (uint32_t)strpool_descs_offset;

    // ----------------------------------------------

    //CStringPool strpool;
    CSystemSerCtx& serctx = const_cast<CSystemSerCtx&>(m_serctx);
    // here we can't really remove handle-objects because there might be hidden handles in unsupported types
    serctx.m_objects.assign(m_objects.begin(), m_objects.end());
    serctx.m_objects.insert(serctx.m_objects.end(), m_handle_objects.begin(), m_handle_objects.end());
    serctx.rebuild_handlemap();

    std::ostringstream ss;
    std::vector<obj_desc_t> obj_descs;
    obj_descs.reserve(serctx.m_objects.size()); // ends up higher in the presence of handles

    // serctx.m_objects is extended during object serialization (handles)
    for (size_t i = 0; i < serctx.m_objects.size(); ++i)
    {
      auto& obj = serctx.m_objects[i];
      const uint32_t tmp_offset = (uint32_t)ss.tellp();
      const uint16_t name_idx = serctx.strpool.to_idx(obj->ctypename().c_str());
      obj_descs.emplace_back(name_idx, tmp_offset);
      if (!obj->serialize_out(ss, serctx))
        return false;
    }

    // time to write strpool
    uint32_t strpool_data_size = 0;
    uint32_t strpool_descs_size = 0;
    if (!serctx.strpool.serialize_out(writer, strpool_descs_size, strpool_data_size, strpool_descs_offset))
      return false;

    new_header.strpool_data_offset = strpool_descs_offset + strpool_descs_size;

    new_header.obj_descs_offset = new_header.strpool_data_offset + strpool_data_size;

    // reoffset offsets
    const uint32_t obj_descs_size = (uint32_t)(obj_descs.size() * sizeof(obj_desc_t));
    const uint32_t objdata_offset = new_header.obj_descs_offset + obj_descs_size;

    new_header.objdata_offset = objdata_offset;

    // reoffset offsets
    for (auto& desc : obj_descs)
      desc.data_offset += objdata_offset;

    // write obj descs + data
    writer.write((char*)obj_descs.data(), obj_descs_size);
    auto objdata = ss.str(); // not optimal but didn't want to depend on boost and no time to dev a mem stream for now
    writer.write(objdata.data(), objdata.size());
    auto end_spos = writer.tellp();

    // at this point data should be correct except blob_size and header
    // so let's rewrite them

    writer.seekp(start_spos);
    writer << cbytes_ref(new_header); 

    // return to end of data
    writer.seekp(end_spos);

    return true;
  }
};


