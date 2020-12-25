#pragma once

#include <stdint.h>
#include <thread>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
#include <vector>


#include "AppLib/IApp.h"

#include "utils.hpp"
#include "cserialization/csav.hpp"
#include "node_editors.hpp"

void ImGui::ShowDemoWindow(bool* p_open);

struct scoped_imgui_id {
  scoped_imgui_id(void* p) { ImGui::PushID(p); }
  ~scoped_imgui_id() { ImGui::PopID(); }
};

using loading_bar_job_fntype = bool (*)(float& progress);

class loading_bar_job_widget
{
  std::thread thread;
  bool finished = false;
  float progress = 0.f;

public:
  bool failed = false;

  bool is_running() const { return thread.joinable(); }

  template <class Fn, std::enable_if_t<std::is_same_v<std::invoke_result_t<Fn, float&>, bool>, int> = 0> 
  bool start(Fn&& fn)
  {
    if (is_running())
      return false;
    failed = false;
    progress = 0.f;
    thread = std::thread([this, fn]() {
      failed = !fn(progress);
      finished = true;
    });
    return true;
  }

  void update()
  {
    if (finished) {
      finished = false;
      thread.join();
    }
  }

  void draw()
  {
    if (is_running()) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(1) << (progress * 100) << "%";
      ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), ss.str().c_str());
    }
  }
};

class csav_collapsable_header
{
protected:
  ImGui::FileBrowser save_dialog;
  loading_bar_job_widget save_job;

  std::shared_ptr<csav> m_csav;
  std::shared_ptr<AppImage> m_img;

  bool m_alive = true;
  bool m_opened = true;
  //std::vector<ScanEntryWidget> scan_entries;
  //using scan_entry_it = decltype(scan_entries)::iterator;
  std::array<char, 24 * 3 + 1> search_needle = {};
  std::array<char, 24     + 1> search_mask = {};

  static inline bool s_inited = false;
  static inline void register_editors()
  {
    node_editor::factory_register_for_node_name<node_hexeditor>(NODE_EDITOR__DEFAULT_EDITOR_NAME);
    node_editor::factory_register_for_node_name<inventory_editor>("inventory");
  }

public:
  csav_collapsable_header(const std::shared_ptr<csav>& csav, const std::shared_ptr<AppImage>& img)
    : save_dialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir)
    , m_csav(csav), m_img(img)
  {
    if (!s_inited) {
      s_inited = true;
      register_editors();
    }

    save_dialog.SetTitle("Saving savefile");
    save_dialog.SetTypeFilters({ ".dat" });
  }

