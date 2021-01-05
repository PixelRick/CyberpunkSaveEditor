#pragma once
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <exception>
#include <stdexcept>

#include <utils.hpp>
#include <cpinternals/cpnames.hpp>
#include <cserialization/serializers.hpp>
#include <cserialization/csystem/CStringPool.hpp>
#include <cserialization/csystem/CPropertyBase.hpp>
#include <cserialization/csystem/CPropertyFactory.hpp>

class CObject;
using CObjectSPtr = std::shared_ptr<CObject>;

struct CPropertyField
{
  CPropertyField() = default;

  CPropertyField(std::string_view name, const CPropertySPtr& prop)
    : name(name), prop(prop) {}

  std::string name;
  CPropertySPtr prop;
};

class CObject
{
protected:
  std::vector<CPropertyField> m_fields;

public:
  CObject() {}

  virtual ~CObject() = default;

protected:
  struct property_desc_t
  {
    property_desc_t() = default;

    property_desc_t(uint16_t name_idx, uint16_t ctypename_idx, uint32_t data_offset)
      : name_idx(name_idx), ctypename_idx(ctypename_idx), data_offset(data_offset) {}

    uint16_t name_idx       = 0;
    uint16_t ctypename_idx  = 0;
    uint32_t data_offset    = 0;
  };

  static inline std::set<std::string> to_implement_ctypenames;

  CPropertySPtr serialize_property(std::string_view ctypename, std::istream& is, CStringPool& strpool, bool eof_is_end_of_prop = false)
  {
    if (!is.good())
      return nullptr;

    auto prop = CPropertyFactory::get().create(ctypename);
    const bool is_unknown_prop = prop->kind() == EPropertyKind::Unknown;

    if (!eof_is_end_of_prop && is_unknown_prop)
      return nullptr;

    size_t start_pos = (size_t)is.tellg();

    try
    {
      if (prop->serialize_in(is, strpool))
      {
        // if end of prop is known, return property only if it serialized completely
        if (!eof_is_end_of_prop || is.peek() == EOF)
          return prop;
      }
    }
    catch (std::exception&)
    {
      is.setstate(std::ios_base::badbit);
    }

    to_implement_ctypenames.emplace(prop->ctypename());

    // try fall-back
    if (!is_unknown_prop && eof_is_end_of_prop)
    {
      is.clear();
      is.seekg(start_pos);
      prop = std::make_shared<CUnknownProperty>(ctypename);
      if (prop->serialize_in(is, strpool))
        return prop;
    }

    return nullptr;
  }

public:
  bool serialize_in(const std::span<char>& blob, CStringPool& strpool)
  {
    span_istreambuf sbuf(blob);
    std::istream is(&sbuf);

    if (!serialize_in(is, strpool, true))
      return false;
    if (!is.good())
      return false;

    if (is.eof()) // tellg would fail if eofbit is set
      is.seekg(0, std::ios_base::end);
    const size_t read = (size_t)is.tellg();

    if (read != blob.size())
      return false;

    return true;
  }

