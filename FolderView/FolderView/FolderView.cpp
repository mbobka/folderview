// for IDE sake
#include "targetver.h"
#include "Resource.h"

// Windows Header Files
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objidl.h>
#include <shlobj.h>
#include <atlbase.h>
#include <VersionHelpers.h>

// Direct2D
#include <d2d1.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;
#undef min
#undef max

// Standard library
#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "AlmostEquals.h"
#include "ImageDecoder.h"

void BuildExtensionsList();
void BuildFilesList(std::filesystem::path const& file_or_folder);
void PreviousImage(HWND hWnd);
void NextImage(HWND hWnd);
void DrawImage(HWND hWnd);
void SetCurrentImage(HWND hWnd, int imageIndex);
float CalculateScale(HWND hWnd, int width, int height);

// Direct2D globals
ComPtr<ID2D1Factory> g_pD2DFactory;
ComPtr<ID2D1HwndRenderTarget> g_pRenderTarget;

// Кэш декодированных изображений
std::vector<std::unique_ptr<DecodedImage>> decodedImagesCache;

std::vector<std::wstring> extensions;
std::vector<std::filesystem::path> files;

size_t current_image = 0;
bool block_changing_of_current_image = false; // запрещает изменять текущее изображение, например, при перетаскивании мышкой
std::unique_ptr<POINT> image_shift; // насколько изображение сдвинуто

float scale = 1.0f;

static POINTS ptsBegin;        // beginning point 

bool is_file_has_right_extension_for_us(std::filesystem::path const& file) {
	for (auto extension : extensions) {
		if (lstrcmpi(file.extension().c_str(), extension.c_str()) == 0)
			return true;
	}

	return false;
}

HRESULT GetUIObjectOfFile(HWND hwnd, LPCWSTR pszPath, REFIID riid, void** ppv)
{
	*ppv = NULL;
	HRESULT hr;
	LPITEMIDLIST pidl;
	SFGAOF sfgao;
	if (SUCCEEDED(hr = SHParseDisplayName(pszPath, NULL, &pidl, 0, &sfgao))) {
		IShellFolder* psf;
		LPCITEMIDLIST pidlChild;
		if (SUCCEEDED(hr = SHBindToParent(pidl, IID_IShellFolder,
			(void**)&psf, &pidlChild))) {
			hr = psf->GetUIObjectOf(hwnd, 1, &pidlChild, riid, NULL, ppv);
			psf->Release();
		}
		CoTaskMemFree(pidl);
	}
	return hr;
}

void BuildFilesList(std::filesystem::path const& file_or_folder)
{
	std::filesystem::directory_entry entry(file_or_folder);
	if (!entry.exists()) {
		MessageBox(NULL, L"Путь не найден", L"Folder view", MB_OK | MB_ICONERROR);
		return;
	}

	std::filesystem::path files_root;

	if (entry.is_directory()) {
		files_root = entry.path();
	}

	if (entry.is_regular_file()) {
		files_root = entry.path().lexically_normal().parent_path();
	}

	for (auto entryx : std::filesystem::directory_iterator(files_root))
	{
		if (!entryx.is_regular_file())
			continue;

		if (!is_file_has_right_extension_for_us(entryx.path()))
			continue;

		if (entry.path().lexically_normal() == entryx.path().lexically_normal())
			current_image = files.size();

		files.push_back(entryx.path());
	}

	decodedImagesCache.resize(files.size());
}

