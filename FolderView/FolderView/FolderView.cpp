// for IDE sake
#include "targetver.h"
#include "Resource.h"

// Windows Header Files
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <atlbase.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")
#undef min
#undef max

// Standard library
#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>

#include "AlmostEquals.h"

void BuildExtensionsList();
void UpdateWindowContent(HWND hWnd);
void BuildFilesList(std::filesystem::path const& file_or_folder);
void PreviousImage(HWND hWnd);
void NextImage(HWND hWnd);
void DrawCurrentImage(HWND hWnd);
void CalculateScale(HWND hWnd);

std::vector<std::wstring> extensions;
std::vector<std::filesystem::path> files;

std::vector<std::tuple<std::unique_ptr<CachedBitmap>, int, int>> bitmaps;

size_t current_image = 0;
bool block_changing_of_current_image = false; // запрещает изменять текущее изображение, например, при перетаскивании мышкой

std::unique_ptr<Bitmap> image;
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

	bitmaps.resize(files.size());
}

void BuildExtensionsList()
{
	UINT decoders_count, decoders_size;
	Gdiplus::GetImageDecodersSize(&decoders_count, &decoders_size);

	auto decoders = reinterpret_cast<ImageCodecInfo*>(malloc(decoders_size));
	Gdiplus::GetImageDecoders(decoders_count, decoders_size, decoders);

	for (int i = 0; i < decoders_count; i++) {
		std::wistringstream f(decoders[i].FilenameExtension);
		std::wstring s;
		while (std::getline(f, s, L';')) {
			extensions.push_back(s.c_str() + 1);
		}
	}

	free(decoders);
}

Rect CalculateImageRect(HWND hwnd) {
	RectF	imageBounds;
	Unit	imageUnits;
	image->GetBounds(&imageBounds, &imageUnits);

	RECT	clientRect;
	GetClientRect(hwnd, &clientRect);

	auto cw = clientRect.right - clientRect.left;
	auto ch = clientRect.bottom - clientRect.top;

	auto dw = (cw - imageBounds.Width * scale) / 2.0f;
	auto dh = (ch - imageBounds.Height * scale) / 2.0f;

	auto x = std::max(0.0f, dw);
	auto y = std::max(0.0f, dh);

	int iw = std::round(std::min<float>(cw, imageBounds.Width * scale));
	int ih = std::round(std::min<float>(ch, imageBounds.Height * scale));

	return Rect(Point(x, y), Size(iw, ih));
}

Rect old_image_rect, new_image_rect;

LPCTSTR pixelFormat(PixelFormat pf) {
	switch(pf) {
	case PixelFormat1bppIndexed:   return TEXT("1bpp Indexed");
	case PixelFormat4bppIndexed:   return TEXT("4bpp Indexed");
	case PixelFormat8bppIndexed:   return TEXT("8bpp Indexed");
	case PixelFormat16bppGrayScale:return TEXT("16bpp GrayScale");
	case PixelFormat16bppRGB555:   return TEXT("16bpp RGB555");
	case PixelFormat16bppRGB565:   return TEXT("16bpp RGB565");
	case PixelFormat16bppARGB1555: return TEXT("16bpp ARGB1555");
	case PixelFormat24bppRGB:	   return TEXT("24bpp RGB");
	case PixelFormat32bppRGB:	   return TEXT("32bpp RGB");
	case PixelFormat32bppARGB:	   return TEXT("32bpp ARGB");
	case PixelFormat32bppPARGB:	   return TEXT("32bpp PARGB");
	case PixelFormat48bppRGB:	   return TEXT("48bpp RGB");
	case PixelFormat64bppARGB:	   return TEXT("64bpp ARGB");
	case PixelFormat64bppPARGB:	   return TEXT("64bpp PARGB");
	case PixelFormat32bppCMYK:	   return TEXT("32bpp CMYK");
	}
	return nullptr;
}

LPCTSTR getFormat(GUID& formatGuid) {
	if (IsEqualGUID(formatGuid, ImageFormatBMP)) return TEXT("BMP");
	if (IsEqualGUID(formatGuid, ImageFormatEMF)) return TEXT("EMF");
	if (IsEqualGUID(formatGuid, ImageFormatWMF)) return TEXT("WMF");
	if (IsEqualGUID(formatGuid, ImageFormatJPEG)) return TEXT("JPEG");
	if (IsEqualGUID(formatGuid, ImageFormatPNG)) return TEXT("PNG");
	if (IsEqualGUID(formatGuid, ImageFormatGIF)) return TEXT("GIF");
	if (IsEqualGUID(formatGuid, ImageFormatTIFF)) return TEXT("TIFF");
	if (IsEqualGUID(formatGuid, ImageFormatEXIF)) return TEXT("EXIF");
	if (IsEqualGUID(formatGuid, ImageFormatIcon)) return TEXT("Icon");
	if (IsEqualGUID(formatGuid, ImageFormatHEIF)) return TEXT("HEIF");
	if (IsEqualGUID(formatGuid, ImageFormatWEBP)) return TEXT("WEBP");

	return nullptr;
}

