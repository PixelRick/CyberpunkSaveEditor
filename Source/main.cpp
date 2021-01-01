#include "AppLib/IApp.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "imgui_extras/imgui_filebrowser.hpp"
#include "nlohmann/json.hpp"
#include "fmt/format.h"

#include "widgets/csav_widget.hpp"
#include "widgets/node_editors/hexedit.hpp"

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

class CPSEApp
	: public IApp
{
protected:
	std::string credits = "(C) 2020 Even Entem (PixelRick)";
	size_t credits_hash = std::hash<std::string>()(credits);
	uint32_t* doom_ptr = (uint32_t*)0x640;

	std::chrono::time_point<std::chrono::steady_clock> last_refit_time, now;
	ImVec4 bg_color = ImVec4(0.1f, 0.1f, 0.1f, 1.f);
	ImVec4 addr_color = ImVec4(0.f, 0.6f, 0.8f, 1.f);
	csav_list_widget csav_list;

public:
	CPSEApp()
	{
		m_wndname = L"Cyberpunk 2077\u2122 Save Editor v0.4.6-alpha (CP_v1.06)";
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

		// Load Fonts
		// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
		// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
		// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
		// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
		// - Read 'misc/fonts/README.txt' for more instructions and details.
		// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
		//io.Fonts->AddFontDefault();
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
		//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
		//IM_ASSERT(font != NULL);
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
		csav_list.update();
	}

	void draw_imgui() override
	{
		static bool test_hexeditor = false;
		static bool imgui_demo = false;
		static bool imgui_style_editor = false;

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
				csav_list.draw_menu_item(this);

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
					ImGui::Text("  Skiller");
					ImGui::Text("  Seberoth");
					ImGui::Text("  SirBitesalot");
					ImGui::Text("  khuong");
					ImGui::Text("  gibbed");
					ImGui::EndMenu();
				}

				// mini-dma
				if (credits_hash != 0xa8140380a724d4a2)
					*doom_ptr = 0;

				ImGui::EndMainMenuBar();
			}
			csav_list.draw_list();
		}
		ImGui::End();

		auto& emgr = hexeditor_windows_mgr::get();
		emgr.draw_windows();

		static auto testnode = node_t::create_shared(123, "testnode");
		if (test_hexeditor)
			emgr.open_window(testnode);

		if (imgui_demo)
			ImGui::ShowDemoWindow(&imgui_demo);

		if (imgui_style_editor)
		{
			ImGui::Begin("ImGui Style Editor", &imgui_style_editor);
			ImGui::ShowStyleEditor();
			ImGui::End();
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

CREATE_APPLICATION(CPSEApp)

