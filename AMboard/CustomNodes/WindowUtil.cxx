//
// Created by LYS on 3/15/2026.
//

#include "WindowUtil.hxx"

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/Ext/ImGuiPopup.hxx>
#include <AMboard/Macro/Ext/NodeInnerText.hxx>

#define NOMINMAX

#include <Psapi.h>
#include <windows.h>

#include <format>
#include <regex>
#include <thread>

#include <imgui.h>
#include <iostream>
#include <misc/cpp/imgui_stdlib.h>
#include <ostream>

#include <opencv2/core/types.hpp>

// A structure to pass our regex pattern and store the result during the enumeration
struct SWindowSearchData {
    bool IsWindowTitle;
    std::regex pattern;
    HWND foundWindow;
};

// The callback function that Windows calls for EVERY open window
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    const auto data = reinterpret_cast<SWindowSearchData*>(lParam);

    // Skip invisible windows (background tasks, hidden system windows)
    if (!IsWindowVisible(hwnd)) {
        return TRUE; // Continue to the next window
    }

    if (data->IsWindowTitle) {
        // Get the length of the window title
        const int length = GetWindowTextLength(hwnd);
        if (length == 0) {
            return TRUE; // Skip windows with no title
        }

        // Retrieve the window title
        std::string windowTitle(length + 1, 0);
        GetWindowText(hwnd, windowTitle.data(), length + 1);

        // Check if the window title matches our Regex pattern
        if (std::regex_search(windowTitle, data->pattern)) {
            data->foundWindow = hwnd; // Save the handle!
            return FALSE; // Stop enumerating (we found it)
        }
    } else {
        DWORD dwProcId = 0;
        GetWindowThreadProcessId(hwnd, &dwProcId);
        if (HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcId); hProc != nullptr) {

            // Retrieve the window path
            std::string executablePath(MAX_PATH, 0);
            if (GetModuleFileNameEx(hProc, nullptr, executablePath.data(), executablePath.size())) {

                // Check if the window path matches our Regex pattern
                if (std::regex_search(executablePath, data->pattern)) {
                    data->foundWindow = hwnd; // Save the handle!
                    CloseHandle(hProc);
                    return FALSE; // Stop enumerating (we found it)
                }
            }

            CloseHandle(hProc);
        }
    }

    return TRUE; // Continue enumerating
}

struct SSnippingConfig {
    HBITMAP hScreenBmp = nullptr;
    POINT ptStart = { 0, 0 };
    POINT ptEnd = { 0, 0 };
    bool bDragging = false;
    int screenX, screenY, screenWidth, screenHeight;

    // --- Window Clamping ---
    RECT TargetRectClient = { 0, 0, 0, 0 };
    bool bHasTarget = false;
};

