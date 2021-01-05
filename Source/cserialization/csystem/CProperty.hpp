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
#include <cserialization/serializers.hpp>
#include <cserialization/csystem/CStringPool.hpp>
#include <cserialization/csystem/CPropertyBase.hpp>
#include <cserialization/csystem/CPropertyFactory.hpp>
#include <cserialization/csystem/CObject.hpp>
#include <widgets/cnames_widget.hpp>

//------------------------------------------------------------------------------
// BOOL
//------------------------------------------------------------------------------

class CBoolProperty
  : public CProperty
{
  static inline std::string s_ctypename = "Bool";

protected:
  bool m_value = false;
  

public:
  CBoolProperty()
    : CProperty(EPropertyKind::Bool)
  {
  }

  ~CBoolProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return s_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    char val = 0;
    is >> cbytes_ref(val);
    m_value = !!val;
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    char val = m_value ? 1 : 0;
    os.write(&val, 1);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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
  EIntKind m_int_kind;

public:
  CIntProperty(EIntKind int_kind)
    : CProperty(EPropertyKind::Integer), m_int_kind(int_kind) {}

  ~CIntProperty() override = default;

public:
  EIntKind int_kind() const { return m_int_kind; }

public:
  // overrides

  std::string_view ctypename() const override
  {
    switch (m_int_kind)
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

  size_t int_size() const
  {
    switch (m_int_kind)
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

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    const size_t value_size = int_size();
    is.read((char*)&m_value.u64, value_size);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    os.write((char*)&m_value.u64, int_size());
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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

    return ImGui::InputScalar(label, dtype, &m_value.u64, 0, 0, dfmt, editable ? 0 : ImGuiInputTextFlags_ReadOnly);
  }

#endif
};

//------------------------------------------------------------------------------
// FLOATING POINT
//------------------------------------------------------------------------------

class CFloatProperty
  : public CProperty
{
  static inline std::string s_ctypename = "Float";

protected:
  float m_value;

public:
  CFloatProperty()
    : CProperty(EPropertyKind::Float)
  {
  }

  ~CFloatProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return s_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    is >> cbytes_ref(m_value);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    char val = m_value ? 1 : 0;
    os.write(&val, 1);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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
{
public:
  using container_type = std::vector<CPropertySPtr>;
  using iterator = container_type::iterator;
  using const_iterator = container_type::const_iterator;

protected:
  container_type m_elts;

protected:
  // with a pointer to System in CProperty we could use
  // CName ids too.. but that's for later
  std::string m_elt_ctypename;
  std::string m_typename;

public:
  CArrayProperty(std::string_view elt_ctypename, size_t size)
    : CProperty(EPropertyKind::DynArray), m_elt_ctypename(elt_ctypename)
  {
    m_typename = fmt::format("[{}]{}", size, m_elt_ctypename);
    m_elts.resize(size);
  }

  ~CArrayProperty() override = default;

public:
  const container_type& elts() const { return m_elts; }

  const_iterator cbegin() const { return m_elts.cbegin(); }
  const_iterator cend() const { return m_elts.cend(); }

  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }

  iterator begin() { return m_elts.begin(); }
  iterator end() { return m_elts.end(); }

  // overrides

  std::string_view ctypename() const override { return m_typename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    auto& factory = CPropertyFactory::get();
    
    for (auto& elt : m_elts)
    {
      // todo: use clone on first instance..
      elt = factory.create(m_elt_ctypename);
      if (elt->kind() == EPropertyKind::Unknown)
        return false;
      if (!elt->serialize_in(is, strpool))
        return false;
    }

    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    for (auto& elt : m_elts)
    {
      if (!elt->serialize_out(os, strpool))
        return false;
    }

    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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
      for (auto& elt : m_elts)
      {
        auto lbl = fmt::format("{:03d}", idx);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text(lbl.c_str());
        ImGui::TableNextColumn();
        modified |= elt->imgui_widget(lbl.c_str(), editable);

        ++idx;
      }

      ImGui::EndTable();
    }

    return modified;
  }

#endif
};

//------------------------------------------------------------------------------
// DYN ARRAY
//------------------------------------------------------------------------------

class CDynArrayProperty
  : public CProperty
{
public:
  using container_type = std::vector<CPropertySPtr>;
  using iterator = container_type::iterator;
  using const_iterator = container_type::const_iterator;

protected:
  container_type m_elts;

protected:
  // with a pointer to System in CProperty we could use
  // CName ids too.. but that's for later
  std::string m_elt_ctypename;
  std::string m_typename;

public:
  CDynArrayProperty(std::string_view elt_ctypename)
    : CProperty(EPropertyKind::DynArray), m_elt_ctypename(elt_ctypename)
  {
    m_typename = "array:" + m_elt_ctypename;
  }

  ~CDynArrayProperty() override = default;

public:
  const container_type& elts() const { return m_elts; }

  const_iterator cbegin() const { return m_elts.cbegin(); }
  const_iterator cend() const { return m_elts.cend(); }

  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }

  iterator begin() { return m_elts.begin(); }
  iterator end() { return m_elts.end(); }

  // accepts no arguments for now
  iterator emplace(const_iterator _where)
  {
    auto new_elt = CPropertyFactory::get().create(m_elt_ctypename);
    return m_elts.emplace(_where);
  }

  // overrides

  std::string_view ctypename() const override { return m_typename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    m_elts.clear();

    uint32_t cnt = 0;
    is >> cbytes_ref(cnt);

    auto& factory = CPropertyFactory::get();
    m_elts.resize(cnt);

    for (auto& elt : m_elts)
    {
      elt = factory.create(m_elt_ctypename);
      if (elt->kind() == EPropertyKind::Unknown)
        return false;
      if (!elt->serialize_in(is, strpool))
        return false;
    }

    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    uint32_t cnt = 0;
    os << cbytes_ref(cnt);

    for (auto& elt : m_elts)
    {
      if (!elt->serialize_out(os, strpool))
        return false;
    }

    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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
      for (auto& elt : m_elts)
      {
        auto lbl = fmt::format("{:03d}", idx);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text(lbl.c_str());
        ImGui::TableNextColumn();
        modified |= elt->imgui_widget(lbl.c_str(), editable);

        ++idx;
      }

      ImGui::EndTable();
    }

    return modified;
  }

