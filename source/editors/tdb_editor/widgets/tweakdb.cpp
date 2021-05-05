#include "tweakdb.hpp"
#include <string>
#include <imgui/imgui.h>

namespace ui {

bool im_tweakdb(tweakdb& tdb, int* selected)
{
  bool modified = false;

  ImGui::BeginGroup();
  {
    ImGui::Text("pool descs:");
    ImGui::ListBox("pool descs", selected, [](const pool_desc_t& pd) -> std::string {
      return fmt::format("{}[{}]", pd.ctypename.name().strv(), pd.len);
    }, tdb.pools_descs());
  }
  ImGui::EndGroup();

  return modified;
}

} // namespace ui