// Window Procedure for the Snipping Overlay
LRESULT CALLBACK SnippingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SSnippingConfig* pData = nullptr;

    if (message == WM_NCCREATE) {
        // Extract the pointer from CREATESTRUCT
        const auto pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pData = static_cast<SSnippingConfig*>(pCreate->lpCreateParams);
        // Store it for later use
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pData));
    } else {
        // Retrieve the pointer for other messages
        pData = reinterpret_cast<SSnippingConfig*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    switch (message) {
    case WM_LBUTTONDOWN: {
        // Use (short) to properly handle negative coordinates in multi-monitor setups
        POINT ptClicked = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

        // 1. If we have a target window, ensure the user clicked INSIDE it
        if (pData->bHasTarget) {
            if (!PtInRect(&pData->TargetRectClient, ptClicked)) {
                break; // Ignore the click entirely if outside the bounds
            }
        }

        pData->bDragging = true;
        pData->ptStart = ptClicked;
        pData->ptEnd = pData->ptStart;
        SetCapture(hWnd); // Lock mouse input to this window
        break;
    }

    case WM_MOUSEMOVE:
        if (pData->bDragging) {
            int currentX = (short)LOWORD(lParam);
            int currentY = (short)HIWORD(lParam);

            // 2. Clamp the dragging coordinates to the target window's boundaries
            if (pData->bHasTarget) {
                currentX = std::clamp(currentX, (int)pData->TargetRectClient.left, (int)pData->TargetRectClient.right);
                currentY = std::clamp(currentY, (int)pData->TargetRectClient.top, (int)pData->TargetRectClient.bottom);
            }

            pData->ptEnd.x = currentX;
            pData->ptEnd.y = currentY;
            InvalidateRect(hWnd, NULL, FALSE); // Trigger WM_PAINT
        }
        break;

    case WM_LBUTTONUP: {
        if (pData->bDragging) {
            pData->bDragging = false;
            ReleaseCapture();

            int currentX = (short)LOWORD(lParam);
            int currentY = (short)HIWORD(lParam);

            // 3. Clamp the final coordinates just to be safe
            if (pData->bHasTarget) {
                currentX = std::clamp(currentX, (int)pData->TargetRectClient.left, (int)pData->TargetRectClient.right);
                currentY = std::clamp(currentY, (int)pData->TargetRectClient.top, (int)pData->TargetRectClient.bottom);
            }

            pData->ptEnd.x = currentX;
            pData->ptEnd.y = currentY;

            // Calculate the selected rectangle dimensions
            const int left = std::min(pData->ptStart.x, pData->ptEnd.x);
            const int top = std::min(pData->ptStart.y, pData->ptEnd.y);
            const int width = std::abs(pData->ptEnd.x - pData->ptStart.x);
            const int height = std::abs(pData->ptEnd.y - pData->ptStart.y);

            if (width > 0 && height > 0) {
                pData->TargetRectClient.left = left;
                pData->TargetRectClient.top = top;
                pData->TargetRectClient.right = width;
                pData->TargetRectClient.bottom = height;
            } else {
                pData->TargetRectClient.right = pData->TargetRectClient.bottom = 0;
            }

            PostQuitMessage(0);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HDC hdcBuffer = CreateCompatibleDC(hdc);
        HBITMAP hbmBuffer = CreateCompatibleBitmap(hdc, pData->screenWidth, pData->screenHeight);
        HBITMAP hbmOldBuffer = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

        // 1. Draw the frozen screen background
        HDC hdcScreenBmp = CreateCompatibleDC(hdc);
        HBITMAP hbmOldScreen = (HBITMAP)SelectObject(hdcScreenBmp, pData->hScreenBmp);
        BitBlt(hdcBuffer, 0, 0, pData->screenWidth, pData->screenHeight, hdcScreenBmp, 0, 0, SRCCOPY);
        SelectObject(hdcScreenBmp, hbmOldScreen);
        DeleteDC(hdcScreenBmp);

        // 2. (Optional but helpful) Draw a dashed border around the allowed target window area
        if (pData->bHasTarget) {
            HPEN hBoundPen = CreatePen(PS_DASH, 2, RGB(0, 255, 255)); // Cyan dashed line
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH));
            HPEN hOldPen = (HPEN)SelectObject(hdcBuffer, hBoundPen);

            Rectangle(hdcBuffer, pData->TargetRectClient.left, pData->TargetRectClient.top, pData->TargetRectClient.right, pData->TargetRectClient.bottom);

            SelectObject(hdcBuffer, hOldPen);
            SelectObject(hdcBuffer, hOldBrush);
            DeleteObject(hBoundPen);
        }

        // 3. Draw the red selection rectangle
        if (pData->bDragging) {
            const int left = std::min(pData->ptStart.x, pData->ptEnd.x);
            const int top = std::min(pData->ptStart.y, pData->ptEnd.y);
            const int right = std::max(pData->ptStart.x, pData->ptEnd.x);
            const int bottom = std::max(pData->ptStart.y, pData->ptEnd.y);

            const HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            const auto hBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
            const auto hOldPen = static_cast<HPEN>(SelectObject(hdcBuffer, hPen));
            const auto hOldBrush = static_cast<HBRUSH>(SelectObject(hdcBuffer, hBrush));

            Rectangle(hdcBuffer, left, top, right, bottom);

            SelectObject(hdcBuffer, hOldPen);
            SelectObject(hdcBuffer, hOldBrush);
            DeleteObject(hPen);
        }

        // 4. Copy buffer to screen
        BitBlt(hdc, 0, 0, pData->screenWidth, pData->screenHeight, hdcBuffer, 0, 0, SRCCOPY);

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
DWORD WINAPI SnippingToolThread(cv::Rect& BoundingBox)
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Extract the target window from the thread parameter
    SDpiSetup DS;

    SSnippingConfig SnippingConfig;

    SnippingConfig.screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    SnippingConfig.screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    SnippingConfig.screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    SnippingConfig.screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // --- Calculate Target Window Boundaries ---
    if (BoundingBox.width != SnippingConfig.screenWidth || BoundingBox.height != SnippingConfig.screenHeight) {
        // Convert the window's screen coordinates to the overlay's client coordinates
        SnippingConfig.TargetRectClient.left = BoundingBox.x - SnippingConfig.screenX;
        SnippingConfig.TargetRectClient.top = BoundingBox.y - SnippingConfig.screenY;
        SnippingConfig.TargetRectClient.right = SnippingConfig.TargetRectClient.left + BoundingBox.width - SnippingConfig.screenX;
        SnippingConfig.TargetRectClient.bottom = SnippingConfig.TargetRectClient.top + BoundingBox.height - SnippingConfig.screenY;

        SnippingConfig.bHasTarget = true;
    } else {
        SnippingConfig.TargetRectClient = { 0, 0, SnippingConfig.screenWidth, SnippingConfig.screenHeight };
        SnippingConfig.bHasTarget = false; // Fallback to full screen if no valid window is provided
    }

    const HDC hdcScreen = GetDC(nullptr);
    const HDC hdcMem = CreateCompatibleDC(hdcScreen);
    SnippingConfig.hScreenBmp = CreateCompatibleBitmap(hdcScreen, SnippingConfig.screenWidth, SnippingConfig.screenHeight);
    SelectObject(hdcMem, SnippingConfig.hScreenBmp);
    BitBlt(hdcMem, 0, 0, SnippingConfig.screenWidth, SnippingConfig.screenHeight, hdcScreen, SnippingConfig.screenX, SnippingConfig.screenY, SRCCOPY);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

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
        SnippingConfig.screenX, SnippingConfig.screenY, SnippingConfig.screenWidth, SnippingConfig.screenHeight,
        nullptr, nullptr, hInstance, &SnippingConfig);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (SnippingConfig.hScreenBmp)
        DeleteObject(SnippingConfig.hScreenBmp);
    UnregisterClass("SnippingToolOverlayClass", hInstance);

    BoundingBox = { SnippingConfig.TargetRectClient.left, SnippingConfig.TargetRectClient.top, SnippingConfig.TargetRectClient.right, SnippingConfig.TargetRectClient.bottom };

    return 0;
}

