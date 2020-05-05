#include "framework.h"

void BuildExtensionsList();
void UpdateWindowContent(HWND hWnd);
void BuildFilesList(std::filesystem::path const& file_or_folder);
void PreviousImage(HWND hWnd);
void NextImage(HWND hWnd);
void DrawCurrentImage(HWND hWnd);

std::vector<std::wstring> extensions;
std::vector<std::filesystem::path> files;
size_t current_image = 0;

std::unique_ptr<Bitmap> image;
std::unique_ptr<CachedBitmap> cache;

VOID OnPaint(HDC hdc)
{
	Graphics graphics(hdc);

	if (!cache)
		cache.reset(new CachedBitmap(image.get(), &graphics));

	graphics.DrawCachedBitmap(cache.get(), 0, 0);
}

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
		MessageBox(NULL, L"ѕуть не найден", L"Folder view", MB_OK | MB_ICONERROR);
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
	cache.reset();

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
	SetWindowText(hWnd, files[current_image].c_str());
	SetWindowPos(hWnd, nullptr, 0, 0, -rect.left + rect.right, -rect.top + rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC          hdc;
	PAINTSTRUCT  ps;

	switch (message)
	{
	case WM_CREATE:
		UpdateWindowContent(hWnd);
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
			current_image = 0;
			DrawCurrentImage(hWnd);
			return 0;

		case VK_NUMPAD1:
		case VK_END:
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

void DrawCurrentImage(HWND hWnd) {
	UpdateWindowContent(hWnd);
	InvalidateRect(hWnd, NULL, FALSE);
}

void PreviousImage(HWND hWnd)
{
	if (current_image == 0) {
		MessageBeep(MB_ICONWARNING);
		current_image = 0;
		return;
	}

	current_image--;

	DrawCurrentImage(hWnd);
}

void NextImage(HWND hWnd)
{
	if (current_image + 1 >= files.size()) {
		MessageBeep(MB_ICONWARNING);
		return;
	}

	current_image++;

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
	BuildFilesList(std::filesystem::path(parameters[1]));

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
	cache.reset();

	DeleteObject(background);

	GdiplusShutdown(gdiplusToken);
	return msg.wParam;
}  // WinMain
