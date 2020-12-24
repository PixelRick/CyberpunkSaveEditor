#pragma once
#include "node_editor.hpp"

#if __has_include(<span>) && (!defined(_HAS_CXX20) or _HAS_CXX20)
  #include <span>
#else
  #include "span.hpp"
  namespace std { using tcb::span; }
#endif

// #include "gap_buffer.hpp", find better impl or code it myself..
#include "imgui_extras/imgui_memory_editor.hpp"

class node_hexeditor
  : public node_editor
{
  static inline std::vector<char> m_clipboard;
  std::vector<char> editbuf;
  MemoryEditor me;

public:
  node_hexeditor(const std::shared_ptr<const node_t>& node)
    : node_editor(node)
  {
    me.ReadFn = read_fn;
    me.WriteFn = write_fn;
    me.HighlightFn = highlight_fn;
    //me.OptShowOptions = false;
    reload();
  }

public:
  static const std::vector<char>& clipboard() { return m_clipboard; }

  void select(size_t offset, size_t len)
  {
    me.DataSelectionStart = std::min(offset, editbuf.size() - 1);
    me.DataSelectionEnd = std::min(offset + len, editbuf.size() - 1);
    me.DataEditingAddr = -1; // cancel pending edit that shouldn't exist anyway
    me.ScrollToAddrNext = me.DataSelectionStart;
  }

  void commit() override
  {
    assign_node_data(editbuf.begin(), editbuf.end());
  }

  void reload() override 
  {
    const auto& buf = node_data_buffer();
    editbuf.assign(buf.begin(), buf.end());
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
    auto& buf = ((node_hexeditor*)data)->editbuf;
    if (off < buf.size())
      buf[off] = d;
  }

  static inline bool highlight_fn(const ImU8* data, size_t off)
  {
    const node_hexeditor* e = (node_hexeditor*)data;
    auto& buf = e->editbuf;
    auto& original = e->node_data_buffer();
    if (off < buf.size() && off < original.size())
      return buf[off] != original[off];
    return false;
  }

  void draw_impl() override
  {
    const ImGuiID id = ImGui::GetCurrentWindow()->GetID((void*)node().get()); // MemoryEditor uses this ID too
    ImVec2 c1 = ImGui::GetCursorScreenPos();
    ImGui::BeginChild(id, ImVec2(0, 400));

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
          if (ImGui::Selectable("paste")) {
            editbuf.erase(editbuf.begin() + seladdr_beg, editbuf.begin() + seladdr_end);
            editbuf.insert(editbuf.begin() + seladdr_beg, m_clipboard.begin(), m_clipboard.end());
          }
          if (ImGui::Selectable("paste insert"))
            editbuf.insert(editbuf.begin() + seladdr_beg, m_clipboard.begin(), m_clipboard.end());
          if (ImGui::Selectable("erase"))
            editbuf.erase(editbuf.begin() + seladdr_beg, editbuf.begin() + seladdr_end);
          ImGui::Separator();
        }

        static int cnt = 1;
        ImGui::DragInt("insert size ##insertsize", &cnt, 1, 1, 128, "%d");
        cnt &= 255;

        static const ImU8 u8_one = 1, u8_eight = 8;
        static char value = 0;
        ImGui::InputScalar("insert value ##insertvalue", ImGuiDataType_U8, (uint8_t*)&value, &u8_one, &u8_eight, "0x%02X");

        if (ImGui::Selectable("insert")) {
          std::vector<char> values((size_t)cnt, value);
          editbuf.insert(editbuf.begin() + seladdr_beg, values.begin(), values.end());
        }
      }

      ImGui::EndPopup();
    }
    
  }

  void on_dirty(size_t offset, size_t len, size_t patch_len) override
  {
    // nothing to do, hexeditor diffs against node_data_buffer() everyframe
  }
};

