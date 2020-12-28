#pragma once

#include "pch.h"

#include "imgui/imgui/imgui.h"
#include "imgui/imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
static inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }

template <typename T>
struct imgui_datatype_of { static constexpr ImGuiDataType value = -1; };
template <> struct imgui_datatype_of< uint8_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_U8; };
template <> struct imgui_datatype_of<uint16_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_U16; };
template <> struct imgui_datatype_of<uint32_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_U32; };
template <> struct imgui_datatype_of<uint64_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_U64; };
template <> struct imgui_datatype_of<  int8_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_S8; };
template <> struct imgui_datatype_of< int16_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_S16; };
template <> struct imgui_datatype_of< int32_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_S32; };
template <> struct imgui_datatype_of< int64_t> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_S64; };
template <> struct imgui_datatype_of<   float> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_Float; };
template <> struct imgui_datatype_of<  double> { static constexpr ImGuiDataType value = ImGuiDataType_::ImGuiDataType_Double; };

struct scoped_imgui_id {
	scoped_imgui_id(const char* str_id) { ImGui::PushID(str_id); }
	scoped_imgui_id(void* ptr_id) { ImGui::PushID(ptr_id); }
	scoped_imgui_id(int int_id) { ImGui::PushID(int_id); }
	~scoped_imgui_id() { ImGui::PopID(); }
};

struct scoped_imgui_style_color {
	scoped_imgui_style_color(ImGuiCol idx, ImVec4 col) { ImGui::PushStyleColor(ImGuiCol_Text, col); }
	~scoped_imgui_style_color() { ImGui::PopStyleColor(); }
};

struct scoped_imgui_button_hue {
	scoped_imgui_button_hue(float hue) {
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(hue, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(hue, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(hue, 0.8f, 0.8f)); }
	~scoped_imgui_button_hue() { ImGui::PopStyleColor(3); }
};


#define WINDOW_STYLE WS_OVERLAPPEDWINDOW

class AppImage
{
private:
	ComPtr<ID3D11ShaderResourceView> tex;
	int w, h;

public:
	AppImage(const ComPtr<ID3D11ShaderResourceView>& tex, int w, int h)
		: tex(tex), w(w), h(h) {}

	void draw(ImVec2 size = {0, 0}, bool rescale=true)
	{
		if (size.x <= 0)
		{
			size.x = (float)w;
			if (rescale && size.y > 0)
				size.x *= size.y / (float)h;
		}
		if (size.y <= 0)
		{
			size.y = (float)h;
			if (rescale)
				size.y *= size.x / (float)w;
		}
		ImGui::Image((void*)tex.Get(), size);
	}
};

class IApp
{
public:
	virtual ~IApp() {}

	int run();

	virtual LRESULT window_proc(UINT msg, WPARAM wParam, LPARAM lParam);
	HWND get_hwnd() { return m_hwnd; }

protected:
	virtual void startup() = 0;
	virtual void cleanup() = 0;
	virtual void update() = 0;
	virtual void draw_imgui() = 0;
	virtual void on_resized() {};
	virtual bool quitting() { return false; }

public:
	std::shared_ptr<AppImage> load_texture_from_file(const std::string& filename);

private:
	bool d3d_init();
	void d3d_fini();
	void d3d_present();
	void d3d_resize(uint32_t width, uint32_t height);
	void d3d_create_rtv();
	void d3d_cleanup_rtv();

private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_devctx;
	ComPtr<IDXGISwapChain> m_swapchain;
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<IDCompositionDevice> m_dcomp_device;
	ComPtr<IDCompositionTarget> m_dcomp_target;
	ComPtr<IDCompositionVisual> m_dcomp_visual;
	HANDLE m_swapchain_waitable_object;

protected:
	std::wstring m_wndname = L"IApp";
	HWND m_hwnd = 0;
	uint32_t m_display_width = 1280;
	uint32_t m_display_height = 800;
	ImVec4 m_bg_color = ImVec4(0.2f, 0.2f, 0.3f, 1.f);
};

#define CREATE_APPLICATION(app_class) \
	int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nShowCmd) \
	{ \
		auto app = std::make_unique<app_class>(); \
		return app->run(); \
	}