void BuildExtensionsList()
{
	// Список поддерживаемых расширений (WIC поддерживает их все)
	extensions.push_back(L".jpg");
	extensions.push_back(L".jpeg");
	extensions.push_back(L".png");
	extensions.push_back(L".bmp");
	extensions.push_back(L".gif");
	extensions.push_back(L".tiff");
	extensions.push_back(L".tif");
	extensions.push_back(L".ico");
	extensions.push_back(L".webp");
	extensions.push_back(L".heif");
	extensions.push_back(L".heic");
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_CREATE:
		return 0;

	case WM_MOUSEWHEEL: {
		auto fwKeys = GET_KEYSTATE_WPARAM(wParam);
		auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

		if (zDelta > 0) {
			PreviousImage(hWnd);
		}
		else if (zDelta < 0) {
			NextImage(hWnd);
		}

		return 0;
	}

	case WM_KEYDOWN: {
		auto vkey = static_cast<UINT8>(wParam);

		switch (vkey) {
		case VK_UP:
			if (GetKeyState(VK_CONTROL) & 0x8000)
				goto scale_up;

			[[fallthrough]];
		case VK_NUMPAD8:
		case VK_PRIOR:
			PreviousImage(hWnd);
			return 0;

		case VK_NUMPAD7:
		case VK_HOME:
			if (block_changing_of_current_image)
				return 0;

			current_image = 0;
			SetCurrentImage(hWnd, current_image);
			return 0;

		case VK_NUMPAD1:
		case VK_END:
			if (block_changing_of_current_image)
				return 0;

			current_image = files.size() - 1;
			SetCurrentImage(hWnd, current_image);
			return 0;

		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;

		case VK_DOWN:
			if (GetKeyState(VK_CONTROL) & 0x8000)
				goto scale_down;

			[[fallthrough]];
		case VK_NUMPAD2:
		case VK_NEXT:
			NextImage(hWnd);
			return 0;

		case VK_ADD:
		scale_up:
			if (scale > 10000.0f)
				return 0;

			scale *= 2.0f;
			InvalidateRect(hWnd, NULL, FALSE);
			return 0;

		case VK_SUBTRACT:
		scale_down:
			if (scale < 0.00001f)
				return 0;

			scale *= 0.5f;
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case '0':
		case VK_MULTIPLY:
			if (AlmostEquals(scale, 1.0f)) {
				scale = CalculateScale(hWnd, decodedImagesCache[current_image]->width, decodedImagesCache[current_image]->height);
				if (AlmostEquals(scale, 1.0f))
					return 0;
			}
			else
				scale = 1.0f;

			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case VK_INSERT:
		case 'C':
		{
			if (GetKeyState(VK_CONTROL) & 0x8000) {
				CComPtr<IDataObject> spdto;

				if (FAILED(GetUIObjectOfFile(nullptr, files[current_image].c_str(), IID_PPV_ARGS(&spdto))) ||
					FAILED(OleSetClipboard(spdto)) ||
					FAILED(OleFlushClipboard()))
				{
					MessageBeep(MB_ICONERROR);
					return 0;
				}

				return 0;
			}
		}
		}

		break;
	}

	case WM_LBUTTONDOWN:

		// Capture mouse input. 
		SetCapture(hWnd);

		// Запретить изменять текущее изображение
		block_changing_of_current_image = true;

		// Установить курсор руку.
		SetCursor(LoadCursor(NULL, IDC_HAND));

		// Сохранить начальные координаты.
		ptsBegin = MAKEPOINTS(lParam);
		return 0;

	case WM_MOUSEMOVE:

		// When moving the mouse, the user must hold down 
		// the left mouse button to drag the image. 

		if ((wParam & MK_LBUTTON) && decodedImagesCache[current_image])
		{
			// Установить новые координаты сдвига, ограничивая их изменения
			if (!image_shift) {
				image_shift.reset(new POINT{ 0,0 });
			}

			int imgWidth = decodedImagesCache[current_image]->width;
			int imgHeight = decodedImagesCache[current_image]->height;

			RECT clientRect;
			GetClientRect(hWnd, &clientRect);

			auto cw = clientRect.right - clientRect.left;
			auto ch = clientRect.bottom - clientRect.top;

			// следующим дапазоном: 
			auto limit_x = std::max<LONG>(0, -cw + (LONG)(imgWidth * scale));
			auto limit_y = std::max<LONG>(0, -ch + (LONG)(imgHeight * scale));

			POINT old_position = *image_shift;

			auto dx = -ptsBegin.x + GET_X_LPARAM(lParam);
			auto dy = -ptsBegin.y + GET_Y_LPARAM(lParam);

			image_shift->x += dx;
			image_shift->y += dy;

			auto coalesced_shift_x = -std::max((LONG)0, std::min(limit_x, -image_shift->x));
			auto coalesced_shift_y = -std::max((LONG)0, std::min(limit_y, -image_shift->y));

			image_shift->x = coalesced_shift_x;
			image_shift->y = coalesced_shift_y;

			// Перерисовать содержимое.
			ScrollWindowEx(hWnd, -old_position.x + image_shift->x, -old_position.y + image_shift->y, NULL, NULL, NULL, NULL, SW_INVALIDATE | SW_ERASE);

			// Сохранить новые начальные координаты для отсчёта сдвига от них.
			ptsBegin = MAKEPOINTS(lParam);
		}
		break;

	case WM_LBUTTONUP:
		// Возвратить правильный курсор.
		SetCursor(LoadCursor(NULL, IDC_ARROW));

		// Разрешить изменять текущее изображение
		block_changing_of_current_image = false;

		// Release the mouse capture. 
		ReleaseCapture();
		return 0;

	case WM_SIZE:
		if (g_pRenderTarget) {
			RECT rc;
			GetClientRect(hWnd, &rc);
			D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
			g_pRenderTarget->Resize(size);
		}
		break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);

		DrawImage(hWnd);

		EndPaint(hWnd, &ps);

		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
} // WndProc

