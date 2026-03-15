#include <Interface/Pipline/DepthTexture.hxx>
#include <Interface/Pipline/RenderPipeline.hxx>

#include <Interface/Font/TextRenderSystem.hxx>

#include <AMboard/Editor/BoardEditor.hxx>

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/FlowPin.hxx>

#include <chrono>
#include <iostream>

int main()
{

    CBoardEditor Window;

    CDepthTexture DepthTexture { Window };

    Window.StartFrame();
    while (true) {

        const auto EventState = Window.ProcessEvent();
        if (EventState == CWindowBase::EWindowEventState::Closed)
            break;
        if (EventState == CWindowBase::EWindowEventState::Minimized)
            continue;

        if (const auto RenderContext = Window.GetRenderContext(DepthTexture)) {
            Window.RenderBoard(RenderContext);
        }
    }

    return 0;
}

/*

#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

// Note: This only checks first-level dependencies!
void PrintDllImports(const char* dllPath) {
    HMODULE hMod = LoadLibraryExA(dllPath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hMod) return;

    ULONG size;
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)
        ImageDirectoryEntryToData(hMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);

    if (importDesc) {
        std::cout << "Dependencies for " << dllPath << ":\n";
        while (importDesc->Name) {
            LPCSTR dllName = (LPCSTR)((PBYTE)hMod + importDesc->Name);
            std::cout << " - " << dllName << "\n";

            // You could add logic here to check if 'dllName' exists in the PATH

            importDesc++;
        }
    }
    FreeLibrary(hMod);
}

int main()
{
    PrintDllImports("NodeExts/ExtCommonNode.dll");
}

 */

