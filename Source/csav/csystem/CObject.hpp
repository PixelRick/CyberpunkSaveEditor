#pragma once
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <exception>
#include <stdexcept>

#include <utils.hpp>
#include <cpinternals/cpnames.hpp>
#include <csav/serializers.hpp>

#include <csav/csystem/fwd.hpp>
#include <csav/csystem/CStringPool.hpp>
#include <csav/csystem/CSystemSerCtx.hpp>
#include <csav/csystem/CObjectBP.hpp>
#include <csav/csystem/CPropertyBase.hpp>
#include <csav/csystem/CPropertyFactory.hpp>


// probably temporary until we can match CClass def
struct CPropertyField
{
  CPropertyField() = default;

  CPropertyField(CSysName name, const CPropertySPtr& prop)
    : name(name), prop(prop) {}

  CSysName name;
  CPropertySPtr prop;
};


enum class EObjectEvent
{
  data_modified,
};

struct CObjectListener
{
  virtual ~CObjectListener() = default;
  virtual void on_cobject_event(const std::shared_ptr<const CObject>& obj, EObjectEvent evt) = 0;
};


class CObject
  : public std::enable_shared_from_this<const CObject>
  , public CPropertyListener
{
protected:
  std::vector<CPropertyField> m_fields;
  CObjectBPSPtr m_blueprint; // todo: replace by CClassSPtr

public:
  CObject(CSysName ctypename)
  {
    m_blueprint = CObjectBPList::get().get_or_make_bp(ctypename);
    reset_fields_from_bp();
  }

  ~CObject() override
  {
    // stop listening to field events
    clear_fields();
  }

public:
  CSysName ctypename() const { return m_blueprint->ctypename(); }

  CPropertySPtr get_prop(CSysName field_name) const
  {
    // fast enough
    for (auto& field : m_fields)
    {
      if (field.name == field_name)
        return field.prop;
    }
    return nullptr;
  }

protected:
  void clear_fields()
  {
    for (auto& field : m_fields)
      field.prop->remove_listener(this);
    m_fields.clear();
  }

  void reset_fields_from_bp()
  {
    clear_fields();
    //return;
    for (auto& field_desc : m_blueprint->field_descs())
    {
      // all fields are property fields
      auto new_prop = field_desc.create_prop();
      new_prop->add_listener(this);
      m_fields.emplace_back(field_desc.name(), new_prop);
    }
  }

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

  [[nodiscard]] bool serialize_field(CPropertyField& field, std::istream& is, CSystemSerCtx& serctx, bool eof_is_end_of_prop = false)
  {
    if (!is.good())
      return false;

    auto prop = field.prop;
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

    to_implement_ctypenames.emplace(std::string(prop->ctypename().str()));

    // try fall-back
    if (!is_unknown_prop && eof_is_end_of_prop)
    {
      is.clear();
      is.seekg(start_pos);
      prop = std::make_shared<CUnknownProperty>(prop->ctypename());
      field.prop->remove_listener(this);
      field.prop = prop;
      field.prop->add_listener(this);
      if (prop->serialize_in(is, serctx))
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
    uint16_t fields_cnt = 0;
    is >> cbytes_ref(fields_cnt);
    if (!fields_cnt)
      return true;

    // field descriptors
    std::vector<property_desc_t> descs(fields_cnt);
    is.read((char*)descs.data(), fields_cnt * sizeof(property_desc_t));
    if (!is.good())
      return false;

    uint32_t data_pos = (uint32_t)is.tellg();

    auto& strpool = serctx.strpool;

    auto& bpdescs = m_blueprint->field_descs();

    std::set<uint32_t> sr_names;
    struct better_desc_t
    {
      CSysName oname;
      CSysName tname;
      uint32_t data_offset;
      uint32_t data_size = 0;
    };

    std::vector<better_desc_t> better_descs(descs.size());

    std::vector<CPropertyField> new_fields;

    uint32_t prev_offset = 0;
    for (size_t i = 0, n = descs.size(); i < n; ++i)
    {
      const auto& desc = descs[i];

      // sanity checks
      if (desc.name_idx >= strpool.size())
        return false;
      if (desc.ctypename_idx >= strpool.size())
        return false;
      if (desc.data_offset < data_pos - start_pos)
        return false;
      if (desc.data_offset < prev_offset)
        return false;
      prev_offset = desc.data_offset;

      auto& bdesc = better_descs[i];
      bdesc.oname = CSysName(strpool.from_idx(desc.name_idx));
      bdesc.tname = CSysName(strpool.from_idx(desc.ctypename_idx));
      bdesc.data_offset = desc.data_offset;

      sr_names.insert(bdesc.oname.idx());

      if (i > 0)
      {
        auto& prev_bdesc = better_descs[i-1];
        prev_bdesc.data_size = desc.data_offset - prev_bdesc.data_offset;
      }

      // find or create field
      bool found = false;
      for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
      {
        if (it->name == bdesc.oname)
        {
          new_fields.emplace_back(*it);
          found = true;
          // remove it from m_fields, it will be prepended
          m_fields.erase(it);
          break;
        }
      }
      if (!found)
      {
        // check in blueprint
        for (auto& fdesc : m_blueprint->field_descs())
        {
          if (fdesc.name() == bdesc.oname)
          {
            auto prop = fdesc.create_prop();
            prop->add_listener(this);
            new_fields.emplace_back(bdesc.oname, prop);
            found = true;
            break;
          }
        }
      }
      // create
      if (!found)
      {
        auto& fdesc = m_blueprint->register_field(bdesc.oname, bdesc.tname);
        auto prop = fdesc.create_prop();
        prop->add_listener(this);
        new_fields.emplace_back(bdesc.oname, prop);
      }
    }
    // prepend the to-be-serialized-in fields
    m_fields.insert(m_fields.begin(), new_fields.begin(), new_fields.end());

    // last field first (most susceptible to fail)
    const auto& last_desc = better_descs.back();
    auto& last_field = m_fields[better_descs.size() - 1];

    is.seekg(start_pos + last_desc.data_offset);
    if (!serialize_field(last_field, is, serctx, eof_is_end_of_object))
      return false;

    if (is.eof()) // tellg would fail if eofbit is set
      is.seekg(0, std::ios_base::end);
    uint32_t end_pos = (uint32_t)is.tellg();

    serctx.log(fmt::format("serialized_in (last) {}::{} in {} bytes", this->ctypename().str(), last_desc.oname.str(), end_pos - (start_pos + last_desc.data_offset)));

    // rest of the m_fields (named props)
    for (size_t i = 0; i + 1 < fields_cnt; ++i)
    {
      const auto& bdesc = better_descs[i];
      auto& field = m_fields[i];

      if (field.name != bdesc.oname)
        throw std::logic_error("CObject::serialize_in: assertion #1");

      isubstreambuf sbuf(is.rdbuf(), start_pos + bdesc.data_offset, bdesc.data_size);
      std::istream isubs(&sbuf);

      if (!serialize_field(field, isubs, serctx, true))
        return false;

      serctx.log(fmt::format("serialized_in ({}) {}::{} in {} bytes", i, this->ctypename().str(), bdesc.oname.str(), bdesc.data_size));
    }

    serctx.log(fmt::format("serialized_in CObject {} in {} bytes", this->ctypename().str(), (size_t)(end_pos - start_pos)));

    is.seekg(end_pos);

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
    property_desc_t empty_desc {};
    for (size_t i = 0; i < fields_cnt; ++i)
      os << cbytes_ref(empty_desc);

    // m_fields (named props)
    std::vector<property_desc_t> descs;
    descs.reserve(fields_cnt);
    for (auto& field : m_fields)
    {
      // the magical thingy again
      if (field.prop->is_skippable_in_serialization())
        continue;

      size_t prop_start_pos = (size_t)os.tellp();

      const uint32_t data_offset = (uint32_t)(prop_start_pos - start_pos);
      descs.emplace_back(
        strpool.to_idx(field.name.str()),
        strpool.to_idx(field.prop->ctypename().str()),
        data_offset
      );

      if (!field.prop->serialize_out(os, serctx))
      {
        serctx.log(fmt::format("couldn't serialize_out {}::{}", this->ctypename().str(), field.name.str()));
        return false;
      }

      size_t prop_end_pos = (size_t)os.tellp();

      serctx.log(fmt::format("serialized_out {}::{} in {} bytes", this->ctypename().str(), field.name.str(), prop_end_pos - prop_start_pos));
    }

    auto end_pos = os.tellp();

    // real field descriptors
    os.seekp(descs_pos);
    os.write((char*)descs.data(), descs.size() * sizeof(property_desc_t));

    serctx.log(fmt::format("serialized_out CObject {} in {} bytes", this->ctypename().str(), (size_t)(end_pos - start_pos)));

    os.seekp(end_pos);
    return true;
  }

  // gui

#ifndef DISABLE_CP_IMGUI_WIDGETS


  static std::string_view field_name_getter(const CPropertyField& field)
  {
    // should be ok for rendering, only one value is used at a time
    static std::string tmp;
    tmp = field.name.str();
    return tmp;
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
        auto field_name = field.name.str();
        ImGui::Text(field_name.c_str());
        //if (editable && ImGui::BeginPopupContextItem("item context menu"))
        //{
        //  if (ImGui::Selectable("reset to default"))
        //    toreset_idx = i;
        //  ImGui::EndPopup();
        //}

        ImGui::TableNextColumn();

        //ImGui::PushItemWidth(300.f);
        modified |= field.prop->imgui_widget(field_name.c_str(), editable);
        //ImGui::PopItemWidth();

        ++i;
      }

      //if (torem_idx != -1)
      //{
      //  m_fields.erase(m_fields.begin() + torem_idx);
      //  modified = true;
      //}

      ImGui::EndTable();
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
      l->on_cobject_event(shared_from_this(), evt);
    }
  }

  void on_cproperty_event(const std::shared_ptr<const CProperty>& prop, EPropertyEvent evt) override
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