#endif
};


//------------------------------------------------------------------------------
// OBJECT
//------------------------------------------------------------------------------

class CObjectProperty
  : public CProperty
{
protected:
  CObjectSPtr m_object;

  std::string m_obj_ctypename;

public:
  CObjectProperty(std::string_view obj_ctypename)
    : CProperty(EPropertyKind::Object), m_obj_ctypename(obj_ctypename)
  {
  }

  ~CObjectProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return m_obj_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    m_object = std::make_shared<CObject>();
    m_object->serialize_in(is, strpool);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    return m_object->serialize_out(os, strpool);
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
  {
    return m_object->imgui_widget(label, editable);
  }

#endif
};

//------------------------------------------------------------------------------
// ENUM
//------------------------------------------------------------------------------

class CEnumProperty
  : public CProperty
{
protected:
  std::string m_enum_name;
  uint32_t m_value = 0;
  std::string m_val_name;
  CEnumList::enum_members_sptr m_p_enum_members;

public:
  CEnumProperty(std::string enum_name)
    : CProperty(EPropertyKind::Combo), m_enum_name(enum_name)
  {
    m_p_enum_members = CEnumList::get().get_enum(enum_name);
  }

  ~CEnumProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return m_enum_name; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    uint16_t strpool_idx = 0;
    is >> cbytes_ref(strpool_idx);
    if (strpool_idx >= strpool.size())
      return false;
    m_val_name = strpool.from_idx(strpool_idx);
    auto& enum_members = *m_p_enum_members;
    m_value = (uint32_t)enum_members.size();
    for (size_t i = 0; i < enum_members.size(); ++i)
    {
      if (enum_members[i] == m_val_name)
      {
        m_value = (uint32_t)i;
        break;
      }
    }
    if (m_value == enum_members.size())
    {
      enum_members.push_back(m_val_name);
    }

    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    uint16_t strpool_idx = strpool.to_idx(m_val_name);
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

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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
      ImGui::Text("%s: %s::%s", label, m_enum_name.c_str(), current_enum_name);
    
    if (current_item == m_value || current_item < 0)
      return false;
    m_value = (uint16_t)current_item;
    m_val_name = enum_members[m_value];
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
  static inline std::string s_ctypename = "TweakDBID";

protected:
  TweakDBID m_id;

public:
  CTweakDBIDProperty()
    : CProperty(EPropertyKind::TweakDBID)
  {
  }

  ~CTweakDBIDProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return s_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    is >> m_id;
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    os << m_id;
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
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
  static inline std::string s_ctypename = "CName";

protected:
  CName m_id;

public:
  CNameProperty()
    : CProperty(EPropertyKind::CName)
  {
  }

  ~CNameProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return s_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    uint16_t strpool_idx = 0;
    is >> cbytes_ref(strpool_idx);
    if (!is.good() || strpool_idx >= strpool.size())
      return false;

    m_id = CName(strpool.from_idx(strpool_idx));
    return true;
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    uint16_t strpool_idx = strpool.to_idx(m_id.str());
    os << cbytes_ref(strpool_idx);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
  {
    return CName_widget::draw(m_id, label);
  }

#endif
};

//------------------------------------------------------------------------------
// HANDLE
//------------------------------------------------------------------------------

class CHandleProperty
  : public CProperty
{
protected:
  uint32_t m_handle;
  std::string m_sub_ctypename;
  std::string m_ctypename;

public:
  CHandleProperty(std::string_view sub_ctypename)
    : CProperty(EPropertyKind::Handle), m_sub_ctypename(sub_ctypename)
  {
    m_ctypename = "handle:" + m_sub_ctypename;
  }

  ~CHandleProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return m_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    is >> cbytes_ref(m_handle);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    os << cbytes_ref(m_handle);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
  {
    ImGui::Text("handle: %08X", m_handle);
    return false;
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
  static inline std::string s_ctypename = "NodeRef";

protected:
  std::string m_str;

public:
  CNodeRefProperty()
    : CProperty(EPropertyKind::NodeRef)
  {
  }

  ~CNodeRefProperty() override = default;

public:
  // overrides

  std::string_view ctypename() const override { return s_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    uint16_t cnt = 0;
    is >> cbytes_ref(cnt);
    m_str.clear();
    m_str.resize(cnt, '\0');
    is.read(m_str.data(), cnt);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    uint16_t cnt = m_str.size();
    os.write(m_str.data(), cnt);
    os << cbytes_ref(cnt);
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
  {
    ImGui::Text("noderef: %s", m_str.c_str());
    return false;
  }

#endif
};

