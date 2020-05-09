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
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")
#undef min
#undef max

// Standard library
#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>

void BuildExtensionsList();
void UpdateWindowContent(HWND hWnd);
void BuildFilesList(std::filesystem::path const& file_or_folder);
void PreviousImage(HWND hWnd);
void NextImage(HWND hWnd);
void DrawCurrentImage(HWND hWnd);
void CalculateScale(HWND hWnd);

std::vector<std::wstring> extensions;
std::vector<std::filesystem::path> files;
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

void UpdateWindowContent(HWND hWnd)
{
	std::unique_ptr<Bitmap> old_image;

	old_image.swap(image);

	image.reset(Bitmap::FromFile(files[current_image].c_str()));

	SetWindowText(hWnd, files[current_image].c_str());

	CalculateScale(hWnd);

	WINDOWPLACEMENT wpl;
	GetWindowPlacement(hWnd, &wpl);

	SizeF bounds;
	image->GetPhysicalDimension(&bounds);

	if (wpl.showCmd == SW_MAXIMIZE) {
		if (!old_image)
			return;

		SizeF old_bounds;
		old_image->GetPhysicalDimension(&old_bounds);

		{
			RECT rect{ bounds.Width, 0, old_bounds.Width, old_bounds.Height };
			if (rect.left <= rect.right)
				InvalidateRect(hWnd, &rect, TRUE);
		}

		{
			RECT rect{ 0, bounds.Height, old_bounds.Width, old_bounds.Height };
			if (rect.top <= rect.bottom)
				InvalidateRect(hWnd, &rect, TRUE);
		}

		old_image.reset();

		return;
	}

	RECT rect{ 0, 0, bounds.Width, bounds.Height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	SetWindowPos(hWnd, nullptr, 0, 0, -rect.left + rect.right, -rect.top + rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW);
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
		case VK_NUMPAD8:
		case VK_UP:
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

		case VK_NUMPAD2:
		case VK_DOWN:
		case VK_NEXT:
			NextImage(hWnd);
			return 0;
		}

		return 0;
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

	case WM_PAINT:
	{
		if (!image)
			UpdateWindowContent(hWnd);

		PAINTSTRUCT  ps;
		HDC          hdc = BeginPaint(hWnd, &ps);

		Graphics graphics(hdc);

		RectF	imageBounds;
		Unit	imageUnits;
		image->GetBounds(&imageBounds, &imageUnits);

		RECT	clientRect;
		GetClientRect(hWnd, &clientRect);

		auto cw = clientRect.right - clientRect.left;
		auto ch = clientRect.bottom - clientRect.top;

		auto dw = (cw - imageBounds.Width * scale) / 2.0f;
		auto dh = (ch - imageBounds.Height * scale) / 2.0f;

		auto x = std::max(0.0f, dw);
		auto y = std::max(0.0f, dh);

		if (image_shift) {
			x += image_shift->x;
			y += image_shift->y;
		}

		graphics.DrawImage(image.get(), std::round(x), std::round(y), imageBounds.Width * scale, imageBounds.Height * scale);

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
	InvalidateRect(hWnd, NULL, FALSE);
}

void PreviousImage(HWND hWnd)
{
	if (block_changing_of_current_image)
		return;

	if (current_image == 0) {
		MessageBeep(MB_ICONWARNING);
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
		MessageBeep(MB_ICONWARNING);
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
	HWND                hWnd;
	MSG                 msg;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	auto parameters = parse_command_line();

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	BuildExtensionsList();

	BuildFilesList(parameters.size() == 2 ? std::filesystem::path(parameters[1]) : std::filesystem::current_path());

	HBRUSH background = CreateSolidBrush(RGB(128, 128, 128));

	WNDCLASSEXW wndClass;
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

	ShowWindow(hWnd, SW_MAXIMIZE);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	image.reset();

	DeleteObject(background);

	GdiplusShutdown(gdiplusToken);
	return msg.wParam;
}  // WinMain