  // eof_is_end_of_object allows for last property to be an unknown one (greedy read)
  // callers should check if object has been serialized completely ! (array props, system..)
  bool serialize_in(std::istream& is, CStringPool& strpool, bool eof_is_end_of_object=false)
  {
    m_fields.clear();

    uint32_t start_pos = (uint32_t)is.tellg();

    // m_fields cnt
    uint16_t fields_cnt = 0;
    is >> cbytes_ref(fields_cnt);

    m_fields.resize(fields_cnt);

    // field descriptors
    std::vector<property_desc_t> descs(fields_cnt);
    is.read((char*)descs.data(), fields_cnt * sizeof(property_desc_t));
    if (!is.good())
      return false;

    uint32_t data_pos = (uint32_t)is.tellg();

    // check descriptors
    for (auto& desc : descs)
    {
      if (desc.name_idx >= strpool.size())
        return false;
      if (desc.ctypename_idx >= strpool.size())
        return false;
      if (desc.data_offset < data_pos - start_pos)
        return false;
    }

    if (!fields_cnt)
      return true;

    // last field first
    const auto& last_desc = descs.back();
    is.seekg(start_pos + last_desc.data_offset);
    auto prop_ctypename = strpool.from_idx(last_desc.ctypename_idx);
    auto last_prop = serialize_property(prop_ctypename, is, strpool, eof_is_end_of_object);

    if (!last_prop)
      return false;

    m_fields.back() = {strpool.from_idx(last_desc.name_idx), last_prop};

    if (is.eof()) // tellg would fail if eofbit is set
      is.seekg(0, std::ios_base::end);
    uint32_t end_pos = (uint32_t)is.tellg();

    // rest of the m_fields (named props)
    uint32_t next_field_offset = last_desc.data_offset;
    for (size_t i = fields_cnt - 1; i--; /**/)
    {
      const auto& desc = descs[i];

      if (desc.data_offset > next_field_offset)
        throw std::logic_error("CObject: false assumption #1. please open an issue.");

      const size_t field_size = next_field_offset - desc.data_offset;

      isubstreambuf sbuf(is.rdbuf(), start_pos + desc.data_offset, field_size);
      std::istream isubs(&sbuf);
      auto prop_ctypename = strpool.from_idx(desc.ctypename_idx);
      auto prop = serialize_property(prop_ctypename, isubs, strpool, true);

      if (!prop)
        return false;

      m_fields[i] = {strpool.from_idx(desc.name_idx), prop};

      next_field_offset = desc.data_offset;
    }

    is.seekg(end_pos);
    return true;
  }

  bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    auto start_pos = os.tellp();

    // m_fields cnt
    uint16_t fields_cnt = (uint16_t)m_fields.size();;
    os << cbytes_ref(fields_cnt);

    // record current cursor position, since we'll rewrite descriptors
    auto descs_pos = os.tellp();

    // temp field descriptors
    property_desc_t empty_desc {};
    for (auto& field : m_fields)
      os << cbytes_ref(empty_desc);

    // m_fields (named props)
    std::vector<property_desc_t> descs;
    descs.reserve(fields_cnt);
    for (auto& field : m_fields)
    {
      const uint32_t data_offset = (uint32_t)(os.tellp() - start_pos);
      descs.emplace_back(
        strpool.to_idx(field.name),
        strpool.to_idx(field.prop->ctypename()),
        data_offset
      );
      field.prop->serialize_out(os, strpool);
    }

    auto end_pos = os.tellp();

    // real field descriptors
    os.seekp(descs_pos);
    os.write((char*)descs.data(), descs.size() * sizeof(property_desc_t));

    os.seekp(end_pos);
    return true;
  }

  // gui

#ifndef DISABLE_CP_IMGUI_WIDGETS

  static std::string_view field_name_getter(const CPropertyField& field)
  {
    return field.name;
  };

  [[nodiscard]] bool imgui_widget(const char* label, bool editable)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

    int torem_idx = -1;
    ImVec2 size = ImVec2(-FLT_MIN, std::min(600.f, ImGui::GetContentRegionAvail().y));
    if (ImGui::BeginTable(label, 2, tbl_flags))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("fields", ImGuiTableColumnFlags_WidthAutoResize);
      ImGui::TableSetupColumn("values", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      // can't clip but not a problem
      int i = 0;
      for (auto& field : m_fields)
      {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text(field.name.c_str());
        if (editable && ImGui::BeginPopupContextItem("item context menu"))
        {
          if (ImGui::Selectable("delete"))
            torem_idx = i;
          ImGui::EndPopup();
        }

        ImGui::TableNextColumn();

        //ImGui::PushItemWidth(300.f);
        modified |= field.prop->imgui_widget(field.name.c_str(), editable);
        //ImGui::PopItemWidth();

        ++i;
      }

      if (torem_idx != -1)
      {
        m_fields.erase(m_fields.begin() + torem_idx);
        modified = true;
      }

      ImGui::EndTable();
    }

    return modified;
  }

#endif
};

/*
ImGuiWindow* window = ImGui::GetCurrentWindow();
if (window->SkipItems)
return false;


*/