float CalculateScale(HWND hWnd, int imgWidth, int imgHeight) {
	WINDOWPLACEMENT wpl = { 0 };
	GetWindowPlacement(hWnd, &wpl);

	float scale = 1.0f;

	if (wpl.showCmd != SW_MAXIMIZE)
		return 1.0f;

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);

	auto cw = clientRect.right - clientRect.left;
	auto ch = clientRect.bottom - clientRect.top;

	if (imgWidth > cw) {
		scale /= imgWidth / (float)cw;
	}

	if (imgHeight * scale > ch) {
		scale /= imgHeight * scale / (float)ch;
	}

	return scale;
}

void UpdateWindowTitle(HWND hWnd, const DecodedImage& image, float scale)
{
	std::wstringstream header;

	header << files[current_image].c_str();

	header << L" [" << image.width << "x" << image.height;

	if (image.hasAlpha)
		header << L" RGBA";
	else
		header << L" RGB";

	header << L"] "
		<< std::fixed << std::setprecision(0) << scale * 100.0f << L"%";

	SetWindowText(hWnd, header.str().c_str());
}

void SetCurrentImage0(HWND hWnd, int imageIndex) {
	image_shift.reset();

	// Проверяем, есть ли уже закешированное декодированное изображение
	DecodedImage* cachedImage = decodedImagesCache[current_image].get();

	if (cachedImage && cachedImage->IsValid()) {
		// Используем кеш, декодирование не нужно
		std::wstringstream debugMsg;
		debugMsg << L"[DECODE] " << files[current_image].filename().c_str() << L": CACHED (skipped)\n";
		OutputDebugStringW(debugMsg.str().c_str());
	}
	else {
		// Декодируем и сохраняем в кеш
		auto decodeStart = std::chrono::high_resolution_clock::now();
		decodedImagesCache[current_image] = ImageDecoderChain::Instance().Decode(files[current_image].c_str());
		auto decodeEnd = std::chrono::high_resolution_clock::now();

		cachedImage = decodedImagesCache[current_image].get();
		if (!cachedImage || !cachedImage->IsValid()) {
			MessageBox(hWnd, L"Не удалось загрузить изображение", L"Ошибка", MB_OK | MB_ICONERROR);
			return;
		}

		auto decodeUs = std::chrono::duration_cast<std::chrono::microseconds>(decodeEnd - decodeStart).count();
		std::wstringstream debugMsg;
		debugMsg << L"[DECODE] " << files[current_image].filename().c_str() << L": " << decodeUs << L" us\n";
		OutputDebugStringW(debugMsg.str().c_str());

		// Очищаем дальний кеш для экономии памяти
		const size_t cache_epsilon = 20;
		for (size_t i = 0; i + cache_epsilon < current_image; i++) {
			decodedImagesCache[i].reset();
		}
		for (size_t i = current_image + cache_epsilon; i < decodedImagesCache.size(); i++) {
			decodedImagesCache[i].reset();
		}
	}

	scale = CalculateScale(hWnd, cachedImage->width, cachedImage->height);
	UpdateWindowTitle(hWnd, *cachedImage, scale);

	WINDOWPLACEMENT wpl = { 0 };
	GetWindowPlacement(hWnd, &wpl);

	if (wpl.showCmd != SW_MAXIMIZE) {
		RECT rect{ 0, 0, (LONG)cachedImage->width, (LONG)cachedImage->height };
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
		SetWindowPos(hWnd, nullptr, 0, 0, -rect.left + rect.right, -rect.top + rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW);
	}
}

