#include "framework.h"
#include <sstream>

std::unique_ptr<Bitmap> image;
std::unique_ptr<CachedBitmap> cache;

VOID OnPaint(HDC hdc)
{
	Graphics graphics(hdc);

	if (!cache)
		cache.reset(new CachedBitmap(image.get(), &graphics));

	graphics.DrawCachedBitmap(cache.get(), 0, 0);
}


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void BuildExtensionsList();
void UpdateWindowContent(HWND hWnd);
void BuildFilesList();

std::vector<std::wstring> extensions;
std::vector<std::filesystem::path> files;
int	current_image = 0;

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
	HWND                hWnd;
	MSG                 msg;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	BuildExtensionsList();
	BuildFilesList();

	WNDCLASSEXW wndClass;
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndClass.lpszMenuName = nullptr;
	wndClass.lpszClassName = TEXT("FolderView");
	wndClass.hIconSm = nullptr;

	RegisterClassEx(&wndClass);

	hWnd = CreateWindow(
		wndClass.lpszClassName,   // window class name
		TEXT("Getting Started"),  // window caption
		WS_OVERLAPPEDWINDOW,      // window style
		CW_USEDEFAULT,            // initial x position
		CW_USEDEFAULT,            // initial y position
		CW_USEDEFAULT,            // initial x size
		CW_USEDEFAULT,            // initial y size
		NULL,                     // parent window handle
		NULL,                     // window menu handle
		hInstance,                // program instance handle
		NULL);                    // creation parameters


	ShowWindow(hWnd, iCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	image.reset();
	cache.reset();

	GdiplusShutdown(gdiplusToken);
	return msg.wParam;
}  // WinMain

bool is_file_has_right_extension_for_us(std::filesystem::path const& file) {
	for (auto extension : extensions) {
		if (lstrcmpi(file.extension().c_str(), L".jpg") == 0)
			return true;
	}

	return false;
}

void BuildFilesList()
{
	for (auto entry : std::filesystem::directory_iterator(std::filesystem::path(L"\\\\server\\d$\\--- Archive ---\\Matsuura Aya - 2009 - Concert Tour 2009 Aki ~Omoi Afurete~ Visual Book")))
	{
		if (!entry.is_regular_file())
			continue;

		if (!is_file_has_right_extension_for_us(entry.path()))
			continue;

		files.push_back(entry.path());
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

void UpdateWindowContent(HWND hWnd) {
	image.reset(Bitmap::FromFile(files[current_image].c_str()));
	cache.reset();

	SizeF bounds;
	image->GetPhysicalDimension(&bounds);

	RECT rect{ 0,0, bounds.Width, bounds.Height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	SetWindowPos(hWnd, nullptr, 0, 0, -rect.left + rect.right, -rect.top + rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC          hdc;
	PAINTSTRUCT  ps;

	switch (message)
	{
	case WM_CREATE: {
		UpdateWindowContent(hWnd);
		return 0;
	}
	case WM_KEYDOWN: {
		auto vkey = static_cast<UINT8>(wParam);

		switch (vkey) {
		case VK_NUMPAD8:
		case VK_UP:
			current_image--;
			if (current_image < 0) {
				MessageBeep(MB_ICONWARNING);
				current_image = 0;
				return 0;
			}

			UpdateWindowContent(hWnd);
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case VK_NUMPAD7:
		case VK_HOME:
			current_image = 0;
			UpdateWindowContent(hWnd);
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case VK_NUMPAD1:
		case VK_END:
			current_image = files.size() - 1;
			UpdateWindowContent(hWnd);
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;

		case VK_NUMPAD2:
		case VK_DOWN:
			current_image++;
			if (current_image >= files.size()) {
				MessageBeep(MB_ICONWARNING);
				current_image = files.size() - 1;
				return 0;
			}

			UpdateWindowContent(hWnd);
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;
		}

		return 0;
	}

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		OnPaint(hdc);
		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	}
	return DefWindowProc(hWnd, message, wParam, lParam);
} // WndProc