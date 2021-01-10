#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <list>
#include <vector>
#include <array>
#include <exception>

#include <utils.hpp>
#include <cpinternals/cpnames.hpp>
#include <cpinternals/cpenums.hpp>
#include <csav/serializers.hpp>
#include <csav/csystem/fwd.hpp>
#include <csav/csystem/CStringPool.hpp>
#include <csav/csystem/CPropertyBase.hpp>
#include <csav/csystem/CPropertyFactory.hpp>
#include <csav/csystem/CObject.hpp>
#include <widgets/cnames_widget.hpp>

//------------------------------------------------------------------------------
// BOOL
//------------------------------------------------------------------------------

class CBoolProperty
  : public CProperty
{
protected:
  bool m_value = false;

public:
  CBoolProperty(CPropertyOwner* owner)
    : CProperty(owner, EPropertyKind::Bool)
  {
  }

  ~CBoolProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override
  {
    static CSysName sname("Bool");
    return sname;
  };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    char val = 0;
    is >> cbytes_ref(val);
    m_value = !!val;
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    char val = m_value ? 1 : 0;
    os.write(&val, 1);
    return true;
  }

  bool value() const { return m_value; }

  void value(bool value)
  {
    if (m_value != value)
    {
      m_value = value;
      post_cproperty_event(EPropertyEvent::data_modified);
    }
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    // no disabled checkbox in imgui atm
    if (!editable)
    {
      ImGui::Text(label);
      ImGui::SameLine();
      ImGui::Text(m_value ? ": true" : ": false");
      return false;
    }

    return ImGui::Checkbox(label, &m_value);
  }

#endif
};

//------------------------------------------------------------------------------
// INTEGER
//------------------------------------------------------------------------------

enum class EIntKind
{
  U8, I8,
  U16, I16,
  U32, I32,
  U64, I64,
};

