#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <numeric>
#include <list>
#include <appbase/widgets/redx.hpp>
#include "redx/scripting/csystem.hpp"
#include <redx/core.hpp>
#include <redx/io/bstream.hpp>
#include <redx/serialization/cnames_blob.hpp>

#include <redx/containers/bitfield.hpp>
#include <redx/serialization/resids_blob.hpp>
#include <redx/io/mem_bstream.hpp>


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

    // create back-up copy
    std::filesystem::path oldpath = m_path;
    oldpath.replace_extension(L".old");
    if (!std::filesystem::exists(oldpath)) 
    std::filesystem::copy(m_path, oldpath);

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
        
        //if (ImGui::BeginTabItem("rarefs", 0, ImGuiTabItemFlags_None))
        //{
        //  //ImGui::BeginChild("current editor", ImVec2(0, 0), false, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse);

        //  static auto name_fn = [](const redx::cname& y) { return y.string(); };
        //  imgui_list_tree_widget(m_resids, name_fn, &CName_widget::draw_, 0, true, true);

        //  //ImGui::EndChild();
        //  ImGui::EndTabItem();
        //}

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

  // todo: template with static constexpr flag !!!
  struct vlotbl_desc
    : trivially_serializable<vlotbl_desc>
  {
    uint32_t descs_offset = 0;
    uint32_t data_offset = 0;
  };

  enum pool_flag
  {
    vlotbl_resids   = 1, // present when 7, not when 6
		vlotbl_strings  = 2, // according to in-blob order
		vlotbl_objects  = 4, // according to in-blob order
  };

  struct header_t
    : trivially_serializable<header_t>
  {
    uint16_t uk1                    = 0; // probably version
    uint16_t vlotbl_flags           = 0;
    uint32_t ids_cnt                = 0;
    
    //uint32_t rarefpool_descs_offset = 0;
    //uint32_t rarefpool_data_offset  = 0;
    //uint32_t strpool_descs_offset   = 0;
    //uint32_t strpool_data_offset    = 0;
    //uint32_t objpool_descs_offset       = 0;
    // tricky: obj offsets are relative to the strpool base
    //uint32_t objpool_data_offset         = 0; 
  };

  struct obj_desc_t
    : trivially_serializable<obj_desc_t>
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

  CSystemSerCtx m_serctx; // strpool + respool + handles

  std::vector<CObjectSPtr> m_objects;
  std::vector<CObjectSPtr> m_handle_objects;

public:
  const std::vector<CObjectSPtr>& objects() const { return m_objects; }
  std::vector<CObjectSPtr>& objects()       { return m_objects; }

