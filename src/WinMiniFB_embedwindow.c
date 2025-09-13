#include "MiniFB.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../thirdparty/wacup/wa_ipc.h" // wacup ipc api

#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static WNDCLASS s_wc;
static HWND s_wnd;
static int s_close = 0;
static int s_width;
static int s_height;
static int s_scale = 1;
static HDC s_hdc;
static void* s_buffer;
static BITMAPINFO* s_bitmapInfo;
static char key_status[512] = {};

static int win_x = 0;
static int win_y = 0;

extern In_Module mod;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT res = 0;

    switch (message) {
        case WM_PAINT: {
            if (s_buffer) {
                StretchDIBits(s_hdc, 0, 0, s_width * s_scale, s_height * s_scale, 0, 0, s_width, s_height, s_buffer,
                              s_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

                ValidateRect(hWnd, NULL);
            }

            break;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            key_status[wParam] = !(lParam >> 31 & 1);
            break;
        }

        //case WM_CLOSE: {
            // where we're going we dont need s_close = 1 and break;
        //}
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

embedWindowState myWindowState;
HWND hMainWnd = NULL;
HWND parent = NULL;

const char* title;
static const char* g_title = NULL;

int mfb_open(const char* title, int width, int height, int scale) {

    mfb_configRead();
    int styles;
    HWND (*e)(embedWindowState *v);

    g_title = title;

    myWindowState.flags = EMBED_FLAGS_NORESIZE | EMBED_FLAGS_NOWINDOWMENU | EMBED_FLAGS_SCALEABLE_WND;

    // left/top are the position (relative to the embed parent)
    myWindowState.r.left   = win_x;
    myWindowState.r.top    = win_y;

    // make right/bottom be coordinates (not raw widths) so right-left == desired width
    myWindowState.r.right  = myWindowState.r.left + ((width) * scale) + 19;   // -> right = left + width
    myWindowState.r.bottom = myWindowState.r.top  + ((height) * scale) + 34;  // -> bottom = top + height

    *(void**)&e = (void *)SendMessage(mod.hMainWindow,WM_WA_IPC,(LPARAM)0,IPC_GET_EMBEDIF);

    if (!e)
    {
		MessageBox(mod.hMainWindow,"This plugin requires Winamp 5.0+","blah",MB_OK);
		return 1;
    }

    parent = e(&myWindowState);

    SetWindowText(myWindowState.me, title); // set our window title

    {	// Register our window class
		WNDCLASS wc;
		memset(&wc,0,sizeof(wc));
		wc.lpfnWndProc = WndProc;				// our window procedure
		wc.hInstance = mod.hDllInstance;	// hInstance of DLL
		wc.lpszClassName = title;			// our window class name
        ATOM atom = RegisterClass(&wc);
        if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBox(mod.hMainWindow, "Error registering window class", "blah", MB_OK);
            return 1;
        }
	}

    s_width = width;
    s_height = height;
    s_scale = scale;

    styles = WS_VISIBLE|WS_CHILDWINDOW|WS_OVERLAPPED|WS_CLIPCHILDREN|WS_CLIPSIBLINGS;
    // compute width/height from RECT (important)
    int win_w = myWindowState.r.right  - myWindowState.r.left;
    int win_h = myWindowState.r.bottom - myWindowState.r.top;

    s_wnd = CreateWindowEx(
        0,
        title,
        NULL,
        styles,
        myWindowState.r.left,   // x (relative to parent)
        myWindowState.r.top,    // y
        win_w,                  // width = right - left
        win_h,                  // height = bottom - top
        parent,
        NULL,
        mod.hDllInstance,
        0
    );

#ifdef _WIN64
    // Store pointer safely on 64-bit
    SetWindowLongPtr(s_wnd, GWLP_USERDATA, (LONG_PTR)&mod);
#else
    // Store pointer safely on 32-bit
    SetWindowLong(s_wnd, GWL_USERDATA, (LONG)(INT_PTR)&mod);
#endif
        //SendMessage(this_mod->hwndParent, WM_WA_IPC, (int)hMainWnd, IPC_SETVISWND);

        if (s_wnd) {
            ShowWindow(parent,SW_SHOWNORMAL);
        }

    s_bitmapInfo = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3);
    s_bitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    s_bitmapInfo->bmiHeader.biPlanes = 1;
    s_bitmapInfo->bmiHeader.biBitCount = 16;
    s_bitmapInfo->bmiHeader.biCompression = BI_BITFIELDS;
    s_bitmapInfo->bmiHeader.biWidth = width;
    s_bitmapInfo->bmiHeader.biHeight = -height;

    ((DWORD *)s_bitmapInfo->bmiColors)[2] = 0xF800;
    ((DWORD *)s_bitmapInfo->bmiColors)[1] = 0x07E0;
    ((DWORD *)s_bitmapInfo->bmiColors)[0] = 0x001F;
    s_hdc = GetDC(s_wnd);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer, int fps_limit) {
    static DWORD previousFrameTime = 0;
    MSG msg;

    s_buffer = buffer;

    InvalidateRect(s_wnd, NULL, TRUE);
    SendMessage(s_wnd, WM_PAINT, 0, 0);

    // set to NULL instead of s_wnd to prevent the window being unmovable
    // and it not updating its top window border, the other sides are just black
    // does not fix gen_ff being a genuine piece of shit (close to 5% CPU usage, runtime error)
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (fps_limit) {
        const DWORD targetFrameTime = 1000 / fps_limit;
        const DWORD elapsedFrameTime = GetTickCount() - previousFrameTime;

        if (elapsedFrameTime < targetFrameTime) {
            Sleep(targetFrameTime - elapsedFrameTime);
        }

        previousFrameTime = GetTickCount();
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close() {
    mfb_configWrite();
    s_buffer = 0;
    free(s_bitmapInfo);
    ReleaseDC(s_wnd, s_hdc);
    if (myWindowState.me)
    {
        SetForegroundWindow(mod.hMainWindow);
        DestroyWindow(myWindowState.me);
    }
    UnregisterClass(title,mod.hDllInstance); // unregister window class
}

char * mfb_keystatus() {
    return key_status;
}

static void mfb_getIniFile(wchar_t* ini_file) {
    // Get the Winamp plugin directory
    wchar_t *plugdir=(wchar_t*)SendMessage(mod.hMainWindow,WM_WA_IPC,0,IPC_GETINIDIRECTORYW);

    // Check if plugdir is valid
    if (plugdir == nullptr) {
        // Error handling: Unable to retrieve plugin directory
        return;
    }

    // Concatenate the plugin directory with the desired INI file name
    wcscpy(ini_file, plugdir);
    wcscat(ini_file, L"\\Plugins\\in_nes.ini");
}

void mfb_configRead() {
    wchar_t ini_file[MAX_PATH];
    mfb_getIniFile(ini_file);

    win_x = GetPrivateProfileIntW(L"smolnes", L"Screen_x", win_x, ini_file);
    win_y = GetPrivateProfileIntW(L"smolnes", L"Screen_y", win_y, ini_file);
}

void mfb_configWrite() {
    if (!parent) return;

    RECT r;
    GetWindowRect(parent, &r);

    // convert absolute screen coords → relative to parent’s client area
    HWND parentParent = GetParent(parent);

    POINT pt = { r.left, r.top };
    ScreenToClient(parentParent, &pt);
    win_x = pt.x;
    win_y = pt.y;

    wchar_t string[32];
    wchar_t ini_file[MAX_PATH];
    mfb_getIniFile(ini_file);

    wsprintfW(string, L"%d", win_x);
    WritePrivateProfileStringW(L"smolnes", L"Screen_x", string, ini_file);
    wsprintfW(string, L"%d", win_y);
    WritePrivateProfileStringW(L"smolnes", L"Screen_y", string, ini_file);
}