struct SWindowPicker {
    bool Render()
    {
        bool changed = ImGui::InputTextWithHint("Filter", "regex", &Filter, ImGuiInputTextFlags_ElideLeft);
        // ImGui::IsItemDeactivatedAfterEdit();

        changed |= ImGui::RadioButton("Windows Title", &FilterType, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Windows Path", &FilterType, 1);

        return changed;
    }

    void WriteExtraContext(std::string& ExtContext) const
    {
        ExtContext += std::format("{}{}", Filter, FilterType);
    }
    void ReadExtraContext(const std::string_view ExtContext)
    {
        if (ExtContext.empty())
            return;

        Filter = ExtContext.substr(0, ExtContext.size() - 1);
        FilterType = ExtContext.back() - '0';
    }

    [[nodiscard]] std::optional<HWND> Search() const
    {
        std::regex RFilter;

        try {
            RFilter.assign(Filter);
        } catch (...) {
            return std::nullopt;
        }

        SWindowSearchData result {
            .IsWindowTitle = FilterType == 0,
            .pattern = std::move(RFilter),
            .foundWindow = nullptr
        };

        // Start looping through all windows
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&result));
        if (result.foundWindow != nullptr)
            return result.foundWindow;
        return std::nullopt;
    }

    std::string Filter;
    int FilterType = 0;
};

class CFindWindow : public CExecuteNode, public INodeImGuiPupUpExt, public INodeInnerText {
public:
    CFindWindow()
    {
        EmplacePin<CDataPin>(false)->SetValueType("HWND").SetToolTips("Window Handle");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Windows";
    }

    std::string GetTitle() override
    {
        return "Filter Config";
    }

    bool Render() override
    {
        Picker.Render();
        return true;
    }

    void Execute() override
    {
        CExecuteNode::Execute();

        if (const auto data = Picker.Search(); data.has_value()) {
            GetOutputPins()[1]->As<CDataPin>()->Set("HWND", *data);
        }
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        Picker.WriteExtraContext(ExtContext);
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        if (ExtContext.empty())
            return;
        Picker.ReadExtraContext(ExtContext);
    }

protected:
    SWindowPicker Picker;
};

