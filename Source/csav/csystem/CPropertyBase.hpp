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
#include <csav/serializers.hpp>
#include <csav/csystem/CStringPool.hpp>

#ifndef DISABLE_CP_IMGUI_WIDGETS
#include <widgets/list_widget.hpp>
#include <imgui_extras/cpp_imgui.hpp>
#include <imgui_extras/imgui_stdlib.h>
#endif

enum class EPropertyKind
{
  None,
  Unknown,
  Bool,
  Integer,
  Float,
  Double,
  Combo,
  Array,
  DynArray,
  Handle,
  Object,
  TweakDBID,
  CName,
  NodeRef
};


static inline std::string_view
property_kind_to_display_name(EPropertyKind prop_kind)
{
  switch (prop_kind)
  {
    case EPropertyKind::None: return "None";
    case EPropertyKind::Unknown: return "Unknown";
    case EPropertyKind::Bool: return "Bool";
    case EPropertyKind::Integer: return "Integer";
    case EPropertyKind::Float: return "Float";
    case EPropertyKind::Double: return "Double";
    case EPropertyKind::Combo: return "Combo";
    case EPropertyKind::Array: return "Array";
    case EPropertyKind::DynArray: return "DynArray";
    case EPropertyKind::Handle: return "Handle";
    case EPropertyKind::Object: return "Object";
  }
  return "Invalid";
}


class CProperty
{
  EPropertyKind m_property_kind;

protected:
  CProperty(EPropertyKind kind)
    : m_property_kind(kind) {}

  virtual ~CProperty() = default;

public:
  EPropertyKind kind() const { return m_property_kind; }

public:
  virtual std::string_view ctypename() const = 0;

  // serialization

  virtual bool serialize_in(std::istream& is, CStringPool& strpool) = 0;

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const = 0;

  // gui (define DISABLE_CP_IMGUI_WIDGETS to disable implementations)

  [[nodiscard]] virtual bool imgui_widget(const char* label, bool editable)
  {
#ifndef DISABLE_CP_IMGUI_WIDGETS
    ImGui::Text("widget not implemented");
    return false;
#endif
  }

  void imgui_widget(const char* label) const
  {
    std::ignore = const_cast<CProperty*>(this)->imgui_widget(label, false);
  }
};

using CPropertySPtr = std::shared_ptr<CProperty>;


//------------------------------------------------------------------------------
// DEFAULT
//------------------------------------------------------------------------------

class CUnknownProperty
  : public CProperty
{
protected:
  std::string m_ctypename;
  std::vector<char> m_data;
  std::string m_child_ctypename;

public:
  CUnknownProperty(std::string_view ctypename)
    : CProperty(EPropertyKind::Unknown), m_ctypename(ctypename)
  {
  }

  ~CUnknownProperty() override = default;

public:
  const std::vector<char>& raw_data() const { return m_data; };

  // overrides

  std::string_view ctypename() const override { return m_ctypename; };

  bool serialize_in(std::istream& is, CStringPool& strpool) override
  {
    std::streampos beg = is.tellg();
    is.seekg(0, std::ios_base::end);
    const size_t size = (size_t)(is.tellg() - beg);
    is.seekg(beg);
    m_data.resize(size);
    is.read(m_data.data(), size);
    //std::istream_iterator<char> it(is), end;
    //m_data.assign(it, end);
    return is.good();
  }

  virtual bool serialize_out(std::ostream& os, CStringPool& strpool) const
  {
    os.write(m_data.data(), m_data.size());
    return true;
  }

#ifndef DISABLE_CP_IMGUI_WIDGETS

  [[nodiscard]] bool imgui_widget(const char* label, bool editable) override
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    ImGui::BeginChild(label, ImVec2(0,0), true, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("refactoring");
    ImGui::Text("ctypename: %s", ctypename().data());
    ImGui::Text("data size: %08X", m_data.size());
    if (m_data.size() > 50)
      ImGui::Text("data: %s...", bytes_to_hex(m_data.data(), 50).c_str());
    else
      ImGui::Text("data: %s", bytes_to_hex(m_data.data(), m_data.size()).c_str());

    ImGui::EndChild();
    return modified;
  }

#endif
};