public:

  bool serialize_in(std::istream& reader, size_t blob_size)
  {
    const uint32_t start_pos = (uint32_t)reader.tellg();
    if (start_pos != 0)
      throw std::exception("start_pos != 0..");

    // get full blob
    reader.seekg(0, std::ios::end);
    const uint32_t end_pos = (uint32_t)reader.tellg();
    const uint32_t buf_size = end_pos;

    auto buffer = std::make_unique<char[]>(buf_size);
    reader.seekg(0);
    reader.read(buffer.get(), buf_size);

    redx::mem_ibstream ar(buffer.get(), buf_size);

    m_ids.clear();
    m_objects.clear();
    m_handle_objects.clear();

    ar >> m_header;

    vlotbl_desc resids_tbl_desc {};
    vlotbl_desc strings_tbl_desc {};
    vlotbl_desc objects_tbl_desc {};

    if (m_header.vlotbl_flags & vlotbl_resids)
      ar >> resids_tbl_desc;

    if (m_header.vlotbl_flags & vlotbl_strings)
      ar >> strings_tbl_desc;

    if (m_header.vlotbl_flags & vlotbl_objects)
      ar >> objects_tbl_desc;

    ar >> m_uk1;

    //if (uk1 != 0)
    //  throw std::exception("uk1 != 0, cool :)");

    uint16_t ids_cnt = 0;
    ar >> ids_cnt;

    if (ids_cnt != m_header.ids_cnt)
      throw std::exception("ids_cnt != m_header.ids_cnt, not cool :(");

    m_ids.resize(ids_cnt);
    ar.read_array(m_ids);

    // end of header

    const uint32_t data_offset = (uint32_t)ar.tellg();
    const uint32_t data_size = buf_size - data_offset;

    // res refs
    
    if (resids_tbl_desc.data_offset)
    {
      if (!m_serctx.respool.read_in(
        buffer.get() + data_offset,
        resids_tbl_desc.descs_offset,
        resids_tbl_desc.data_offset - resids_tbl_desc.descs_offset,
        data_size - resids_tbl_desc.data_offset, // larger than truth
        m_uk1 != 0))
      {
        return false;
      }
    }
    else
    {
      m_serctx.respool.clear();
    }

    if (strings_tbl_desc.data_offset)
    {
      if (!m_serctx.strpool.read_in(
        buffer.get() + data_offset,
        strings_tbl_desc.descs_offset,
        strings_tbl_desc.data_offset - strings_tbl_desc.descs_offset,
        data_size - strings_tbl_desc.data_offset)) // larger than truth
      {
        return false;
      }
    }
    else
    {
      m_serctx.respool.clear();
    }

    // now let's read objects

    // we don't have the impl for all props
    // so we'll use the assumption that everything is serialized in
    // order and use the offset of next item as end of blob

    const uint32_t obj_descs_size = objects_tbl_desc.data_offset - objects_tbl_desc.descs_offset;
    if (obj_descs_size % sizeof(obj_desc_t) != 0)
      return false;

    const size_t obj_descs_cnt = obj_descs_size / sizeof(obj_desc_t);
    if (obj_descs_cnt == 0)
      return objects_tbl_desc.data_offset + data_offset == blob_size; // could be empty

    //std::vector<obj_desc_t> obj_descs(obj_descs_cnt);
    std::span<obj_desc_t> obj_descs((obj_desc_t*)(buffer.get() + data_offset + objects_tbl_desc.descs_offset), obj_descs_cnt);

    // prepare default initialized objects
    m_serctx.m_objects.clear();
    m_serctx.m_objects.reserve(obj_descs.size());
    for (auto it = obj_descs.begin(); it != obj_descs.end(); ++it)
    {
      const auto& desc = *it;

      // check desc is valid
      if (desc.data_offset < objects_tbl_desc.data_offset)
        return false;

      auto obj_ctypename = m_serctx.strpool.at(desc.name_idx).gstr();
      auto new_obj = std::make_shared<CObject>(obj_ctypename, false); // todo: static create method
      m_serctx.m_objects.push_back(new_obj);
    }

    uint32_t next_obj_offset = data_size;
    auto obj_it = m_serctx.m_objects.rbegin();
    for (auto it = obj_descs.rbegin(); it != obj_descs.rend(); ++it, ++obj_it)
    {
      const auto& desc = *it;

      const uint32_t offset = desc.data_offset;
      if (offset > next_obj_offset)
        throw std::logic_error("CSystem: false assumption #2. please open an issue.");

      std::span<char> objblob(buffer.get() + data_offset + offset, next_obj_offset - offset);
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
    // ----------------------------------------------

    //CStringPool strpool;
    CSystemSerCtx& serctx = const_cast<CSystemSerCtx&>(m_serctx);

    serctx.strpool.clear();
    serctx.respool.clear();

    // here we can't really remove handle-objects because there might be hidden handles in unsupported types
    serctx.m_objects.assign(m_objects.begin(), m_objects.end());
    serctx.m_objects.insert(serctx.m_objects.end(), m_handle_objects.begin(), m_handle_objects.end());
    serctx.rebuild_handlemap();

    std::ostringstream objects_ss;
    std::vector<obj_desc_t> obj_descs;
    obj_descs.reserve(serctx.m_objects.size()); // ends up higher in the presence of handles

    // serctx.m_objects is extended during object serialization (handles)
    for (size_t i = 0; i < serctx.m_objects.size(); ++i)
    {
      auto& obj = serctx.m_objects[i];
      const uint32_t tmp_offset = (uint32_t)objects_ss.tellp();
      const uint16_t name_idx = serctx.strpool.insert(obj->ctypename());
      obj_descs.emplace_back(name_idx, tmp_offset);
      if (!obj->serialize_out(objects_ss, serctx))
        return false;
    }

    // ----------------------------------------------

    generic_obstream stw(*writer.rdbuf());
    uint32_t start_spos = (uint32_t)stw.tellp();
    uint32_t blob_size = 0; // we don't know it yet

    header_t new_header = m_header;
    stw << new_header; 

    vlotbl_desc resids_tbl_desc {};
    vlotbl_desc strings_tbl_desc {};
    vlotbl_desc objects_tbl_desc {};

    if (serctx.respool.size() != 0)
    {
      new_header.vlotbl_flags |= vlotbl_resids;
      stw << resids_tbl_desc;
    }
    
    if (serctx.strpool.size() != 0)
    {
      new_header.vlotbl_flags |= vlotbl_strings;
      stw << strings_tbl_desc;
    }

    if (serctx.m_objects.size() != 0)
    {
      new_header.vlotbl_flags |= vlotbl_objects;
      stw << objects_tbl_desc;
    }

    stw << m_uk1;

    uint16_t ids_cnt = (uint16_t)m_ids.size();
    stw << ids_cnt;
    stw.write_array(m_ids);
    new_header.ids_cnt = ids_cnt;

    // end of header

    const uint32_t data_spos = (uint32_t)stw.tellp();
    const uint32_t data_offset = data_spos - start_spos;

    // rarefs

    if (serctx.respool.size() != 0)
    {
      const uint32_t descs_offset = (uint32_t)stw.tellp() - data_spos;
      uint32_t descs_size = 0;
      uint32_t data_size = 0;
      serctx.respool.serialize_out(stw, data_spos, descs_size, data_size, m_uk1 != 0);
      resids_tbl_desc.descs_offset = descs_offset;
      resids_tbl_desc.data_offset = descs_offset + descs_size;
    }

    // strpool

    if (serctx.strpool.size() != 0)
    {
      const uint32_t descs_offset = (uint32_t)stw.tellp() - data_spos;
      uint32_t descs_size = 0;
      uint32_t data_size = 0;
      serctx.strpool.serialize_out(stw, data_spos, descs_size, data_size);
      strings_tbl_desc.descs_offset = descs_offset;
      strings_tbl_desc.data_offset = descs_offset + descs_size;
    }

    // objects

    objects_tbl_desc.descs_offset = (uint32_t)stw.tellp() - data_spos;
    const size_t obj_descs_size = obj_descs.size() * sizeof(obj_descs[0]);
    objects_tbl_desc.data_offset = (uint32_t)(objects_tbl_desc.descs_offset + obj_descs_size);

    // reoffset offsets
    for (auto& desc : obj_descs)
      desc.data_offset += objects_tbl_desc.data_offset;

    stw.write_array(obj_descs);

    auto objdata = objects_ss.str(); // todo: fix
    stw.write_bytes(objdata.data(), objdata.size());
    uint32_t end_spos = (uint32_t)stw.tellp();

    // at this point let's overwrite the header

    stw.seekp(start_spos);
    
    stw << new_header; 

    if (new_header.vlotbl_flags & vlotbl_resids)
    {
      stw << resids_tbl_desc;
    }
    
    if (new_header.vlotbl_flags & vlotbl_strings)
    {
      stw << strings_tbl_desc;
    }

    if (new_header.vlotbl_flags & vlotbl_objects)
    {
      stw << objects_tbl_desc;
    }

    // return to end of data
    stw.seekp(end_spos);

    return true;
  }
};