/*
#define NOMINMAX

#include <windowsx.h>
#include <windows.h>
#include <algorithm>
#include <cmath>

// --- Global Variables for the Snipping Thread ---
static HBITMAP hScreenBmp = NULL;
static POINT ptStart = { 0, 0 };
static POINT ptEnd = { 0, 0 };
static bool bDragging = false;
static int screenX, screenY, screenWidth, screenHeight;

// Helper function to copy the cropped bitmap to the Windows Clipboard
void CopyBitmapToClipboard(HWND hWnd, HBITMAP hbm) {
    if (OpenClipboard(hWnd)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hbm);
        CloseClipboard();
    }
}

// Window Procedure for the Snipping Overlay
LRESULT CALLBACK SnippingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_LBUTTONDOWN:
        bDragging = true;
        ptStart.x = LOWORD(lParam);
        ptStart.y = HIWORD(lParam);
        ptEnd = ptStart;
        SetCapture(hWnd); // Lock mouse input to this window
        break;

    case WM_MOUSEMOVE:
        if (bDragging) {
            ptEnd.x = LOWORD(lParam);
            ptEnd.y = HIWORD(lParam);
            InvalidateRect(hWnd, NULL, FALSE); // Trigger WM_PAINT
        }
        break;

    case WM_LBUTTONUP:
    {
        if (bDragging) {
            bDragging = false;
            ReleaseCapture();
            ptEnd.x = LOWORD(lParam);
            ptEnd.y = HIWORD(lParam);

            // Calculate the selected rectangle dimensions
            int left = std::min(ptStart.x, ptEnd.x);
            int top = std::min(ptStart.y, ptEnd.y);
            int width = std::abs(ptEnd.x - ptStart.x);
            int height = std::abs(ptEnd.y - ptStart.y);

            if (width > 0 && height > 0) {
                HDC hdcScreen = GetDC(NULL);
                HDC hdcMemSource = CreateCompatibleDC(hdcScreen);
                HDC hdcMemDest = CreateCompatibleDC(hdcScreen);

                // Create a new bitmap for just the cropped area
                HBITMAP hCropBmp = CreateCompatibleBitmap(hdcScreen, width, height);

                SelectObject(hdcMemSource, hScreenBmp);
                SelectObject(hdcMemDest, hCropBmp);

                // Copy the selected area from the frozen screen to the new bitmap
                BitBlt(hdcMemDest, 0, 0, width, height, hdcMemSource, left, top, SRCCOPY);

                // Send to clipboard
                CopyBitmapToClipboard(hWnd, hCropBmp);

                // Cleanup GDI objects
                DeleteDC(hdcMemSource);
                DeleteDC(hdcMemDest);
                ReleaseDC(NULL, hdcScreen);
                DeleteObject(hCropBmp);
            }

            // Close the snipping tool overlay
            PostQuitMessage(0);
        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // --- Double Buffering to prevent flickering ---
        HDC hdcBuffer = CreateCompatibleDC(hdc);
        HBITMAP hbmBuffer = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
        HBITMAP hbmOldBuffer = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

        // 1. Draw the frozen screen background into the buffer
        HDC hdcScreenBmp = CreateCompatibleDC(hdc);
        HBITMAP hbmOldScreen = (HBITMAP)SelectObject(hdcScreenBmp, hScreenBmp);
        BitBlt(hdcBuffer, 0, 0, screenWidth, screenHeight, hdcScreenBmp, 0, 0, SRCCOPY);
        SelectObject(hdcScreenBmp, hbmOldScreen);
        DeleteDC(hdcScreenBmp);

        // 2. Draw the red selection rectangle into the buffer
        if (bDragging) {
            int left = std::min(ptStart.x, ptEnd.x);
            int top = std::min(ptStart.y, ptEnd.y);
            int right = std::max(ptStart.x, ptEnd.x);
            int bottom = std::max(ptStart.y, ptEnd.y);

            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent fill

            HPEN hOldPen = (HPEN)SelectObject(hdcBuffer, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcBuffer, hBrush);

            Rectangle(hdcBuffer, left, top, right, bottom);

            SelectObject(hdcBuffer, hOldPen);
            SelectObject(hdcBuffer, hOldBrush);
            DeleteObject(hPen);
        }

        // 3. Copy the fully drawn buffer to the actual screen
        BitBlt(hdc, 0, 0, screenWidth, screenHeight, hdcBuffer, 0, 0, SRCCOPY);

        // Cleanup buffer
        SelectObject(hdcBuffer, hbmOldBuffer);
        DeleteObject(hbmBuffer);
        DeleteDC(hdcBuffer);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { // Press ESC to cancel
            PostQuitMessage(0);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// --- The Isolated Snipping Thread ---
DWORD WINAPI SnippingToolThread(LPVOID lpParam) {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // 1. ISOLATE DPI AWARENESS TO THIS THREAD ONLY (Windows 10+)
    HMODULE hUser32 = LoadLibrary("user32.dll");
    typedef DPI_AWARENESS_CONTEXT(WINAPI* SetThreadDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
    SetThreadDpiAwarenessContextProc setThreadDpiAwareness = NULL;
    DPI_AWARENESS_CONTEXT previousDpiContext = NULL;

    if (hUser32) {
        setThreadDpiAwareness = (SetThreadDpiAwarenessContextProc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        if (setThreadDpiAwareness) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is defined as ((DPI_AWARENESS_CONTEXT)-4)
            previousDpiContext = setThreadDpiAwareness(((DPI_AWARENESS_CONTEXT)-4));
        }
    }

    // 2. Get Virtual Screen Metrics (Covers all monitors)
    screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 3. Capture the entire virtual screen
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    hScreenBmp = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hScreenBmp);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, screenX, screenY, SRCCOPY);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // 4. Register Window Class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = SnippingWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_CROSS); // Crosshair cursor
    wcex.lpszClassName = "SnippingToolOverlayClass";
    RegisterClassEx(&wcex);

    // 5. Create borderless, topmost window covering all monitors
    HWND hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // TOPMOST keeps it above everything
        "SnippingToolOverlayClass", "Snip",
        WS_POPUP | WS_VISIBLE,
        screenX, screenY, screenWidth, screenHeight,
        NULL, NULL, hInstance, NULL
    );

    // 6. Message Loop for this thread
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 7. Cleanup
    if (hScreenBmp) DeleteObject(hScreenBmp);
    UnregisterClass("SnippingToolOverlayClass", hInstance);

    // 8. Restore previous DPI state for this thread
    if (setThreadDpiAwareness && previousDpiContext) {
        setThreadDpiAwareness(previousDpiContext);
    }
    if (hUser32) FreeLibrary(hUser32);

    return 0;
}

// --- Main Application Entry Point ---
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // Simulate your main application running...
    MessageBox(NULL, "Click OK to launch the Snipping Tool.\n\n(Press ESC to cancel the snip once it starts)", "Main Application", MB_OK | MB_ICONINFORMATION);

    // Launch the Snipping Tool in a completely separate thread.
    // This ensures your main application's DPI state and message loop are NOT affected.
    HANDLE hThread = CreateThread(NULL, 0, SnippingToolThread, NULL, 0, NULL);

    if (hThread) {
        // Wait for the user to finish snipping before closing the main app
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    MessageBox(NULL, "Snipping complete! Check your clipboard.", "Main Application", MB_OK | MB_ICONINFORMATION);

    return 0;
}
 */