class CIntProperty
  : public CProperty
{
protected:
  union 
  {
    uint8_t  u8;
    int8_t   i8;
    uint16_t u16;
    int16_t  i16;
    uint32_t u32;
    int32_t  i32;
    uint64_t u64;
    int64_t  i64 = 0;
  }
  m_value;

protected:
  const EIntKind m_int_kind;
  const size_t m_int_size;
  CSysName m_ctypename;

public:
  CIntProperty(CPropertyOwner* owner, EIntKind int_kind)
    : CProperty(owner, EPropertyKind::Integer)
    , m_int_kind(int_kind)
    , m_int_size(int_size(int_kind))
    , m_ctypename(int_ctypename(int_kind))
  {
  }

  ~CIntProperty() override = default;

public:
  EIntKind int_kind() const { return m_int_kind; }

public:
  
  static size_t int_size(EIntKind int_kind)
  {
    switch (int_kind)
    {
      case EIntKind::U8:
      case EIntKind::I8: return 1;
      case EIntKind::U16:
      case EIntKind::I16: return 2;
      case EIntKind::U32:
      case EIntKind::I32: return 4;
      case EIntKind::U64:
      case EIntKind::I64: return 8;
      default: break;
    }
    throw std::domain_error("unknown EIntKind");
  }

  static std::string int_ctypename(EIntKind int_kind)
  {
    switch (int_kind)
    {
      case EIntKind::U8: return "Uint8";
      case EIntKind::I8: return "Int8";
      case EIntKind::U16: return "Uint16";
      case EIntKind::I16: return "Int16";
      case EIntKind::U32: return "Uint32";
      case EIntKind::I32: return "Int32";
      case EIntKind::U64: return "Uint64";
      case EIntKind::I64: return "Int64";
      default: break;
    }
    throw std::domain_error("unknown EIntKind");
  };

  uint64_t u64() const { return m_value.u64; }
  uint32_t u32() const { return m_value.u32; }
  uint16_t u16() const { return m_value.u16; }
  uint8_t u8() const { return m_value.u8; }

  // todo: template this class..

  void u64(uint64_t value)
  {
    if (value != m_value.u64)
    {
      m_value.u64 = value;
      post_cproperty_event(EPropertyEvent::data_modified);
    }
  }

  void u32(uint32_t value)
  {
    if (value != m_value.u32)
    {
      m_value.u32 = value;
      post_cproperty_event(EPropertyEvent::data_modified);
    }
  }

  void u16(uint16_t value)
  {
    if (value != m_value.u16)
    {
      m_value.u16 = value;
      post_cproperty_event(EPropertyEvent::data_modified);
    }
  }

  void u8(uint8_t value)
  {
    if (value != m_value.u8)
    {
      m_value.u8 = value;
      post_cproperty_event(EPropertyEvent::data_modified);
    }
  }

  // overrides

  CSysName ctypename() const override
  {
    return m_ctypename;
  }

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    is.read((char*)&m_value.u64, m_int_size);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    os.write((char*)&m_value.u64, m_int_size);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    //auto label = ctypename() + "##CIntProperty";
    ImGuiDataType dtype = ImGuiDataType_U64;
    auto dfmt = "%016X";

    switch (m_int_kind)
    {
      case EIntKind::U8:  dtype = ImGuiDataType_U8;  dfmt = "%02X";  break;
      case EIntKind::I8:  dtype = ImGuiDataType_U8;  dfmt = "%02X";  break;
      case EIntKind::U16: dtype = ImGuiDataType_U16; dfmt = "%04X";  break;
      case EIntKind::I16: dtype = ImGuiDataType_U16; dfmt = "%04X";  break;
      case EIntKind::U32: dtype = ImGuiDataType_U32; dfmt = "%08X";  break;
      case EIntKind::I32: dtype = ImGuiDataType_U32; dfmt = "%08X";  break;
      case EIntKind::U64: dtype = ImGuiDataType_U64; dfmt = "%016X"; break;
      case EIntKind::I64: dtype = ImGuiDataType_U64; dfmt = "%016X"; break;
      default: break;
    }

    return ImGui::InputScalar(label, dtype, &m_value.u64, 0, 0, dfmt,
      ImGuiInputTextFlags_CharsHexadecimal | (editable ? 0 : ImGuiInputTextFlags_ReadOnly));
  }

#endif
};

//------------------------------------------------------------------------------
// FLOATING POINT
//------------------------------------------------------------------------------

class CFloatProperty
  : public CProperty
{
protected:
  float m_value = 0.f;

public:
  CFloatProperty(CPropertyOwner* owner)
    : CProperty(owner, EPropertyKind::Float) {}

  ~CFloatProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override
  {
    static CSysName sname("Float");
    return sname;
  };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    is >> cbytes_ref(m_value);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    os << cbytes_ref(m_value);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    return ImGui::InputFloat(label, &m_value, 0, 0, "%.3f", editable ? 0 : ImGuiInputTextFlags_ReadOnly);
  }

#endif
};


//------------------------------------------------------------------------------
// ARRAY (fixed len)
//------------------------------------------------------------------------------