void SetCurrentImage(HWND hWnd, int imageIndex) {
	SetCurrentImage0(hWnd, imageIndex);

	// Инвалидируем окно для перерисовки через Direct2D
	InvalidateRect(hWnd, NULL, FALSE);
}

ComPtr<ID2D1Bitmap> CreateD2DBitmapFromDecoded(ID2D1RenderTarget* pRenderTarget, const DecodedImage& image) {
	D2D1_BITMAP_PROPERTIES bitmapProps = D2D1::BitmapProperties(
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
			image.hasAlpha ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE)
	);

	ComPtr<ID2D1Bitmap> d2dBitmap;
	HRESULT hr = pRenderTarget->CreateBitmap(
		D2D1::SizeU(image.width, image.height),
		image.pixels.data(),
		image.stride,
		bitmapProps,
		&d2dBitmap
	);

	if (FAILED(hr)) {
		return nullptr;
	}

	return d2dBitmap;
}

void DrawImage(HWND hWnd) {
	auto drawStart = std::chrono::high_resolution_clock::now();

	DecodedImage* cachedImage = decodedImagesCache[current_image].get();
	if (!g_pRenderTarget || !cachedImage || !cachedImage->IsValid())
		return;

	// Проверяем состояние render target
	D2D1_WINDOW_STATE windowState = g_pRenderTarget->CheckWindowState();
	if (windowState & D2D1_WINDOW_STATE_OCCLUDED) {
		return;
	}

	int imgWidth = cachedImage->width;
	int imgHeight = cachedImage->height;

	// Создаём D2D bitmap из DecodedImage
	ComPtr<ID2D1Bitmap> d2dBitmap = CreateD2DBitmapFromDecoded(g_pRenderTarget.Get(), *cachedImage);

	if (!d2dBitmap)
		return;

	// Получаем размеры клиентской области
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	float cw = static_cast<float>(clientRect.right - clientRect.left);
	float ch = static_cast<float>(clientRect.bottom - clientRect.top);

	// Вычисляем позицию изображения (центрирование)
	float scaledWidth = imgWidth * scale;
	float scaledHeight = imgHeight * scale;

	float destX = std::max(0.0f, (cw - scaledWidth) / 2.0f);
	float destY = std::max(0.0f, (ch - scaledHeight) / 2.0f);

	// Применяем сдвиг если есть
	if (image_shift) {
		destX += image_shift->x;
		destY += image_shift->y;
	}

	D2D1_RECT_F destRect = D2D1::RectF(destX, destY, destX + scaledWidth, destY + scaledHeight);

	g_pRenderTarget->BeginDraw();

	// Заливаем фон
	g_pRenderTarget->Clear(D2D1::ColorF(0.66f, 0.66f, 0.66f)); // DarkGray

	// Рисуем изображение с интерполяцией
	g_pRenderTarget->DrawBitmap(
		d2dBitmap.Get(),
		destRect,
		1.0f,
		D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
	);

	HRESULT hr = g_pRenderTarget->EndDraw();

	if (hr == D2DERR_RECREATE_TARGET) {
		// Render target потерян, нужно пересоздать
		g_pRenderTarget.Reset();
	}

	auto drawEnd = std::chrono::high_resolution_clock::now();
	auto drawUs = std::chrono::duration_cast<std::chrono::microseconds>(drawEnd - drawStart).count();
	std::wstringstream debugMsg;
	debugMsg << L"[DRAW] " << drawUs << L" us\n";
	OutputDebugStringW(debugMsg.str().c_str());
}