/*
#define NOMINMAX

#include <algorithm>
#include <cmath>
#include <thread>
#include <windows.h>

// --- Global Variables for the Snipping Thread ---
static HBITMAP hScreenBmp = NULL;
static POINT ptStart = { 0, 0 };
static POINT ptEnd = { 0, 0 };
static bool bDragging = false;
static int screenX, screenY, screenWidth, screenHeight;

// --- New Variables for Window Clamping ---
static HWND g_TargetWindow = NULL;
static RECT g_TargetRectClient = { 0, 0, 0, 0 };
static bool g_bHasTarget = false;

// Helper function to copy the cropped bitmap to the Windows Clipboard
void CopyBitmapToClipboard(HWND hWnd, HBITMAP hbm)
{
    if (OpenClipboard(hWnd)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hbm);
        CloseClipboard();
    }
}

// Window Procedure for the Snipping Overlay
LRESULT CALLBACK SnippingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_LBUTTONDOWN: {
        // Use (short) to properly handle negative coordinates in multi-monitor setups
        POINT ptClicked = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

        // 1. If we have a target window, ensure the user clicked INSIDE it
        if (g_bHasTarget) {
            if (!PtInRect(&g_TargetRectClient, ptClicked)) {
                break; // Ignore the click entirely if outside the bounds
            }
        }

        bDragging = true;
        ptStart = ptClicked;
        ptEnd = ptStart;
        SetCapture(hWnd); // Lock mouse input to this window
        break;
    }

    case WM_MOUSEMOVE:
        if (bDragging) {
            int currentX = (short)LOWORD(lParam);
            int currentY = (short)HIWORD(lParam);

            // 2. Clamp the dragging coordinates to the target window's boundaries
            if (g_bHasTarget) {
                currentX = std::clamp(currentX, (int)g_TargetRectClient.left, (int)g_TargetRectClient.right);
                currentY = std::clamp(currentY, (int)g_TargetRectClient.top, (int)g_TargetRectClient.bottom);
            }

            ptEnd.x = currentX;
            ptEnd.y = currentY;
            InvalidateRect(hWnd, NULL, FALSE); // Trigger WM_PAINT
        }
        break;

    case WM_LBUTTONUP: {
        if (bDragging) {
            bDragging = false;
            ReleaseCapture();

            int currentX = (short)LOWORD(lParam);
            int currentY = (short)HIWORD(lParam);

            // 3. Clamp the final coordinates just to be safe
            if (g_bHasTarget) {
                currentX = std::clamp(currentX, (int)g_TargetRectClient.left, (int)g_TargetRectClient.right);
                currentY = std::clamp(currentY, (int)g_TargetRectClient.top, (int)g_TargetRectClient.bottom);
            }

            ptEnd.x = currentX;
            ptEnd.y = currentY;

            // Calculate the selected rectangle dimensions
            int left = std::min(ptStart.x, ptEnd.x);
            int top = std::min(ptStart.y, ptEnd.y);
            int width = std::abs(ptEnd.x - ptStart.x);
            int height = std::abs(ptEnd.y - ptStart.y);

            if (width > 0 && height > 0) {
                HDC hdcScreen = GetDC(NULL);
                HDC hdcMemSource = CreateCompatibleDC(hdcScreen);
                HDC hdcMemDest = CreateCompatibleDC(hdcScreen);

                HBITMAP hCropBmp = CreateCompatibleBitmap(hdcScreen, width, height);

                SelectObject(hdcMemSource, hScreenBmp);
                SelectObject(hdcMemDest, hCropBmp);

                BitBlt(hdcMemDest, 0, 0, width, height, hdcMemSource, left, top, SRCCOPY);
                CopyBitmapToClipboard(hWnd, hCropBmp);

                DeleteDC(hdcMemSource);
                DeleteDC(hdcMemDest);
                ReleaseDC(NULL, hdcScreen);
                DeleteObject(hCropBmp);
            }

            PostQuitMessage(0);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HDC hdcBuffer = CreateCompatibleDC(hdc);
        HBITMAP hbmBuffer = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
        HBITMAP hbmOldBuffer = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

        // 1. Draw the frozen screen background
        HDC hdcScreenBmp = CreateCompatibleDC(hdc);
        HBITMAP hbmOldScreen = (HBITMAP)SelectObject(hdcScreenBmp, hScreenBmp);
        BitBlt(hdcBuffer, 0, 0, screenWidth, screenHeight, hdcScreenBmp, 0, 0, SRCCOPY);
        SelectObject(hdcScreenBmp, hbmOldScreen);
        DeleteDC(hdcScreenBmp);

        // 2. (Optional but helpful) Draw a dashed border around the allowed target window area
        if (g_bHasTarget) {
            HPEN hBoundPen = CreatePen(PS_DASH, 2, RGB(0, 255, 255)); // Cyan dashed line
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH));
            HPEN hOldPen = (HPEN)SelectObject(hdcBuffer, hBoundPen);

            Rectangle(hdcBuffer, g_TargetRectClient.left, g_TargetRectClient.top, g_TargetRectClient.right, g_TargetRectClient.bottom);

            SelectObject(hdcBuffer, hOldPen);
            SelectObject(hdcBuffer, hOldBrush);
            DeleteObject(hBoundPen);
        }

        // 3. Draw the red selection rectangle
        if (bDragging) {
            int left = std::min(ptStart.x, ptEnd.x);
            int top = std::min(ptStart.y, ptEnd.y);
            int right = std::max(ptStart.x, ptEnd.x);
            int bottom = std::max(ptStart.y, ptEnd.y);

            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);

            HPEN hOldPen = (HPEN)SelectObject(hdcBuffer, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcBuffer, hBrush);

            Rectangle(hdcBuffer, left, top, right, bottom);

            SelectObject(hdcBuffer, hOldPen);
            SelectObject(hdcBuffer, hOldBrush);
            DeleteObject(hPen);
        }

        // 4. Copy buffer to screen
        BitBlt(hdc, 0, 0, screenWidth, screenHeight, hdcBuffer, 0, 0, SRCCOPY);

        SelectObject(hdcBuffer, hbmOldBuffer);
        DeleteObject(hbmBuffer);
        DeleteDC(hdcBuffer);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// --- The Isolated Snipping Thread ---
DWORD WINAPI SnippingToolThread(LPVOID lpParam)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Extract the target window from the thread parameter
    g_TargetWindow = (HWND)lpParam;
    DPI_AWARENESS_CONTEXT previousDpiContext = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // --- Calculate Target Window Boundaries ---
    if (g_TargetWindow && IsWindow(g_TargetWindow)) {
        RECT screenRect;
        GetWindowRect(g_TargetWindow, &screenRect);

        // Convert the window's screen coordinates to the overlay's client coordinates
        g_TargetRectClient.left = screenRect.left - screenX;
        g_TargetRectClient.top = screenRect.top - screenY;
        g_TargetRectClient.right = screenRect.right - screenX;
        g_TargetRectClient.bottom = screenRect.bottom - screenY;

        g_bHasTarget = true;
    } else {
        g_bHasTarget = false; // Fallback to full screen if no valid window is provided
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    hScreenBmp = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hScreenBmp);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, screenX, screenY, SRCCOPY);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = SnippingWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcex.lpszClassName = "SnippingToolOverlayClass";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "SnippingToolOverlayClass", "Snip",
        WS_POPUP | WS_VISIBLE,
        screenX, screenY, screenWidth, screenHeight,
        NULL, NULL, hInstance, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hScreenBmp)
        DeleteObject(hScreenBmp);
    UnregisterClass("SnippingToolOverlayClass", hInstance);

    SetThreadDpiAwarenessContext(previousDpiContext);

    return 0;
}

// --- Main Application Entry Point ---
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // For demonstration: Let's try to find an open Notepad window to clamp to.
    HWND hTargetApp = FindWindow("Notepad", NULL);

    if (hTargetApp) {
        MessageBox(NULL, "Notepad found!\n\nThe snipping area will be clamped to the Notepad window.", "Main Application", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(NULL, "Notepad not found.\n\nSnipping will default to Full Screen. Open Notepad before running to test the clamping feature.", "Main Application", MB_OK | MB_ICONWARNING);
    }

    // Pass the target window handle (hTargetApp) into the thread parameter
    std::thread{&SnippingToolThread, (LPVOID)hTargetApp}.join();

    MessageBox(NULL, "Snipping complete! Check your clipboard.", "Main Application", MB_OK | MB_ICONINFORMATION);

    return 0;
}*/