class CArrayProperty
  : public CProperty
  , public CPropertyOwner
{
public:
  using container_type = std::vector<CPropertyUPtr>;
  using iterator = container_type::iterator;
  using const_iterator = container_type::const_iterator;

protected:
  container_type m_elts;

protected:
  // with a pointer to System in CProperty we could use
  // CName ids too.. but that's for later
  CSysName m_elt_ctypename;
  CSysName m_typename;
  std::function<CPropertyUPtr(CPropertyOwner*)> m_elt_create_fn;

public:
  CArrayProperty(CPropertyOwner* owner, CSysName elt_ctypename, size_t size)
    : CProperty(owner, EPropertyKind::DynArray)
    , m_elt_ctypename(elt_ctypename)
    , m_typename(fmt::format("[{}]{}", size, elt_ctypename.str()))
  {
    m_elts.resize(size);
    auto& factory = CPropertyFactory::get();
    m_elt_create_fn = factory.get_creator(elt_ctypename);
    for (auto& elt : m_elts)
    {
      // todo: use clone on first instance..
      elt = m_elt_create_fn(this);
    }
  }

public:
  const container_type& elts() const { return m_elts; }

  const_iterator cbegin() const { return m_elts.cbegin(); }
  const_iterator cend() const { return m_elts.cend(); }

  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }

  iterator begin() { return m_elts.begin(); }
  iterator end() { return m_elts.end(); }

  // overrides

  CSysName ctypename() const override { return m_typename; };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    size_t start_pos = is.tellg();

    uint32_t uk = 0;
    is >> cbytes_ref(uk);
    if (uk != m_elts.size())
      throw std::logic_error("CArrayProperty: false assumption #1. please open an issue");

    for (auto& elt : m_elts)
    {
      if (elt->kind() == EPropertyKind::Unknown)
        return false;
      if (!elt->serialize_in(is, serctx))
        return false;
    }

    size_t end_pos = is.tellg();
    serctx.log(fmt::format("serialized_in {} in {} bytes", this->ctypename().str(), (size_t)(end_pos - start_pos)));

    return is.good();
  }

  virtual [[nodiscard]] bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    size_t start_pos = os.tellp();

    uint32_t uk = (uint32_t)m_elts.size();
    os << cbytes_ref(uk);

    for (auto& elt : m_elts)
    {
      if (!elt->serialize_out(os, serctx))
        return false;
    }

    size_t end_pos = os.tellp();
    serctx.log(fmt::format("serialized_in {} in {} bytes", this->ctypename().str(), (size_t)(end_pos - start_pos)));

    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

    ImVec2 size = ImVec2(-FLT_MIN, std::min(400.f, ImGui::GetContentRegionAvail().y));
    if (ImGui::BeginTable(label, 2, tbl_flags, size))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("idx", ImGuiTableColumnFlags_WidthFixed, 30.f);
      ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      size_t idx = 0;
      int to_rem = -1;
      for (auto& elt : m_elts)
      {
        auto lbl = fmt::format("{:03d}", idx);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text(lbl.c_str());
        if (editable && ImGui::BeginPopupContextItem("item context menu"))
        {
          if (ImGui::Selectable("^ delete"))
            to_rem = (int)idx;
          ImGui::EndPopup();
        }

        ImGui::TableNextColumn();

        modified |= elt->imgui_widget(lbl.c_str(), editable);

        ++idx;
      }

      if (to_rem >= 0)
      {
        m_elts.erase(m_elts.begin() + to_rem);
        modified = true;
      }

      ImGui::EndTable();
    }

    return modified;
  }

  bool imgui_is_one_liner() override { return false; }

#endif

  // child events

  void on_cproperty_event(const CProperty& prop, EPropertyEvent evt) override
  {
    post_cproperty_event(EPropertyEvent::data_modified);
  }
};

//------------------------------------------------------------------------------
// DYN ARRAY
//------------------------------------------------------------------------------

