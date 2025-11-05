///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "crashhandler.h"

#include <windows.h>
#include <dbghelp.h>

// Use common controls v6, rather than v5
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDC_EXIT_BUTTON     1500
#define IDC_COPY_BUTTON     1501
#define IDC_GITHUB_BUTTON   1502

static HWND hWindow;
static HWND hLabel;
static HWND hEdit;
static HWND hGithubButton;
static HWND hCopyButton;
static HWND hExitButton;

static qtwebapp::LoggerWithFile *crashLogger;

static void ScaleWindow(HWND wnd, int x, int y, int w, int h)
{
    int dpi = GetDpiForWindow(wnd);

    int scaledX = MulDiv(x, dpi, USER_DEFAULT_SCREEN_DPI);
    int scaledY = MulDiv(y, dpi, USER_DEFAULT_SCREEN_DPI);
    int scaledW = MulDiv(w, dpi, USER_DEFAULT_SCREEN_DPI);
    int scaledH = MulDiv(h, dpi, USER_DEFAULT_SCREEN_DPI);

    SetWindowPos(wnd, wnd, scaledX, scaledY, scaledW, scaledH, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void Scale(HWND wnd, int& w, int &h)
{
    int dpi = GetDpiForWindow(wnd);

    w = MulDiv(w, dpi, USER_DEFAULT_SCREEN_DPI);
    h = MulDiv(h, dpi, USER_DEFAULT_SCREEN_DPI);
}

static void Unscale(HWND wnd, int& w, int &h)
{
    int dpi = GetDpiForWindow(wnd);

    w = MulDiv(w, USER_DEFAULT_SCREEN_DPI, dpi);
    h = MulDiv(h, USER_DEFAULT_SCREEN_DPI, dpi);
}

static void ScaleControls()
{
    RECT rect;
    int w;
    int h;

    GetWindowRect(hWindow, &rect);
    w = rect.right - rect.left;
    h = rect.bottom - rect.top;
    Unscale(hWindow, w, h);

    int buttonY = h - 100;
    int editW = w - 40;
    int editH = h - 200;

    if (hLabel) {
        ScaleWindow(hLabel, 10, 10, editW, 70);
    }
    if (hEdit) {
        ScaleWindow(hEdit, 10, 80, editW, editH);
    }
    if (hGithubButton) {
        ScaleWindow(hGithubButton, 10, buttonY, 150, 30);
    }
    if (hCopyButton) {
        ScaleWindow(hCopyButton, 170, buttonY, 150, 30);
    }
    if (hExitButton) {
        ScaleWindow(hExitButton, w - 180, buttonY, 150, 30);
    }
}

static void ScaleWindow()
{
    if (hWindow)
    {
        RECT rect;

        GetWindowRect(hWindow, &rect);
        ScaleWindow(hWindow, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    }
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);

        switch (wmId)
        {

        case IDC_EXIT_BUTTON:
            PostQuitMessage(0);
            break;

        case IDC_GITHUB_BUTTON:
            // Open SDRangel GitHub issues page in web browser
            ShellExecute(NULL, TEXT("open"), TEXT("https://github.com/f4exb/sdrangel/issues"), NULL, NULL, SW_SHOWNORMAL);
            break;

        case IDC_COPY_BUTTON:
        {
            // Copy contents of edit control to clipboard
            int len = GetWindowTextLength(hEdit);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));

            if (hMem)
            {
                void *text = GlobalLock(hMem);
                if (text)
                {
                    GetWindowText(hEdit, (LPTSTR) text, len);
                    GlobalUnlock(hMem);
                    OpenClipboard(0);
                    EmptyClipboard();
                    SetClipboardData(CF_UNICODETEXT, hMem);
                    CloseClipboard();
                }
            }
            break;
        }

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        LPCREATESTRUCT cs = (LPCREATESTRUCT) lParam;

        hLabel = CreateWindow(TEXT("Static"),
            TEXT("SDRangel has crashed.\r\n\r\nPlease consider opening a bug report on GitHub, copying the text below and a screenshot."),
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0,
            hWnd,
            NULL,
            hInst,
            NULL);

        hEdit = CreateWindow(TEXT("EDIT"),
            (LPTSTR) cs->lpCreateParams,
            WS_BORDER | WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0,
            hWnd,
            NULL,
            hInst,
            NULL);

        hGithubButton = CreateWindow(
            TEXT("BUTTON"),
            TEXT("Open Github..."),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            0, 0, 0, 0,
            hWnd,
            (HMENU) IDC_GITHUB_BUTTON,
            hInst,
            NULL);

        hCopyButton = CreateWindow(
            TEXT("BUTTON"),
            TEXT("Copy text"),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            0, 0, 0, 0,
            hWnd,
            (HMENU) IDC_COPY_BUTTON,
            hInst,
            NULL);

        hExitButton = CreateWindow(
            TEXT("BUTTON"),
            TEXT("Exit SDRangel"),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            0, 0, 0, 0,
            hWnd,
            (HMENU) IDC_EXIT_BUTTON,
            hInst,
            NULL);

        ScaleControls();

        break;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO) lParam;
        int w = 500;
        int h = 220;
        Scale(hWindow, w, h);

        lpMMI->ptMinTrackSize.x = w;
        lpMMI->ptMinTrackSize.y = h;
        break;
    }

    case WM_SIZE:
        ScaleControls();
        RedrawWindow(hWindow, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        break;

    case WM_DPICHANGED:
        ScaleControls();
        ScaleWindow();
        break;

    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC) wParam, TRANSPARENT);
        return (LRESULT) GetStockObject(NULL_BRUSH);

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Create and display crash log when an unhandled exception occurs
static LONG crashHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    const int reportBufferSize = 1024*1024;
    char *reportBuffer = new char[reportBufferSize];
    char *reportBufferPtr = reportBuffer;
    int reportBufferRemaining = reportBufferSize;

    int written = snprintf(reportBufferPtr, reportBufferRemaining,
        "SDRangel crash data\r\n%s %s\r\nQt %s\r\n%s %s\r\nStack trace:\r\n",
        APPLICATION_NAME,
        SDRANGEL_VERSION,
        QT_VERSION_STR,
        qPrintable(QSysInfo::prettyProductName()),
        qPrintable(QSysInfo::currentCpuArchitecture())
    );
    reportBufferPtr += written;
    reportBufferRemaining -= written;

    // Create stack trace

    STACKFRAME64 stack;
    CONTEXT context;
    HANDLE process;
    DWORD64 displacement;
    ULONG frame;
    BOOL symInit;
    char symName[(MAX_PATH * sizeof(TCHAR))];
    char storage[sizeof(IMAGEHLP_SYMBOL64) + (sizeof(symName))];
    IMAGEHLP_SYMBOL64* symbol;

    RtlCaptureContext(&context);
    memset(&stack, 0, sizeof(STACKFRAME64));
