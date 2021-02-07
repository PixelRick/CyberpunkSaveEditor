#include "appbase/IApp.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <appbase/extras/imgui_filebrowser.hpp>

#include "cpinternals/tweakdb/tweakdb.hpp"
#include "cpinternals/init.hpp"
#include "appbase/app_version.h"
#include "widgets/tweakdb.hpp"

using namespace std::chrono_literals;

void to_json(nlohmann::json& j, const ImVec4& p)
{
  j = {{"x", p.x}, {"y", p.y}, {"z", p.z}, {"w", p.w}};
}

void from_json(const nlohmann::json& j, ImVec4& p)
{
  j.at("x").get_to(p.x);
  j.at("y").get_to(p.y);
  j.at("z").get_to(p.z);
  j.at("w").get_to(p.w);
}

class CPTEApp
  : public IApp
{
protected:
  std::string credits = "(C) 2020 Even Entem (PixelRick)";
  size_t credits_hash = std::hash<std::string>()(credits);
  uint32_t* doom_ptr = (uint32_t*)0x640;

  std::chrono::time_point<std::chrono::steady_clock> last_refit_time, now;
  ImVec4 bg_color = ImVec4(0.1f, 0.1f, 0.1f, 1.f);
  ImVec4 addr_color = ImVec4(0.f, 0.6f, 0.8f, 1.f);

  cp::tdb::tweakdb tdb;

public:
  CPTEApp()
  {
    m_wndname = L"Cyberpunk 2077\u2122 TweakDB Editor " VERSION_STRING;
    m_display_width = 1600;
    m_display_height = 900;
  }

protected:
  void log_msg(const std::string& msg) {}
  void log_error(const std::string& msg) {}

protected:
  void startup() override
  {
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    io.IniFilename = "imgui_settings.ini";

    // Setup Dear ImGui style
    load_style();
  }

  bool load_style()
  {
    // default first
    ImGui::StyleColorsDark();

    try
    {
      nlohmann::json jstyle;

      std::ifstream style_file;
      style_file.open("style.json");
      style_file >> jstyle;
      style_file.close();

      ImVec4* colors = ImGui::GetStyle().Colors;
      for (size_t i = 0; i < (size_t)ImGuiCol_COUNT; ++i)
      {
        auto colkey = fmt::format("col{:02d}", i);
        if (jstyle.contains(colkey))
          colors[ImGuiCol(i)] = jstyle.at(colkey).get<ImVec4>();
      }

      return true;
    }
    catch (std::exception&)
    {
    }

    return false;
  }

  bool store_style()
  {
    try
    {
      nlohmann::json jstyle;

      ImVec4* colors = ImGui::GetStyle().Colors;
      for (size_t i = 0; i < (size_t)ImGuiCol_COUNT; ++i)
      {
        auto colkey = fmt::format("col{:02d}", i);
        jstyle[colkey] = colors[ImGuiCol(i)];
      }

      std::ofstream style_file;
      style_file.open("style.json");
      style_file << jstyle;
      style_file.close();

      return true;
    }
    catch (std::exception&)
    {
    }

    return false;
  }

  void cleanup() override
  {
    store_style();
  }

  void update() override
  {
    now = std::chrono::steady_clock::now();
  }

  void draw_imgui() override
  {
    static bool imgui_demo = false;
    static bool imgui_style_editor = false;

    static bool cploaded = cp::init_cpinternals();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)m_display_width, (float)m_display_height));

    if (ImGui::Begin("main windows", 0
      , ImGuiWindowFlags_NoTitleBar
      | ImGuiWindowFlags_NoResize
      | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_MenuBar
      //| ImGuiWindowFlags_NoBackground
      | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
      if (ImGui::BeginMainMenuBar())
      {
        if (ImGui::BeginMenu("Options"))
        {
          imgui_style_editor |= ImGui::MenuItem("ui style editor", 0, false);
          ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Dev"))
        {
          if (ImGui::MenuItem("imgui demo", 0, false))
            imgui_demo = true;
          ImGui::EndMenu(); 
        }

        if (ImGui::BeginMenu("About"))
        {
          ImGui::Separator();
          ImGui::Text(credits.c_str());
          ImGui::Separator();
          ImGui::Text("Contributors:");
          ImGui::Text("  Novumordo");
          ImGui::Text("  Gibbed");
          ImGui::EndMenu();
        }

        // mini-dma
        if (credits_hash != 0xa8140380a724d4a2)
          *doom_ptr = 0;

        ImGui::EndMainMenuBar();
      }

      static int selected = 0;
      ui::im_tweakdb(tdb, &selected);
    }
    ImGui::End();


    if (imgui_demo)
    {
      ImGui::ShowDemoWindow(&imgui_demo);
    }

    if (imgui_style_editor)
    {
      ImGui::Begin("ImGui Style Editor", &imgui_style_editor);
      ImGui::ShowStyleEditor();
      ImGui::End();
    }
  }

  bool has_file_drop() const override { return true; }

  void on_file_drop(std::wstring fpath) override
  {
    std::filesystem::path p(fpath);
    p = std::filesystem::canonical(p); // let's try to remove last null character.. todo: fix in IAppLib
    auto fname = p.filename().string();
    if (fname.rfind("tweakdb.bin", 0) == 0)
    {
      tdb.open(fpath);
      return;
    }
  }

  void on_resized() override
  {
  }

  LRESULT window_proc(UINT msg, WPARAM wParam, LPARAM lParam) override
  {
    switch (msg)
    {
      case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
          case SC_KEYMENU: // Disable ALT application menu
            return 0;
          default:
            break;
        }
        break;
      default:
        break;
    }

    return IApp::window_proc(msg, wParam, lParam);
  }
};

CREATE_APPLICATION(CPTEApp)