void UpdateWindowContent(HWND hWnd)
{
	std::unique_ptr<Bitmap> old_image;

	if (image) {
		old_image_rect = CalculateImageRect(hWnd);
	}

	old_image.swap(image);

	image.reset(Bitmap::FromFile(files[current_image].c_str()));

	std::wstringstream	header;

	CalculateScale(hWnd);

	WINDOWPLACEMENT wpl;
	GetWindowPlacement(hWnd, &wpl);

	auto pixel_format = image->GetPixelFormat();
	SizeF bounds;
	image->GetPhysicalDimension(&bounds);

	header
		<< files[current_image].c_str();
		
	GUID	raw_format;
	image->GetRawFormat(&raw_format);

	if (getFormat(raw_format))
		header << L" " << getFormat(raw_format);

	header
		<< L" [" << (int)bounds.Width << "x" << (int)bounds.Height;
	
	if (pixelFormat(pixel_format))
		header << L" " << pixelFormat(pixel_format);

	header
		<< L"] "
		<< std::fixed << std::setprecision(0) << scale * 100.0f << L"%";

	SetWindowText(hWnd, header.str().c_str());

	if (wpl.showCmd != SW_MAXIMIZE) {
		RECT rect{ 0, 0, bounds.Width, bounds.Height };
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
		SetWindowPos(hWnd, nullptr, 0, 0, -rect.left + rect.right, -rect.top + rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW);
	}

	new_image_rect = CalculateImageRect(hWnd);
	if (!old_image)
		old_image_rect = new_image_rect;
	else
		old_image.reset();

	InvalidateRect(hWnd, nullptr, TRUE);
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

		case VK_NUMPAD8:
		case VK_PRIOR:
			PreviousImage(hWnd);
			return 0;

		case VK_NUMPAD7:
		case VK_HOME:
			if (block_changing_of_current_image)
				return 0;

			current_image = 0;
			DrawCurrentImage(hWnd);
			return 0;

		case VK_NUMPAD1:
		case VK_END:
			if (block_changing_of_current_image)
				return 0;

			current_image = files.size() - 1;
			DrawCurrentImage(hWnd);
			return 0;

		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;

		case VK_DOWN:
			if (GetKeyState(VK_CONTROL) & 0x8000)
				goto scale_down;

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
				CalculateScale(hWnd);
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

		if (wParam & MK_LBUTTON)
		{
			// Установить новые координаты сдвига, ограничивая их изменения
			if (!image_shift) {
				image_shift.reset(new POINT{ 0,0 });
			}

			RectF	imageBounds;
			Unit	imageUnits;
			image->GetBounds(&imageBounds, &imageUnits);

			RECT	clientRect;
			GetClientRect(hWnd, &clientRect);

			auto cw = clientRect.right - clientRect.left;
			auto ch = clientRect.bottom - clientRect.top;

			// следующим дапазоном: 
			auto limit_x = std::max<LONG>(0, -cw + imageBounds.Width * scale);
			auto limit_y = std::max<LONG>(0, -ch + imageBounds.Height * scale);

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

	case WM_ERASEBKGND: {
		Graphics hp((HDC)wParam);

		RECT	clientRect;
		GetClientRect(hWnd, &clientRect);

		Rect overlap;

		if (!image) {
			Rect r0(clientRect.left, clientRect.top, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

			SolidBrush f0(Color::DarkGray);

			hp.FillRectangle(&f0, r0);

			return 1L;
		}

		if (Rect::Intersect(overlap, new_image_rect, old_image_rect)) {
			int oix = clientRect.left;
			int oiy = clientRect.top;
			int oixw = clientRect.right;
			int oiyh = clientRect.bottom;

			int ovx = overlap.X;
			int ovy = overlap.Y;
			int ovxw = overlap.X + overlap.Width;
			int ovyh = overlap.Y + overlap.Height;

			Rect r0(oix, oiy, oixw - oix, ovy - oiy);
			Rect r1(oix, ovy, ovx - oix, ovyh - ovy);
			Rect r2(ovxw, ovy, oixw - ovxw, ovyh - ovy);
			Rect r3(oix, ovyh, oixw - oix, oiyh - ovyh);

			SolidBrush f0(Color::DarkGray);
			SolidBrush f1(Color::DarkGray);
			SolidBrush f2(Color::DarkGray);
			SolidBrush f3(Color::DarkGray);

			hp.FillRectangle(&f0, r0);
			hp.FillRectangle(&f1, r1);
			hp.FillRectangle(&f2, r2);
			hp.FillRectangle(&f3, r3);
		}

		return 1L;
	}

	case WM_PAINT:
	{
		if (!image)
			UpdateWindowContent(hWnd);

		PAINTSTRUCT  ps;
		HDC          hdc = BeginPaint(hWnd, &ps);

		auto image_rect = CalculateImageRect(hWnd);

		Graphics hp(hdc);

		CachedBitmap* cached_bitmap = std::get<0>(bitmaps[current_image]).get();

		int cached_w = std::get<1>(bitmaps[current_image]);
		int cached_h = std::get<2>(bitmaps[current_image]);

		if (image_shift || !cached_bitmap || cached_w != image_rect.Width || cached_h != image_rect.Height)
		{
			Bitmap  b(image_rect.Width, image_rect.Height);
			std::unique_ptr<Graphics> g(Graphics::FromImage(&b));

			if (image_shift)
				g->TranslateTransform(image_shift->x, image_shift->y);

			g->ScaleTransform(scale, scale);

			RectF	imageBounds;
			Unit	imageUnits;
			image->GetBounds(&imageBounds, &imageUnits);

			g->DrawImage(image.get(), 0.0f, 0.0f, imageBounds.Width, imageBounds.Height);

			g.reset();

			cached_bitmap = new CachedBitmap(&b, &hp);

			cached_h = image_rect.Height;
			cached_w = image_rect.Width;

			bitmaps[current_image] = std::make_tuple(std::unique_ptr<CachedBitmap>(cached_bitmap), cached_w, cached_h);

			const int cache_epsilon = 20; // сколько в окресности держать кешированных изображений
			for (int i = 0; i < (int)current_image - cache_epsilon; i++) {
				bitmaps[i] = std::make_tuple(std::unique_ptr<CachedBitmap>(), 0, 0);
			}
			for (int i = current_image + cache_epsilon; i < bitmaps.size(); i++) {
				bitmaps[i] = std::make_tuple(std::unique_ptr<CachedBitmap>(), 0, 0);
			}
		}

		hp.DrawCachedBitmap(cached_bitmap, image_rect.X, image_rect.Y);

		EndPaint(hWnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
} // WndProc

void CalculateScale(HWND hWnd) {
	WINDOWPLACEMENT wpl;
	GetWindowPlacement(hWnd, &wpl);

	scale = 1.0f;

	if (wpl.showCmd != SW_MAXIMIZE)
		return;

	RectF	imageBounds;
	Unit	imageUnits;
	image->GetBounds(&imageBounds, &imageUnits);

	RECT	clientRect;
	GetClientRect(hWnd, &clientRect);

	auto cw = clientRect.right - clientRect.left;
	auto ch = clientRect.bottom - clientRect.top;

	if (imageBounds.Width > cw) {
		scale /= imageBounds.Width / (float)cw;
	}

	if (imageBounds.Height * scale > ch) {
		scale /= imageBounds.Height * scale / (float)ch;
		imageBounds.Height = ch;
	}
}

void DrawCurrentImage(HWND hWnd) {
	UpdateWindowContent(hWnd);
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
	image_shift.reset();

	DrawCurrentImage(hWnd);
}

void NextImage(HWND hWnd)
{
	if (block_changing_of_current_image)
		return;

	if (current_image + 1 >= files.size()) {
		return;
	}

	current_image++;
	image_shift.reset();

	DrawCurrentImage(hWnd);
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
	OleInitialize(NULL);

	HWND                hWnd;
	MSG                 msg;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	auto parameters = parse_command_line();

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	BuildExtensionsList();

	auto root = parameters.size() == 2 ? std::filesystem::path(parameters[1]) : std::filesystem::current_path();
	BuildFilesList(root);

	WNDCLASSEXW wndClass;
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = nullptr;
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

	ShowWindow(hWnd, SW_MAXIMIZE);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	image.reset();

	bitmaps.clear();

	GdiplusShutdown(gdiplusToken);

	OleUninitialize();

	return msg.wParam;
}  // WinMain