void PreviousImage(HWND hWnd)
{
	if (block_changing_of_current_image)
		return;

	if (current_image == 0) {
		current_image = 0;
		return;
	}

	current_image--;

	SetCurrentImage(hWnd, current_image);
}

void NextImage(HWND hWnd)
{
	if (block_changing_of_current_image)
		return;

	if (current_image + 1 >= files.size()) {
		return;
	}

	current_image++;

	SetCurrentImage(hWnd, current_image);
}

std::vector<std::wstring> parse_command_line()
{
	std::vector<std::wstring>	result;

	LPWSTR* szArglist;
	int nArgs;
	int i;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (NULL == szArglist)
	{
		return result;
	}

	for (i = 0; i < nArgs; i++) {
		result.push_back(std::wstring(szArglist[i]));
	}

	LocalFree(szArglist);

	return result;
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
	if (OleInitialize(NULL) != S_OK)
		return -1;

	HWND hWnd;
	MSG msg;

	auto parameters = parse_command_line();

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Инициализируем Direct2D Factory
	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, g_pD2DFactory.GetAddressOf());
	if (FAILED(hr)) {
		MessageBox(NULL, L"Failed to create Direct2D factory", L"Error", MB_OK | MB_ICONERROR);
		return -1;
	}

	// Инициализируем цепочку декодеров
	ImageDecoderChain::Initialize();

	BuildExtensionsList();

	auto root = parameters.size() == 2 ? std::filesystem::path(parameters[1]) : std::filesystem::current_path();
	BuildFilesList(root);

	HBRUSH background = CreateSolidBrush(RGB(169, 169, 169)); // DarkGray

	WNDCLASSEXW wndClass{};

	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = background;
	wndClass.lpszMenuName = nullptr;
	wndClass.lpszClassName = TEXT("FolderView");
	wndClass.hIconSm = nullptr;

	RegisterClassEx(&wndClass);

	hWnd = CreateWindow(
		wndClass.lpszClassName,   // window class name
		TEXT("Folder view"),      // window caption
		WS_OVERLAPPEDWINDOW,      // window style
		CW_USEDEFAULT,            // initial x position
		CW_USEDEFAULT,            // initial y position
		CW_USEDEFAULT,            // initial x size
		CW_USEDEFAULT,            // initial y size
		NULL,                     // parent window handle
		NULL,                     // window menu handle
		hInstance,                // program instance handle
		NULL);                    // creation parameters

	// Создаём Direct2D Render Target для окна с аппаратным ускорением
	RECT rc;
	GetClientRect(hWnd, &rc);
	D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

	D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_HARDWARE,  // Принудительно GPU
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
		96.0f, 96.0f
	);

	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRtProps = D2D1::HwndRenderTargetProperties(
		hWnd,
		size,
		D2D1_PRESENT_OPTIONS_IMMEDIATELY  // Без VSync для максимальной скорости
	);

	hr = g_pD2DFactory->CreateHwndRenderTarget(rtProps, hwndRtProps, &g_pRenderTarget);

	// Если GPU недоступен, fallback на software
	if (FAILED(hr)) {
		rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
		hr = g_pD2DFactory->CreateHwndRenderTarget(rtProps, hwndRtProps, &g_pRenderTarget);

		OutputDebugStringW(L"GPU render target creation failed, falling back to software rendering.\n");
	}

	if (FAILED(hr)) {
		MessageBox(NULL, L"Failed to create Direct2D render target", L"Error", MB_OK | MB_ICONERROR);
		return -1;
	}

	ShowWindow(hWnd, SW_MAXIMIZE);

	SetCurrentImage0(hWnd, 0);

	UpdateWindow(hWnd);


	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Освобождаем кеш
	decodedImagesCache.clear();
	g_pRenderTarget.Reset();
	g_pD2DFactory.Reset();

	DeleteObject(background);

	// Освобождаем ресурсы декодеров
	ImageDecoderChain::Shutdown();

	OleUninitialize();

	return (int)msg.wParam;
}  // WinMain