class CDynArrayProperty
  : public CProperty
  , public CPropertyOwner
{
public:
  using container_type = std::vector<CPropertyUPtr>;
  using iterator = container_type::iterator;
  using const_iterator = container_type::const_iterator;

protected:
  container_type m_elts;

protected:
  // with a pointer to System in CProperty we could use
  // CName ids too.. but that's for later
  CSysName m_elt_ctypename;
  CSysName m_ctypename;
  std::function<CPropertyUPtr(CPropertyOwner*)> m_elt_create_fn;

public:
  CDynArrayProperty(CPropertyOwner* owner, CSysName elt_ctypename)
    : CProperty(owner, EPropertyKind::DynArray)
    , m_elt_ctypename(elt_ctypename)
    , m_ctypename(std::string("array:") + elt_ctypename.str())
  {
    auto& factory = CPropertyFactory::get();
    m_elt_create_fn = factory.get_creator(elt_ctypename);
  }

public:
  const container_type& elts() const { return m_elts; }

  const_iterator cbegin() const { return m_elts.cbegin(); }
  const_iterator cend() const { return m_elts.cend(); }

  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }

  iterator begin() { return m_elts.begin(); }
  iterator end() { return m_elts.end(); }

  iterator emplace(const_iterator _where)
  {
    auto it = m_elts.emplace(_where, m_elt_create_fn(this));
    post_cproperty_event(EPropertyEvent::data_modified);
    return it;
  }

  // overrides

  CSysName ctypename() const override { return m_ctypename; };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    m_elts.clear();

    uint32_t cnt = 0;
    is >> cbytes_ref(cnt);

    auto& factory = CPropertyFactory::get();
    m_elts.resize(cnt);

    for (auto& elt : m_elts)
    {
      elt = m_elt_create_fn(this);
      if (elt->kind() == EPropertyKind::Unknown)
        return false;
      if (!elt->serialize_in(is, serctx))
        return false;
    }

    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    uint32_t cnt = (uint32_t)m_elts.size();
    os << cbytes_ref(cnt);

    for (auto& elt : m_elts)
    {
      if (!elt->serialize_out(os, serctx))
        return false;
    }

    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

    ImVec2 size = ImVec2(-FLT_MIN, std::min(400.f, ImGui::GetContentRegionAvail().y));
    if (ImGui::BeginTable(label, 2, tbl_flags, size))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("idx", ImGuiTableColumnFlags_WidthFixed, editable ? 68.f : 28.f);
      ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      int to_rem = -1;
      for (size_t idx = 0; idx < m_elts.size(); ++idx)
      {
        scoped_imgui_id _sii((int)idx);

        auto& elt = m_elts[idx];

        auto lbl = fmt::format("{:03d}", idx);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::Text(lbl.c_str());
        if (editable)
        { 
          ImGui::SameLine();
          if (ImGui::ArrowButton("up", ImGuiDir_Up))
          {
            if (idx > 0)
            {
              size_t prev = idx - 1;
              std::swap(m_elts[idx], m_elts[prev]);
              modified = true;
            }
          }
          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
          ImGui::SameLine();
          if (ImGui::ArrowButton("down", ImGuiDir_Down))
          {
            if (idx + 1 < m_elts.size())
            {
              size_t next = idx + 1;
              std::swap(m_elts[idx], m_elts[next]);
              modified = true;
            }
          }
          ImGui::PopStyleVar();
          if (editable && ImGui::SmallButton("delete"))
            to_rem = (int)idx;
        }

        ImGui::TableNextColumn();
        modified |= elt->imgui_widget(lbl.c_str(), editable);
      }
      ImGui::EndTable();

      if (to_rem >= 0)
      {
        auto remit = m_elts.begin() + to_rem;
        m_elts.erase(remit);
        modified = true;
      }
    }

    return modified;
  }

  bool imgui_is_one_liner() override { return false; }

#endif

  // child events

  void on_cproperty_event(const CProperty& prop, EPropertyEvent evt) override
  {
    post_cproperty_event(EPropertyEvent::data_modified);
  }
};


//------------------------------------------------------------------------------
// OBJECT
//------------------------------------------------------------------------------

class CObjectProperty
  : public CProperty
  , public CObjectListener
{
protected:
  CObjectSPtr m_object;
  CSysName m_obj_ctypename;

public:
  CObjectProperty(CPropertyOwner* owner, CSysName obj_ctypename)
    : CProperty(owner, EPropertyKind::Object), m_obj_ctypename(obj_ctypename)
  {
    m_object = std::make_shared<CObject>(m_obj_ctypename);
    m_object->add_listener(this);
  }

  ~CObjectProperty() override
  {
    m_object->remove_listener(this);
  }

public:
  CObjectSPtr obj() const { return m_object; }

  // overrides

  CSysName ctypename() const override { return m_obj_ctypename; };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    return m_object->serialize_in(is, serctx) && is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    return m_object->serialize_out(os, serctx);
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    return m_object->imgui_widget(label, editable);
  }

  bool imgui_is_one_liner() override { return false; }