class CGetWindowTitle : public CBaseNode {
public:
    CGetWindowTitle()
    {
        EmplacePin<CDataPin>(true)->SetValueType("HWND").SetToolTips("Window Handle");
        EmplacePin<CDataPin>(false)->SetValueType("string");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Util";
    }

    static std::string GetWindowTitle(const HWND Window)
    {
        if (Window == nullptr) {
            return "null";
        }

        // Get the length of the window title
        const int length = GetWindowTextLength(Window);
        if (length == 0) {
            return "";
        }

        // Retrieve the window title
        std::string windowTitle(length + 1, 0);
        GetWindowText(Window, windowTitle.data(), length + 1);

        return windowTitle;
    }

protected:
    bool Evaluate() noexcept override
    {
        if (!CBaseNode::Evaluate())
            return false;
        const auto Window = GetInputPins()[0]->As<CDataPin>()->TryGetTrivial<HWND>("HWND");
        GetOutputPins()[0]->As<CDataPin>()->Set("string", std::move(std::make_shared<std::string>(GetWindowTitle(Window))));
        return true;
    }
};

class CScreenArea : public CBaseNode, public INodeImGuiPupUpExt, public INodeInnerText {
public:
    CScreenArea()
    {
        EmplacePin<CDataPin>(false)->SetValueType("cv::Rect").SetToolTips("Client Rect");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Capture";
    }

    [[nodiscard]] cv::Rect GetFullSize() const
    {
        SDpiSetup DS;

        const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        return { x, y, width, height };
    }

    [[nodiscard]] cv::Rect MaxBoundingArea() const
    {
        if (const auto Window = Picker.Search(); Window.has_value()) {

            RECT WindowSize;
            GetClientRect(*Window, &WindowSize);
            POINT WindowTopLeft = { 0, 0 };
            ClientToScreen(*Window, &WindowTopLeft);
            return { WindowTopLeft.x, WindowTopLeft.y, WindowSize.right, WindowSize.bottom };
        }

        return GetFullSize();
    }

    bool Evaluate() noexcept override
    {
        CBaseNode::Evaluate();

        cv::Rect Result = MaxBoundingArea();
        if (RelArea.area() != 0) {
            Result += RelArea.tl();
            Result.width = RelArea.width;
            Result.height = RelArea.height;
        }

        GetOutputPinsWith<EPinType::Data>().front()->As<CDataPin>()->Set("cv::Rect", std::make_shared<cv::Rect>(Result));

        return true;
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        ExtContext = std::format("{} {} {} {} ", RelArea.x, RelArea.y, RelArea.width, RelArea.height);
        Picker.WriteExtraContext(ExtContext);
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        int charsRead = 0;
        (void)std::sscanf(ExtContext.c_str(), "%d %d %d %d %n", &RelArea.x, &RelArea.y, &RelArea.width, &RelArea.height, &charsRead);
        Picker.ReadExtraContext(std::string_view { ExtContext }.substr(charsRead));
        if (const auto Window = Picker.Search(); Window.has_value()) {
            m_WindowTitle = CGetWindowTitle::GetWindowTitle(*Window);
        }
    }

    std::string GetTitle() override
    {
        return "Filter Config";
    }

    bool Render() override
    {
        if (Picker.Render()) {
            if (const auto Window = Picker.Search(); Window.has_value()) {
                m_WindowTitle = CGetWindowTitle::GetWindowTitle(*Window);
            } else {
                m_WindowTitle.clear();
            }
        }
        ImGui::SeparatorText(m_WindowTitle.c_str());
        if (ImGui::Button("Select Area")) {
            auto Result = MaxBoundingArea();
            RelArea.x = Result.x;
            RelArea.y = Result.y;

            std::thread { &SnippingToolThread, std::ref(Result) }.join();

            RelArea.width = Result.width;
            RelArea.height = Result.height;
            RelArea.x = Result.x - RelArea.x;
            RelArea.y = Result.y - RelArea.y;
        }

        return true;
    }

protected:
    std::string m_WindowTitle;

    SWindowPicker Picker;
    cv::Rect RelArea;
};

REGISTER_MACROS(CFindWindow, CGetWindowTitle, CScreenArea)
ENABLE_IMGUI()