public:
  bool is_alive() const { return m_alive; }

  void update()
  {
    save_job.update();
  }

  static inline std::shared_ptr<const node_t> appearance_src;

  void draw()
  {
    scoped_imgui_id sii {this};

    std::string label = m_csav->filepath.u8string();
    
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, style.ItemSpacing.y));

    if (ImGui::ArrowButtonEx("expanded", m_opened ? ImGuiDir_Down : ImGuiDir_Right, ImVec2(20, 60)))
      m_opened = !m_opened;

    ImGui::SameLine();
    float post_arrow_x = ImGui::GetCursorPosX();

    if (m_img)
      m_img->draw(ImVec2(107, 60));
    else
    {
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK);
      ImGui::ButtonEx("nopic##screenshot", ImVec2(107, 60), ImGuiButtonFlags_Disabled);
      ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::ButtonEx("SAVE##SAVE", ImVec2(60, 60)))
    {
      save_job.start([this](float& progress) -> bool {
        return m_csav->save_with_progress(m_csav->filepath, progress);
      });
      ImGui::OpenPopup("Saving..##SAVE");
    }

    ImGui::SameLine();
    if (ImGui::ButtonEx("COPY SKIN##SAVE", ImVec2(100, 60)))
    {
      appearance_src = m_csav->search_node("CharacetrCustomization_Appearances");
    }

    ImGui::SameLine();
    if (ImGui::ButtonEx("PASTE SKIN##SAVE", ImVec2(100, 60)))
    {
      auto appearance_node = m_csav->search_node("CharacetrCustomization_Appearances");
      if (appearance_src && appearance_src != appearance_node)
      {
        auto& src_buf = appearance_src->data();
        appearance_node->nonconst().data().assign(src_buf.begin(), src_buf.end());
      }
    }

    // Always center this window when appearing
    ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool pushed_stylecol = false;
    if (save_job.failed) {
      ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, (ImVec4)ImColor::HSV(0.f, 1.f, 0.6f, 0.35f));
      pushed_stylecol = true;
    }

    if (ImGui::BeginPopupModal("Saving..##SAVE", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("path: %s", m_csav->filepath.string().c_str());
      
      if (save_job.is_running())
      {
        save_job.draw();
      }
      else 
      {
        if (save_job.failed)
          ImGui::Text("error, couldn't save savefile");
        else
          ImGui::Text("success!");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
          ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (pushed_stylecol)
      ImGui::PopStyleColor();

    // todo: save as.. (dialog)

    ImGui::SameLine();
    ImGui::ButtonEx(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30, 60), ImGuiButtonFlags_Disabled);

    ImGui::SameLine();
    m_alive &= !ImGui::ButtonEx("X##CLOSE", ImVec2(30, 60));

    ImGui::PopStyleVar();

    if (!m_opened)
      return;

    const float indent = 12.f;

    ImGui::Indent(indent);

    if (ImGui::CollapsingHeader("Original Node Descriptors", ImGuiTreeNodeFlags_None))
    {
      ImGui::Indent(5.f);

      static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

      static float u32row_width = ImGui::CalcTextSize("0xFFFFFFFF ").x;

      ImVec2 size = ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 30);
      if (ImGui::BeginTable("##nodedescs_table", 6, flags, size))
      {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("idx", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("next idx", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("child idx", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("offset", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("size", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)m_csav->node_descs.size());
        while (clipper.Step())
        {
          for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
          {
            auto& desc = m_csav->node_descs[row];
            scoped_imgui_id sii{&desc};
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("0x%X", row);
            ImGui::TableNextColumn(); ImGui::Text("%s", desc.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.next_idx);
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.child_idx);
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.data_offset);
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.data_size);
          }
        }
        ImGui::EndTable();
      }

      ImGui::Unindent(5.f);
    }

    if (ImGui::CollapsingHeader("Node Tree", ImGuiTreeNodeFlags_None))
    {
      ImGui::Indent(5.f);

      if (m_csav->root_node)
        for (const auto& n : m_csav->root_node->children())
          draw_node(n);

      ImGui::Unindent(5.f);
    }

    if (ImGui::CollapsingHeader("Search Tools", ImGuiTreeNodeFlags_None))
    {
      ImGui::Indent(5.f);
      ImGui::BeginChild("Search Tools##child", ImVec2(0, 0));
      
      int line_width = (int)ImGui::GetContentRegionAvail().x;
      float slider_width = (float)std::max(line_width - 300, 100);

      static uint32_t u32_v;
      const bool u32_search = ImGui::Button("search u32", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input u32", ImGuiDataType_U32, &u32_v);
      if (u32_search) {
        search_pattern_in_nodes(std::string((char*)&u32_v, (char*)&u32_v + 8), "");
      }

      static float f32_v;
      const bool f32_search = ImGui::Button("search float", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input float", ImGuiDataType_Float, &f32_v, 0, 0);
      if (f32_search) {
        search_pattern_in_nodes(std::string((char*)&f32_v, (char*)&f32_v + 4), "");
      }

      static double f64_v;
      const bool f64_search = ImGui::Button("search double", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input double", ImGuiDataType_Double, &f64_v);
      if (f64_search) {
        search_pattern_in_nodes(std::string((char*)&f64_v, (char*)&f64_v + 8), "");
      }

      ImGui::Separator();
      if (ImGui::Button("search bytes from editor clipboard (context/copy)"))
      {
        auto& cp = node_hexeditor::clipboard();
        std::string needle(cp.begin(), cp.end());
        if (needle.size())
          search_pattern_in_nodes(needle, "");
      }

      ImGui::Separator();

      // bool pat_search = ImGui::Button("search pattern", ImVec2(150, 0)); ImGui::SameLine();
      // ImGui::PushItemWidth(slider_width);
      // ImGui::InputText("pattern bytes", search_needle.data(), (int)search_needle.size(), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AlwaysInsertMode);
      // ImGui::InvisibleButton("search pattern##inv", ImVec2(150, 1)); ImGui::SameLine();
      // ImGui::PushItemWidth(slider_width);
      // ImGui::InputTextEx("pattern mask", "x?xxx??x", search_mask.data(), (int)search_mask.size(), ImVec2(0, 0), ImGuiInputTextFlags_AlwaysInsertMode);
      
      // todo, parse hex string

      ImGui::Separator();

      // results

      static std::shared_ptr<node_hexeditor> nh;

      static float u32row_width = ImGui::CalcTextSize("0xFFFFFFFF ").x;

      static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
      ImVec2 size = ImVec2(500, ImGui::GetTextLineHeightWithSpacing() * 28);
      if (ImGui::BeginTable("##searchres_table2", 1, flags, size))
      {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("matches", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)search_result.size());
        while (clipper.Step())
        {
          for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
          {
            auto& match = search_result[row];
            scoped_imgui_id sii{&match};
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            
            static char matchname[512];
            ImFormatString(matchname, 512, "offset 0x%08X in node %4d - %s", match.offset, match.n->idx(), match.n->name().c_str());

            if (ImGui::Selectable(matchname, selected_result == row))
            {
              selected_result = row;
              if (!nh || nh->node() != match.n)
                nh = std::make_shared<node_hexeditor>(match.n);
              nh->select(match.offset, match.size);
            }
          }
        }
        ImGui::EndTable();
      }

      if (nh) {
        bool opened = true;
        ImGui::SameLine(510);
        nh->draw_widget();
        if (!opened)
          nh.reset();
      }

      ImGui::EndChild();
      ImGui::Unindent(5.f);
    }
    
    ImGui::Unindent(indent);

    //m_csav->entry_descs
  }

  static inline std::shared_ptr<const node_t>& selected_node()
  {
    static std::shared_ptr<const node_t> _singleton = {};
    return _singleton;
  }

protected:
  void draw_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
    {
      ImGui::Text("null");
      return;
    }

    auto& sel_node = selected_node();
    bool selected = node == sel_node;
    ImGuiTreeNodeFlags node_flags = selected ? ImGuiTreeNodeFlags_Selected : 0;
    if (!node->has_children())
      node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool opened =
      (node->is_blob() or node->is_root())
      ? ImGui::TreeNodeEx((void*)node.get(), node_flags, "%s", node->name().c_str())
      : ImGui::TreeNodeEx((void*)node.get(), node_flags, "%s (%u)", node->name().c_str(), node->idx());

    if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0))
      sel_node = node;

    if (opened && node->has_children())
    {
      for (auto& child : node->children())
        draw_node(child);

      ImGui::TreePop();
    }
  }

  // hex search.. 

  struct search_match {
    std::shared_ptr<const node_t> n;
    size_t offset;
    size_t size;
  };
  std::vector<search_match> search_result;
  size_t selected_result = (size_t)-1;

  std::vector<search_match>& search_pattern_in_nodes(const std::string& needle, const std::string& mask)
  {
    selected_result = (size_t)-1;
    // history draft
    /*
    std::stringstream ss;
    ss << "bytes:";
    ss << std::hex << std::uppercase;
    for (auto c : needle)
      ss << std::setfill('0') << std::setw(2) << c << " ";
    if (mask.size())
      ss << "mask:" << mask;
    auto it = searches.emplace(ss.str(), std::vector<search_match>());
    */
    search_result.clear();
    if (m_csav and !std::all_of(needle.begin(), needle.end(), [](char i) { return i==0; })) // searching for zeroes is just.. the way to die
      search_pattern_in_nodes_rec(search_result, m_csav->root_node, needle, mask);
    return search_result;
  }

  void search_pattern_in_nodes_rec(std::vector<search_match>& matches, const std::shared_ptr<const node_t> node, const std::string& needle, const std::string& mask)
  {
    auto& haystack = node->data();
    std::vector<uintptr_t> match_offsets;
    if (mask.size())
      match_offsets = sse2_strstr_masked((uint8_t*)haystack.data(), haystack.size(), (uint8_t*)needle.data(), needle.size(), mask.data(), mask.size());
    else
      match_offsets = sse2_strstr((uint8_t*)haystack.data(), haystack.size(), (uint8_t*)needle.data(), needle.size());
    for (auto off : match_offsets)
      matches.emplace_back(search_match{node, off, needle.size()});
    for (auto& c : node->children())
      search_pattern_in_nodes_rec(matches, c, needle, mask);
  }
};