#if defined(_AMD64_)
    stack.AddrPC.Offset    = context.Rip;
    stack.AddrPC.Mode      = AddrModeFlat;
    stack.AddrStack.Offset = context.Rsp;
    stack.AddrStack.Mode   = AddrModeFlat;
    stack.AddrFrame.Offset = context.Rbp;
    stack.AddrFrame.Mode   = AddrModeFlat;
#else
    stack.AddrPC.Offset    = context.Eip;
    stack.AddrPC.Mode      = AddrModeFlat;
    stack.AddrStack.Offset = context.Esp;
    stack.AddrStack.Mode   = AddrModeFlat;
    stack.AddrFrame.Offset = context.Ebp;
    stack.AddrFrame.Mode   = AddrModeFlat;
#endif
    displacement = 0;
    process = GetCurrentProcess();
    symInit = SymInitialize(process, "plugins", TRUE);
    symbol = (IMAGEHLP_SYMBOL64*) storage;

    for (frame = 0; reportBufferRemaining > 0; frame++)
    {
        BOOL result = StackWalk(IMAGE_FILE_MACHINE_AMD64,
            process,
            GetCurrentThread(),
            &stack,
            &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL);

        if (result)
        {
            symbol->SizeOfStruct = sizeof(storage);
            symbol->MaxNameLength = sizeof(symName);

            BOOL symResult = SymGetSymFromAddr64(process, (ULONG64)stack.AddrPC.Offset, &displacement, symbol);
            if (symResult) {
                UnDecorateSymbolName(symbol->Name, (PSTR)symName, sizeof(symName), UNDNAME_COMPLETE);
            }

            written = snprintf(
                reportBufferPtr,
                reportBufferRemaining,
                "%02u 0x%p  %s\r\n",
                frame,
                stack.AddrPC.Offset,
                symResult ? symbol->Name : "Unknown"
            );
            if (written > 0)
            {
                if (written <= reportBufferRemaining)
                {
                    reportBufferPtr += written;
                    reportBufferRemaining -= written;
                }
                else
                {
                    reportBufferPtr += reportBufferRemaining;
                    reportBufferRemaining = 0;
                }
            }
        }
        else
        {
            break;
        }
    }

    // Append log file
    if (crashLogger)
    {
        QString log = crashLogger->getBufferLog();
        log = log.replace('\n', "\r\n");
        written = snprintf(reportBufferPtr, reportBufferRemaining, "Log:\r\n%s\r\n", qPrintable(log));
        if (written > 0)
        {
            reportBufferPtr += written;
            reportBufferRemaining -= written;
        }
    }

    // Create a window to display the crash report
    // Can't use QMessageBox here, as may not work depending where the crash was, so need to use Win32 API

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE); // Needed, otherwise GetDpiForWindow always returns 96, rather than the actual value

    TCHAR windowClass[] = TEXT("SDRangel Crash Window Class");
    HMODULE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = { };

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = windowClass;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);

    RegisterClassEx(&wc);

    hWindow = CreateWindow(windowClass, TEXT("SDRangel Crash"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 640, 500,
        NULL, NULL, hInstance, (LPVOID) reportBuffer
    );
    if (hWindow)
    {
        ScaleWindow();
        ShowWindow(hWindow, SW_SHOWNORMAL);
        UpdateWindow(hWindow);

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    else
    {
        fprintf(stderr, "Failed to create window\n");
        fprintf(stderr, reportBuffer);
    }

    delete[] reportBuffer;

    return EXCEPTION_EXECUTE_HANDLER;
}

void installCrashHandler(qtwebapp::LoggerWithFile *logger)
{
    crashLogger = logger;
    SetUnhandledExceptionFilter(crashHandler);
}