#endif

  // object events

  void on_cobject_event(const CObject& prop, EObjectEvent evt) override
  {
    post_cproperty_event(EPropertyEvent::data_modified);
  }
};

//------------------------------------------------------------------------------
// ENUM
//------------------------------------------------------------------------------

class CEnumProperty
  : public CProperty
{
protected:
  CSysName m_enum_name;
  uint32_t m_value = 0;
  CSysName m_val_name;
  CEnumList::enum_members_sptr m_p_enum_members;

public:
  CEnumProperty(CPropertyOwner* owner, CSysName enum_name)
    : CProperty(owner, EPropertyKind::Combo), m_enum_name(enum_name)
  {
    m_p_enum_members = CEnumList::get().get_enum(enum_name.str());
    if (m_p_enum_members->size())
      m_val_name = CSysName(m_p_enum_members->at(m_value));
    else
      m_val_name = CSysName("empty enum");
  }

  ~CEnumProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override { return m_enum_name; };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    uint16_t strpool_idx = 0;
    is >> cbytes_ref(strpool_idx);
    if (strpool_idx >= serctx.strpool.size())
      return false;
    m_val_name = CSysName(serctx.strpool.from_idx(strpool_idx));
    auto& enum_members = *m_p_enum_members;
    m_value = (uint32_t)enum_members.size();
    for (size_t i = 0; i < enum_members.size(); ++i)
    {
      if (enum_members[i] == m_val_name.str())
      {
        m_value = (uint32_t)i;
        break;
      }
    }
    if (m_value == enum_members.size())
    {
      enum_members.push_back(m_val_name.str());
    }

    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    uint16_t strpool_idx = serctx.strpool.to_idx(m_val_name.str());
    os << cbytes_ref(strpool_idx);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  static bool enum_name_getter(void* data, int idx, const char** out_str)
  {
    auto& enum_members = *(CEnumList::enum_members_t*)data;
    if (idx < 0 || idx >= enum_members.size())
      return false;
    *out_str = enum_members[idx].c_str();
    return true;
  }

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    auto& enum_members = *m_p_enum_members;
    int current_item = m_value;

    auto current_enum_name = fmt::format("unknown enum value {}", current_item);
    if (current_item >= 0 && current_item < enum_members.size())
      current_enum_name = enum_members[current_item];
    else
      ImGui::Text(fmt::format("unknown enum value %d", current_item).c_str());

    if (editable)
      ImGui::Combo(label, &current_item, &enum_name_getter, (void*)&enum_members, (int)enum_members.size());
    else
      ImGui::Text("%s: %s::%s", label, m_enum_name.str().c_str(), current_enum_name);
    
    if (current_item == m_value || current_item < 0)
      return false;

    m_value = (uint16_t)current_item;
    m_val_name = CSysName(enum_members[m_value]);
    return true;
  }

#endif
};

//------------------------------------------------------------------------------
// TweakDBID
//------------------------------------------------------------------------------

class CTweakDBIDProperty
  : public CProperty
{
protected:
  TweakDBID m_id = {};

public:
  CTweakDBIDProperty(CPropertyOwner* owner)
    : CProperty(owner, EPropertyKind::TweakDBID)
  {
  }

  ~CTweakDBIDProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override
  {
    static CSysName sname("TweakDBID");
    return sname;
  };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    is >> m_id;
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    os << m_id;
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    return TweakDBID_widget::draw(m_id, label);
  }

#endif
};

//------------------------------------------------------------------------------
// CName
//------------------------------------------------------------------------------

class CNameProperty
  : public CProperty
{
protected:
  CName m_id = {};

public:
  CNameProperty(CPropertyOwner* owner)
    : CProperty(owner, EPropertyKind::CName)
  {
  }

  ~CNameProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override
  {
    static CSysName sname("CName");
    return sname;
  };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    uint16_t strpool_idx = 0;
    is >> cbytes_ref(strpool_idx);
    if (!is.good() || strpool_idx >= serctx.strpool.size())
      return false;

    m_id = CName(serctx.strpool.from_idx(strpool_idx));
    return true;
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    uint16_t strpool_idx = serctx.strpool.to_idx(m_id.str());
    os << cbytes_ref(strpool_idx);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    return CName_widget::draw(m_id, label);
  }