class csav_list_widget
{
protected:
  ImGui::FileBrowser open_dialog;
  loading_bar_job_widget open_job;
  std::filesystem::path open_filepath;
  std::shared_ptr<csav> opened_save;
  std::shared_ptr<AppImage> opened_save_img;

  std::list<csav_collapsable_header> m_list;

public:
  csav_list_widget()
  {
    open_dialog.SetTitle("Opening savefile");
    open_dialog.SetTypeFilters({ ".dat" });
  }

  void update()
  {
    open_job.update();
    if (opened_save)
    {
      m_list.emplace_back(opened_save, opened_save_img);
      opened_save.reset();
      opened_save_img.reset();
    }

    m_list.erase(
      std::remove_if(m_list.begin(), m_list.end(), [](auto& a){ return !a.is_alive(); }),
      m_list.end()
    );

    for (auto& cs : m_list)
      cs.update();
  }

  void draw_list()
  {
    for (auto& cs : m_list)
      cs.draw();
    draw_editor();
  }

  void draw_menu_item(IApp* owning_app)
  {
    if (ImGui::MenuItem("Open savefile", 0, false, !open_job.is_running()))
      open_dialog.Open();

    open_dialog.Display();

    if (open_dialog.HasSelected())
    {
      open_filepath = std::filesystem::absolute(open_dialog.GetSelected().wstring());

      auto screenshot_path = open_filepath;
      screenshot_path.replace_filename(L"screenshot.png");
      if (std::filesystem::exists(screenshot_path))
        opened_save_img = owning_app->load_texture_from_file(screenshot_path.string());

      open_job.start([this](float& progress) -> bool {
        auto cs = std::make_shared<csav>();
        if (!cs->open_with_progress(open_filepath, progress))
          return false;
        opened_save = cs;
        return true;
      });
      ImGui::OpenPopup("Loading..##LOAD");

      open_dialog.ClearSelected();
    }

    if (0 && ImGui::Button("open error"))
      ImGui::OpenPopup("Loading..##LOAD");

    // Always center this window when appearing
    ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool pushed_stylecol = false;
    if (open_job.failed) {
      ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, (ImVec4)ImColor::HSV(0.f, 1.f, 0.6f, 0.35f));
      pushed_stylecol = true;
    }

    if (ImGui::BeginPopupModal("Loading..##LOAD", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("path: %s", open_filepath.string().c_str());

      if (open_job.is_running())
      {
        open_job.draw();
      }
      else if (open_job.failed)
      {
        ImGui::Text("error, couldn't load savefile");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
          ImGui::CloseCurrentPopup();
      }
      else
      {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (pushed_stylecol)
      ImGui::PopStyleColor();
  }

  void draw_editor()
  {
    static std::shared_ptr<node_editor> current_editor {};
    static bool opened = false;
    bool changed_node = false;

    auto& sel_node = csav_collapsable_header::selected_node();
    if (sel_node && (!current_editor || current_editor->node() != sel_node))
    {
      current_editor = node_editor::create(sel_node);
      opened = current_editor != nullptr;
      changed_node = true;
    }
    if (opened)
      current_editor->draw_window(&opened);
    if (!opened)
      sel_node.reset();
  }
};

