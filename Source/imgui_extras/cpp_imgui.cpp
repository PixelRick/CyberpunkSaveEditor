#include "cpp_imgui.hpp"
#include "imgui.h"



static bool ListBoxVectorGetter(void* data, int idx, const char** out_pcstr)
{
  const auto& items = *(const std::vector<std::string>*)data;
  *out_pcstr = items[idx].c_str();
  return true;
}

bool ImGui::ListBox(std::string_view label, int* current_item, const std::vector<std::string>& items, int height_items)
{
  return ImGui::ListBox(label.data(), current_item, ListBoxVectorGetter, (void*)&items, (int)items.size(), height_items);
}




