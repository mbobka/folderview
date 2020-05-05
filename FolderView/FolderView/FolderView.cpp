#include "framework.h"

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

std::vector<std::filesystem::path> files;

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
	HWND                hWnd;
	MSG                 msg;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	for (auto entry : std::filesystem::directory_iterator(std::filesystem::path(L"\\\\server\\d$\\--- Archive ---\\Matsuura Aya - 2009 - Concert Tour 2009 Aki ~Omoi Afurete~ Visual Book")))
	{
		if (!entry.is_regular_file())
			continue;

		if (lstrcmpi(entry.path().extension().c_str(), L".jpg") != 0)
			continue;

		files.push_back(entry.path());
	}


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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	HDC          hdc;
	PAINTSTRUCT  ps;

	switch (message)
	{
	case WM_CREATE: {
		image.reset(Bitmap::FromFile(files[1].c_str()));

		SizeF bounds;
		image->GetPhysicalDimension(&bounds);

		UINT dpi = GetDpiForWindow(hWnd);

		RECT rect{ 0,0, bounds.Width, bounds.Height };

		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
		SetWindowPos(hWnd, nullptr, 0, 0, -rect.left + rect.right, -rect.top + rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
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
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
} // WndProc