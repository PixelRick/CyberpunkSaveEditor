#include "CObject.hpp"
#include "CProperty.hpp"

bool CObject::imgui_widget_wpos(const char* label, bool editable)
{
  bool modified = false;

  auto fxx = (CIntProperty*)((CObjectProperty*)m_fields[0].prop.get())->obj()->get_prop("Bits"_gndef);
  auto fxy = (CIntProperty*)((CObjectProperty*)m_fields[1].prop.get())->obj()->get_prop("Bits"_gndef);
  auto fxz = (CIntProperty*)((CObjectProperty*)m_fields[2].prop.get())->obj()->get_prop("Bits"_gndef);

  constexpr float exponent = 1.f / (2 << 16);

  float ffs[3] = {
    (int32_t)fxx->u32() * exponent,
    (int32_t)fxy->u32() * exponent,
    (int32_t)fxz->u32() * exponent
  };

  ImGuiInputTextFlags flgs = editable ? 0 : ImGuiInputTextFlags_ReadOnly;

  modified |= ImGui::InputScalarN("x,y,z", ImGuiDataType_Float, &ffs, 3, 0, 0, "%.10g", flgs);

  if (modified)
  {
    fxx->u32((uint32_t)((ffs[0] / exponent) + 0.5f));
    fxy->u32((uint32_t)((ffs[1] / exponent) + 0.5f));
    fxz->u32((uint32_t)((ffs[2] / exponent) + 0.5f));
  }
    
  return modified;
}

bool CObject::imgui_widget_quat(const char* label, bool editable)
{
  bool modified = false;

  auto pffi = (CFloatProperty*)m_fields[0].prop.get();
  auto pffj = (CFloatProperty*)m_fields[1].prop.get();
  auto pffk = (CFloatProperty*)m_fields[2].prop.get();
  auto pffr = (CFloatProperty*)m_fields[3].prop.get();

  constexpr float exponent = 1.f / (2 << 16);

  float ffs[4] = {
    pffi->value(),
    pffj->value(),
    pffk->value(),
    pffr->value()
  };

  ImGuiInputTextFlags flgs = editable ? 0 : ImGuiInputTextFlags_ReadOnly;

  modified |= ImGui::InputScalarN("i,j,k,r", ImGuiDataType_Float, &ffs, 4, 0, 0, "%.10g", flgs);

  if (modified)
  {
    pffi->set_value(ffs[0]);
    pffj->set_value(ffs[1]);
    pffk->set_value(ffs[2]);
    pffr->set_value(ffs[3]);
  }

  return modified;
}