#endif
};

//------------------------------------------------------------------------------
// CRUID
//------------------------------------------------------------------------------

class CCRUIDProperty
  : public CProperty
{
protected:
  uint64_t m_id = {};

public:
  CCRUIDProperty(CPropertyOwner* owner)
    : CProperty(owner, EPropertyKind::CRUID)
  {
  }

  ~CCRUIDProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override
  {
    static CSysName sname("CRUID");
    return sname;
  };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    is >> cbytes_ref(m_id);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    os << cbytes_ref(m_id);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    return ImGui::InputScalar(label, ImGuiDataType_U64, &m_id, 0, 0, "%016X",
      ImGuiInputTextFlags_CharsHexadecimal | (editable ? 0 : ImGuiInputTextFlags_ReadOnly));
  }

#endif
};

//------------------------------------------------------------------------------
// HANDLE (pointers..)
//------------------------------------------------------------------------------

// for now, replacing the object is permissive, but the original obj_ctypename
// must be a base class of the new object class.
// todo: implement inheritance in the BP database
//
// also, base type may not be constructible, so a default type might be necessary..
//

class CHandleProperty
  : public CProperty
{
protected:
  CSysName m_base_ctypename;
  CSysName m_ctypename;
  CObjectSPtr m_obj;

public:
  CHandleProperty(CPropertyOwner* owner, CSysName sub_ctypename)
    : CProperty(owner, EPropertyKind::Handle)
    , m_base_ctypename(sub_ctypename)
    , m_ctypename(std::string("handle:") + m_base_ctypename.str())
  {
    // dangerous, could infinite loop.. but also dangerous to have a nullptr here atm
    m_obj = std::make_shared<CObject>(m_base_ctypename);
  }

  ~CHandleProperty() override = default;

public:

  CObjectSPtr obj() const { return m_obj; }

  void set_obj(const CObjectSPtr& new_obj)
  {
    if (!new_obj) // no!
      return;
    m_obj = new_obj;
  }

  // overrides

  CSysName ctypename() const override { return m_ctypename; }

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    uint32_t handle;
    is >> cbytes_ref(handle);

    auto new_obj = serctx.from_handle(handle);
    set_obj(new_obj);

    return new_obj && is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    uint32_t handle = serctx.to_handle(m_obj);
    os << cbytes_ref(handle);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    if (!m_obj)
    {
      ImGui::Text("error: dead handle.");
      return false;
    }
    //auto basetype = m_base_ctypename.str();
    auto childtype = m_obj->ctypename().str();
    ImGui::Text("shared %s", childtype.c_str());
    return m_obj->imgui_widget(label, editable);
  }

#endif
};

//------------------------------------------------------------------------------
// NODEREF
//------------------------------------------------------------------------------

// this is 0x2d-0xa long

class CNodeRefProperty
  : public CProperty
{
protected:
  std::string m_str;

public:
  CNodeRefProperty(CPropertyOwner* owner)
    : CProperty(owner, EPropertyKind::NodeRef)
  {
  }

  ~CNodeRefProperty() override = default;

public:
  // overrides

  CSysName ctypename() const override
  {
    static CSysName sname("NodeRef");
    return sname;
  };

  bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override
  {
    uint16_t cnt = 0;
    is >> cbytes_ref(cnt);
    m_str.clear();
    m_str.resize(cnt, '\0');
    is.read(m_str.data(), cnt);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const
  {
    uint16_t cnt = (uint16_t)m_str.size();
    os << cbytes_ref(cnt);
    os.write(m_str.data(), cnt);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override
  {
    ImGui::Text("noderef: %s", m_str.c_str());
    return false;
  }

#endif
};

