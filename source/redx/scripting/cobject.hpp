#pragma once
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <exception>
#include <stdexcept>

#include "cpinternals/csav/serializers.hpp"
#include "cpinternals/common.hpp"
#include "cpinternals/ctypes.hpp"

#include "fwd.hpp"
#include "CStringPool.hpp"
#include "csystem_serctx.hpp"
#include "cclass.hpp"
#include "iproperty.hpp"
#include "cproperty_factory.hpp"

// todo: move the ui part out of this lib
#include "imgui/extras/ImGuizmo.h"

// probably temporary until we can match CClass def


enum class EObjectEvent
{
  data_modified,
};


struct CObjectListener
{
  virtual ~CObjectListener() = default;
  virtual void on_cobject_event(const CObject& obj, EObjectEvent evt) = 0;
};


class CObject
  : public std::enable_shared_from_this<const CObject>
  , public CPropertyOwner
{
  struct field_t
  {
    field_t() = default;

    field_t(gname name, CPropertyUPtr&& prop)
      : name(name), prop(std::move(prop)) {}

    gname name;
    CPropertyUPtr prop;

    void fallback_to_unknown_prop()
    {
      if (prop)
        prop = std::make_unique<CUnknownProperty>(prop->owner(), prop->ctypename());
    }
  };

protected:
  std::vector<field_t> m_fields;
  CObjectBPSPtr m_blueprint;

public:
  CObject(gname ctypename, bool delay_fields_init=false)
  {
    m_blueprint = CObjectBPList::get().get_or_make_bp(ctypename);
    if (!delay_fields_init)
      reset_fields_from_bp();
  }

  ~CObject() override
  {
    // stop listening to field events
    clear_fields();
  }

public:
  gname ctypename() const { return m_blueprint->ctypename(); }

  CProperty* get_prop(gname field_name) const
  {
    // fast enough
    for (auto& field : m_fields)
    {
      if (field.name == field_name)
        return field.prop.get();
    }
    return nullptr;
  }

  template <typename T>
  T* get_prop_cast(gname field_name) const
  {
    // fast enough
    for (auto& field : m_fields)
    {
      if (field.name == field_name)
        return dynamic_cast<T*>(field.prop.get());
    }
    return nullptr;
  }

protected:
  void clear_fields()
  {
    m_fields.clear();
  }

  void reset_fields_from_bp()
  {
    clear_fields();
    for (auto& field_desc : m_blueprint->field_bps())
    {
      m_fields.emplace_back(field_desc.name(), std::move(field_desc.create_prop(this)));
    }
  }

  struct serial_field_desc_t
  {
    serial_field_desc_t() = default;

    serial_field_desc_t(uint16_t name_idx, uint16_t ctypename_idx, uint32_t data_offset)
      : name_idx(name_idx), ctypename_idx(ctypename_idx), data_offset(data_offset) {}

    uint16_t name_idx       = 0;
    uint16_t ctypename_idx  = 0;
    uint32_t data_offset    = 0;
  };

  static inline std::set<std::string> to_implement_ctypenames;

  // todo move inside field struct
  [[nodiscard]] bool serialize_field(field_t& field, std::istream& is, CSystemSerCtx& serctx, bool eof_is_end_of_prop = false)
  {
    if (!is.good())
      return false;

    auto prop = field.prop.get();
    const bool is_unknown_prop = prop->kind() == EPropertyKind::Unknown;

    if (!eof_is_end_of_prop && is_unknown_prop)
      return false;

    size_t start_pos = (size_t)is.tellg();

    try
    {
      if (prop->serialize_in(is, serctx))
      {
        // if end of prop is known, return property only if it serialized completely
        if (!eof_is_end_of_prop || is.peek() == EOF)
          return true;
      }
    }
    catch (std::exception&)
    {
      is.setstate(std::ios_base::badbit);
    }

    to_implement_ctypenames.emplace(std::string(prop->ctypename().c_str()));

    // try fall-back
    if (!is_unknown_prop && eof_is_end_of_prop)
    {
      is.clear();
      is.seekg(start_pos);
      field.fallback_to_unknown_prop();
      if (field.prop->serialize_in(is, serctx))
        return true;
    }

    return false;
  }

public:
  [[nodiscard]] bool serialize_in(const std::span<char>& blob, CSystemSerCtx& serctx)
  {
    span_istreambuf sbuf(blob);
    std::istream is(&sbuf);

    if (!serialize_in(is, serctx, true))
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
  [[nodiscard]] bool serialize_in(std::istream& is, CSystemSerCtx& serctx, bool eof_is_end_of_object=false)
  {
    uint32_t start_pos = (uint32_t)is.tellg();

    // m_fields cnt
    uint16_t serial_fields_cnt = 0;
    is >> cbytes_ref(serial_fields_cnt);
    if (serial_fields_cnt < 1)
      return true;

    // field descriptors
    std::vector<serial_field_desc_t> serial_descs(serial_fields_cnt);
    is.read((char*)serial_descs.data(), serial_fields_cnt * sizeof(serial_field_desc_t));
    if (!is.good())
      return false;

    uint32_t data_pos = (uint32_t)is.tellg();

    auto& strpool = serctx.strpool;

    auto& bpdescs = m_blueprint->field_bps();

    struct data_desc_t
    {
      uint32_t data_offset;
      uint32_t data_size = 0;
    };

    std::vector<CFieldDesc> field_descs(serial_descs.size());
    std::vector<data_desc_t> data_descs(serial_descs.size());

    uint32_t prev_offset = 0;
    for (size_t i = 0, n = serial_descs.size(); i < n; ++i)
    {
      const auto& sdesc = serial_descs[i];
      auto& fdesc = field_descs[i];
      auto& ddesc = data_descs[i];

      // sanity checks
      if (sdesc.name_idx >= strpool.size())
        return false;
      if (sdesc.ctypename_idx >= strpool.size())
        return false;
      if (sdesc.data_offset < data_pos - start_pos)
        return false;
      if (sdesc.data_offset < prev_offset)
        return false;
      prev_offset = sdesc.data_offset;

      fdesc = {
        strpool.at(sdesc.name_idx).gstr(),
        strpool.at(sdesc.ctypename_idx).gstr()
      };
      ddesc.data_offset = sdesc.data_offset;

      if (i > 0)
      {
        auto& prev_ddesc = data_descs[i-1];
        prev_ddesc.data_size = ddesc.data_offset - prev_ddesc.data_offset;
      }
    }

    //if (!m_blueprint->register_partial_field_descs(field_descs))
    //  return false;

    reset_fields_from_bp();

    // ideally doing it in reverse order would catch failures earlier (unknown prop with unknown end)
    // but it would also reverse the order of appearance in the log

    //auto field_it = m_fields.begin();
    auto prev_field_it = m_fields.begin();
    for (size_t i = 0; i < serial_fields_cnt; ++i)
    {
      auto& fdesc = field_descs[i];
      auto& ddesc = data_descs[i];

      // search for field
      auto field_it(prev_field_it);
      while (field_it != m_fields.end() && field_it->name != fdesc.name)
        ++field_it;

      // (allow for unordered, but the reserialization tests will fail)
      if (field_it == m_fields.end())
      {
        field_it = m_fields.begin();
        while (field_it != m_fields.end() && field_it->name != fdesc.name)
          ++field_it;
        if (field_it != m_fields.end())
        {
          serctx.log(fmt::format(
            "serialized_in ({}) out of order {}::{} (ctype:{})",
            i, this->ctypename().c_str(), fdesc.name.c_str(), fdesc.ctypename.c_str()));
        }
      }
      else
      {
        prev_field_it = field_it;
      }

      if (field_it == m_fields.end())
      {
        bool a = false;
        // todo: replace with logging
        throw std::runtime_error(
          fmt::format(
            "CObject::serialize_in: serial field {}::{} is missing from bp fields",
            this->ctypename().c_str(), fdesc.name.c_str())
        );
        return false;
      }

      if (field_it->prop->ctypename() != fdesc.ctypename)
      {
        // todo: replace with logging
        throw std::runtime_error(
          fmt::format(
            "CObject::serialize_in: serial field {} has different type ({}) than bp's ({})",
            fdesc.name.c_str(), fdesc.ctypename.c_str(), field_it->prop->ctypename().c_str())
        );
        return false;
      }

      if (i + 1 < serial_fields_cnt)
      {
        isubstreambuf sbuf(is.rdbuf(), start_pos + ddesc.data_offset, ddesc.data_size);
        std::istream isubs(&sbuf);

        if (!serialize_field(*field_it, isubs, serctx, true))
          return false;
      }
      else 
      {
        is.seekg(start_pos + ddesc.data_offset);
        if (!serialize_field(*field_it, is, serctx, eof_is_end_of_object))
          return false;
        break;
      }

      serctx.log(fmt::format(
        "serialized_in ({}) {}::{} (ctype:{}) in {} bytes",
        i, this->ctypename().c_str(), fdesc.name.c_str(), fdesc.ctypename.c_str(), ddesc.data_size));
    }

    if (is.eof()) // tellg would fail if eofbit is set
      is.seekg(0, std::ios_base::end);
    uint32_t end_pos = (uint32_t)is.tellg();

    serctx.log(fmt::format("serialized_in CObject {} in {} bytes", this->ctypename().c_str(), (size_t)(end_pos - start_pos)));

    post_cobject_event(EObjectEvent::data_modified);
    return true;
  }

  [[nodiscard]] bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    auto& strpool = serctx.strpool;

    auto start_pos = os.tellp();

    // m_fields cnt
    uint16_t fields_cnt = 0;

    for (auto& field : m_fields)
    {
      if (!field.prop)
        throw std::runtime_error("null property field");

      // the magical thingy
      if (field.prop->is_skippable_in_serialization())
        continue;

      fields_cnt++;
    }

    os << cbytes_ref(fields_cnt);
    if (!fields_cnt)
      return true;

    // record current cursor position, since we'll rewrite descriptors
    auto descs_pos = os.tellp();

    // temp field descriptors
    serial_field_desc_t empty_desc {};
    for (size_t i = 0; i < fields_cnt; ++i)
      os << cbytes_ref(empty_desc);

    // m_fields (named props)
    std::vector<serial_field_desc_t> descs;
    descs.reserve(fields_cnt);
    for (auto& field : m_fields)
    {
      // the magical thingy again
      if (field.prop->is_skippable_in_serialization())
        continue;

      size_t prop_start_pos = (size_t)os.tellp();

      const uint32_t data_offset = (uint32_t)(prop_start_pos - start_pos);

      const uint32_t name_idx = strpool.insert(field.name);
      const uint32_t ctypename_idx = strpool.insert(field.prop->ctypename());

      descs.emplace_back(
        name_idx,
        ctypename_idx,
        data_offset
      );

      if (!field.prop->serialize_out(os, serctx))
      {
        serctx.log(fmt::format("couldn't serialize_out {}::{}", this->ctypename().c_str(), field.name.c_str()));
        return false;
      }

      size_t prop_end_pos = (size_t)os.tellp();

      serctx.log(fmt::format("serialized_out {}::{} in {} bytes", this->ctypename().c_str(), field.name.c_str(), prop_end_pos - prop_start_pos));
    }

    auto end_pos = os.tellp();

    // real field descriptors
    os.seekp(descs_pos);
    os.write((char*)descs.data(), descs.size() * sizeof(serial_field_desc_t));

    serctx.log(fmt::format("serialized_out CObject {} in {} bytes", this->ctypename().c_str(), (size_t)(end_pos - start_pos)));

    os.seekp(end_pos);
    return true;
  }

  // gui

#ifndef DISABLE_CP_IMGUI_WIDGETS


  static std::string_view field_name_getter(const field_t& field)
  {
    // should be ok for rendering, only one value is used at a time
    static std::string tmp;
    return field.name.strv();
  };

  static inline bool show_field_types = false;

  [[nodiscard]] bool imgui_widget_wpos(const char* label, bool editable);
  [[nodiscard]] bool imgui_widget_quat(const char* label, bool editable);

  [[nodiscard]] bool imgui_widget(const char* label, bool editable)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    if (ctypename() == "WorldPosition"_gn)
    {
      return imgui_widget_wpos(label, editable);
    }

    if (ctypename() == "Quaternion"_gn)
    {
      return imgui_widget_quat(label, editable);
    }

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg
      | ImGuiTableFlags_Resizable;

    int torem_idx = -1;
    ImVec2 size = ImVec2(-FLT_MIN, std::min(600.f, ImGui::GetContentRegionAvail().y));
    if (ImGui::BeginTable(label, show_field_types ? 3 : 2, tbl_flags))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      if (show_field_types)
        ImGui::TableSetupColumn("field type", ImGuiTableColumnFlags_WidthFixed, 100.f);
      ImGui::TableSetupColumn("field gname", ImGuiTableColumnFlags_WidthFixed, 100.f);
      ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      // can't clip but not a problem
      int i = 0;
      for (auto& field : m_fields)
      {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (show_field_types)
        {
          auto field_type = field.prop->ctypename();
          ImGui::Text(field_type.c_str());

          ImGui::TableNextColumn();
        }

        auto field_name = field.name;
        ImGui::Text(field_name.c_str());
        //if (editable && ImGui::BeginPopupContextItem("item context menu"))
        //{
        //  if (ImGui::Selectable("reset to default"))
        //    toreset_idx = i;
        //  ImGui::EndPopup();
        //}

        ImGui::TableNextColumn();

        auto prop = field.prop.get();
        if (!prop->imgui_is_one_liner())
        {
          if (ImGui::TreeNodeEx((void*)prop, 0, "view value"))
          {
            modified |= prop->imgui_widget(field_name.c_str(), editable);
            ImGui::TreePop();
          }
        }
        else
        {
          if (prop->ctypename() != "WorldPosition"_gn && prop->ctypename() != "Quaternion"_gn)
            ImGui::PushItemWidth(-FLT_MIN);
          else
            ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - 50);
          modified |= field.prop->imgui_widget(field_name.c_str(), editable);
        }

        ++i;
      }

      //if (torem_idx != -1)
      //{
      //  m_fields.erase(m_fields.begin() + torem_idx);
      //  modified = true;
      //}

      ImGui::EndTable();
    }

    if (ctypename() == "Quaternion"_gn)
    {
      //const float siz = 100.f;
      //
      //ImGuiContext& g = *ImGui::GetCurrentContext();
      //const ImGuiStyle& style = g.Style;
      //
      //const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(100.f, 100.f));
      //
      //ImGui::ItemSize(frame_bb, style.FramePadding.y);
      //ImGui::ItemAdd(frame_bb, 0, &frame_bb);
      //
      //ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
      //
      //ImGuizmo::SetOrthographic(true);
      //ImGuizmo::SetDrawlist();
      //ImGuizmo::SetRect(frame_bb.Min.x, frame_bb.Min.y, 100.f, 100.f);
      //
      //static float identityMatrix[16] = {
      //  1.0f, 0.0f, 0.0f, 0.0f,
      //  0.0f, 1.0f, 0.0f, 0.0f,
      //  0.0f, 0.0f, 1.0f, 0.0f,
      //  0.0f, 0.0f, 0.0f, 1.0f };
      //
      //const float vsf = 0.57735;
      //
      //static float cameraView[16] = {
      //   vsf, -vsf, -vsf, 0.0f, // rht
      //  -vsf, -vsf,  vsf, 0.0f, // up
      //  -vsf, -vsf, -vsf, 0.0f, // fwd
      //   vsf,  vsf,  vsf, 1.0f };
      //
      //static float cameraProjection[16] = {
      //  0.5f, 0.0f, 0.0f, 0.0f,
      //  0.0f, 0.5f, 0.0f, 0.0f,
      //  0.0f, 0.0f, 0.5f, 0.0f,
      //  0.0f, 0.0f, 0.5f, 1.0f };
      //
      //ImGui::InputScalarN("view", ImGuiDataType_Float, cameraView, 16);
      //ImGui::InputScalarN("proj", ImGuiDataType_Float, cameraProjection, 16);
      //
      //ImGuizmo::DrawGrid(cameraView, cameraProjection, identityMatrix, 100.f);
      //ImGuizmo::DrawCubes(cameraView, cameraProjection, identityMatrix, 1);
      //ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
      //
      //ImGuizmo::ViewManipulate(cameraView, camDistance, ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128), 0x10101010);
    
    }

    return modified;
  }

#endif

  // events

protected:
  std::set<CObjectListener*> m_listeners;

  void post_cobject_event(EObjectEvent evt) const
  {
    std::set<CObjectListener*> listeners = m_listeners;
    for (auto& l : listeners) {
      l->on_cobject_event(*this, evt);
    }
  }

  void on_cproperty_event(const CProperty& prop, EPropertyEvent evt) override
  {
    post_cobject_event(EObjectEvent::data_modified);
  }

public:
  // provided as const for ease of use

  void add_listener(CObjectListener* listener) const
  {
    auto& listeners = const_cast<CObject*>(this)->m_listeners;
    listeners.insert(listener);
  }

  void remove_listener(CObjectListener* listener) const
  {
    auto& listeners = const_cast<CObject*>(this)->m_listeners;
    listeners.erase(listener);
  }
};

