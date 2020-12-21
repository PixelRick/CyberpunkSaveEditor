#include "IApp.h"
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void ShowD3DError(HRESULT hr)
{
	LPWSTR output;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&output, 0, NULL);
	MessageBox(NULL, output, L"D3DError", MB_OK);
}

#define HRCHECK(x) \
	{ \
		HRESULT _hr = (x); \
		if (FAILED(_hr)) \
			ShowD3DError(_hr); \
	}

IApp* g_runningApp = nullptr;

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const HWND current_hwnd = g_runningApp->get_hwnd();
	if (current_hwnd != NULL) {
		assert(hWnd == current_hwnd);
		if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
			return 1;
		return g_runningApp->window_proc(message, wParam, lParam);
	}
	return ::DefWindowProc(hWnd, message, wParam, lParam);
}

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WTIME WIDE1(__TIME__)

int IApp::run()
{
	g_runningApp = this;
	HINSTANCE hInst = ::GetModuleHandle(0);

	const wchar_t* className = m_wndname.c_str();

	// Register class
	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_CLASSDC;
	wcex.lpfnWndProc = ::WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInst;
	wcex.hIcon = NULL;//::LoadIcon(hInst, IDI_APPLICATION);
	wcex.hCursor = NULL;//::LoadCursor(nullptr, IDC_ARROW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = className;
	wcex.hIconSm = NULL;//::LoadIcon(hInst, IDI_APPLICATION);
	if (::RegisterClassEx(&wcex) == 0)
		throw std::runtime_error("unable to register window class");

	// init window
	/*
	RECT drc;
	GetClientRect(GetDesktopWindow(), &drc);
	m_display_width = drc.right > 400 ? drc.right - 200 : 200;
	m_display_height = drc.bottom > 400 ? drc.bottom - 200 : 200;
	*/
	RECT rc = { 0, 0, (LONG)m_display_width, (LONG)m_display_height };
	//::AdjustWindowRect(&rc, WINDOW_STYLE, FALSE);

	m_hwnd = ::CreateWindowEx(
		WS_EX_CLIENTEDGE, // WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST,
		className, className, WINDOW_STYLE, 100, 100,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

	if (m_hwnd == 0)
	{
		::UnregisterClass(wcex.lpszClassName, wcex.hInstance);
		return GetLastError() || 1;
	}

	if (!d3d_init())
	{
		::UnregisterClass(wcex.lpszClassName, wcex.hInstance);
		return 1;
	}

	::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(m_hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(m_hwnd);
	ImGui_ImplDX11_Init(m_device.Get(), m_devctx.Get());

	startup();

	MSG msg = {};
	bool running = true;
	while (running && !quitting())
	{
		/*while*/ if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				running = false;
		}

		if (running)
		{
			update();

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			draw_imgui();

			ImGui::Render();
			m_devctx->OMSetRenderTargets(1, m_rtv.GetAddressOf(), NULL);
			m_devctx->ClearRenderTargetView(m_rtv.Get(), (float*)&m_bg_color);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			d3d_present();
		}
	}

	cleanup();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	d3d_fini();
	::DestroyWindow(m_hwnd);
	::UnregisterClass(wcex.lpszClassName, wcex.hInstance);

	return 0;
}

LRESULT IApp::window_proc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SIZE:
		if (m_device.Get() && wParam != SIZE_MINIMIZED)
			d3d_resize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}
	return ::DefWindowProc(m_hwnd, message, wParam, lParam);
}

bool IApp::d3d_init()
{
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 2;
	sd.BufferDesc.Width = m_display_width;
	sd.BufferDesc.Height = m_display_height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 144;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	// createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, 
		D3D11_SDK_VERSION, &sd, &m_swapchain, &m_device, &featureLevel, &m_devctx);
	HRCHECK(hr);
	if (hr != S_OK)
		return false;

	d3d_create_rtv();
	return true;
}

void IApp::d3d_fini()
{
	d3d_cleanup_rtv();
	m_swapchain.Reset();
	m_devctx.Reset();
	m_device.Reset();
}

void IApp::d3d_present()
{
	HRCHECK(m_swapchain->Present(1, 0));
}

void IApp::d3d_resize(uint32_t width, uint32_t height)
{
	if (width < 10 || height < 10)
		return;

	m_display_width = width;
	m_display_height = height;

	d3d_cleanup_rtv();
	HRCHECK(m_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));
	d3d_create_rtv();

	on_resized();
}

void IApp::d3d_create_rtv()
{
	ComPtr<ID3D11Texture2D> backbuffer;
	HRCHECK(m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
	HRCHECK(m_device->CreateRenderTargetView(backbuffer.Get(), NULL, &m_rtv));
}

void IApp::d3d_cleanup_rtv()
{
	m_rtv.Reset();
}

std::shared_ptr<AppImage> IApp::load_texture_from_file(const std::string& filename)
{
	if (!m_device)
		return {};

	int w = 0, h = 0;
	// Load from disk into a raw RGBA buffer
	unsigned char* image_data = stbi_load(filename.c_str(), &w, &h, NULL, 4);
	if (image_data == NULL)
		return {};

	ComPtr<ID3D11ShaderResourceView> tex;

	// Create texture
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	ComPtr<ID3D11Texture2D> texture;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = image_data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	m_device->CreateTexture2D(&desc, &subResource, &texture);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, &tex);

	stbi_image_free(image_data);

	return std::make_shared<AppImage>(tex, w, h);
}

