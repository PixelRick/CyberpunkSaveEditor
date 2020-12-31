#pragma once

#include "node_editor.hpp"
#include "utils.hpp"
#include "imgui_extras/imgui_memory_editor.hpp"

class node_hexeditor
  : public node_editor_widget
{
  static inline std::vector<char> m_clipboard;
  std::vector<char> editbuf;
  MemoryEditor me;

public:
  node_hexeditor(const std::shared_ptr<const node_t>& node)
    : node_editor_widget(node)
  {
    me.ReadFn = read_fn;
    me.WriteFn = write_fn;
    me.HighlightFn = highlight_fn;
    //me.OptShowOptions = false;
    reload();
  }

  ~node_hexeditor() override {}

public:
  static const std::vector<char>& clipboard() { return m_clipboard; }

  void select(size_t offset, size_t len)
  {
    me.DataSelectionStart = std::min(offset, editbuf.size() - 1);
    me.DataSelectionEnd = std::min(offset + len - 1, editbuf.size() - 1);
    me.DataEditingAddr = -1; // cancel pending edit that shouldn't exist anyway
    me.ScrollToAddrNext = me.DataSelectionStart;
  }

protected:
  static inline ImU8 read_fn(const ImU8* data, size_t off)
  {
    auto& buf = ((node_hexeditor*)data)->editbuf;
    if (off < buf.size())
      return buf.at(off);
    return 0;
  }

  static inline void write_fn(ImU8* data, size_t off, ImU8 d)
  {
    node_hexeditor* e = (node_hexeditor*)data;
    auto& buf = e->editbuf;
    if (off < buf.size())
      buf[off] = d;
    e->m_has_unsaved_changes = true;
  }

  static inline bool highlight_fn(const ImU8* data, size_t off)
  {
    const node_hexeditor* e = (node_hexeditor*)data;
    auto& buf = e->editbuf;
    auto& original = e->node()->data();
    if (off < buf.size() && off < original.size())
      return buf[off] != original[off];
    return false;
  }

  void draw_impl(const ImVec2& size) override
  {
    const ImGuiID id = ImGui::GetCurrentWindow()->GetID((void*)node().get()); // MemoryEditor uses this ID too
    ImVec2 c1 = ImGui::GetCursorScreenPos();

    MemoryEditor::Sizes s;
    me.CalcSizes(s, editbuf.size(), 0);
    ImVec2 child_size = size;
    if (child_size.x <= 0)
      child_size.x = s.WindowWidth;
    if (child_size.y <= 0)
      child_size.y = 400;

    //ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(s.WindowWidth, FLT_MAX));
    ImGui::BeginChild(id, child_size, 1, ImGuiWindowFlags_AlwaysAutoResize);

    me.DrawContents((void*)this, editbuf.size(), 0);

    //if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
    //  ImGui::Text("Window Hovered");

    ImGui::EndChild();
    ImVec2 c2 = ImGui::GetCursorScreenPos();
    c2.x += ImGui::GetContentRegionAvailWidth();

    if (ImGui::IsMouseHoveringRect(c1, c2) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
      ImGui::OpenPopup("context##hexedit");

    if (ImGui::BeginPopup("context##hexedit"))
    {
      if (me.DataSelectionStart != (size_t)-1 || editbuf.empty())
      {
        size_t seladdr_beg = me.SelectionStart();
        size_t seladdr_end = seladdr_beg + me.SelectionLen();

        if (editbuf.empty()) {
          seladdr_beg = 0;
          seladdr_end = 0;
        }

        if (!editbuf.empty())
        {
          if (ImGui::Selectable("copy"))
            m_clipboard.assign(editbuf.begin() + seladdr_beg, editbuf.begin() + seladdr_end);

          if (ImGui::Selectable("paste"))
          {
            editbuf.erase(editbuf.begin() + seladdr_beg, editbuf.begin() + seladdr_end);
            editbuf.insert(editbuf.begin() + seladdr_beg, m_clipboard.begin(), m_clipboard.end());
            m_has_unsaved_changes = true;
          }
          if (ImGui::Selectable("paste insert"))
          {
            editbuf.insert(editbuf.begin() + seladdr_beg, m_clipboard.begin(), m_clipboard.end());
            m_has_unsaved_changes = true;
          }
          if (ImGui::Selectable("erase"))
          {
            editbuf.erase(editbuf.begin() + seladdr_beg, editbuf.begin() + seladdr_end);
            m_has_unsaved_changes = true;
          }
          ImGui::Separator();
        }

        static int cnt = 1;
        ImGui::DragInt("insert size ##insertsize", &cnt, 1, 1, 128, "%d");
        cnt &= 255;

        static const ImU8 u8_one = 1, u8_eight = 8;
        static char value = 0;
        ImGui::InputScalar("insert value ##insertvalue", ImGuiDataType_U8, (uint8_t*)&value, &u8_one, &u8_eight, "0x%02X");

        if (ImGui::Selectable("insert"))
        {
          std::vector<char> values((size_t)cnt, value);
          editbuf.insert(editbuf.begin() + seladdr_beg, values.begin(), values.end());
          m_has_unsaved_changes = true;
        }
      }

      ImGui::EndPopup();
    }
  }

  bool commit_impl() override
  {
    ncnode()->assign_data(editbuf.begin(), editbuf.end());
    return true;
  }

  bool reload_impl() override 
  {
    const auto& buf = node()->data();
    editbuf.assign(buf.begin(), buf.end());
    return true;
  }
};

