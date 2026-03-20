//
// Created by LYS on 3/15/2026.
//

#include <AMboard/CustomNodes/WindowUtil.hxx>
#include <AMboard/Macro/BaseNode.hxx>
#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/Ext/FileDrop.hxx>
#include <AMboard/Macro/Ext/ImGuiPopup.hxx>
#include <AMboard/Macro/Ext/NodeInnerText.hxx>
#include <filesystem>

#include <iostream>
#include <variant>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <spdlog/spdlog.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

class CScreenCapture : public CBaseNode {

public:
    CScreenCapture()
    {
        EmplacePin<CDataPin>(true)->SetValueType("cv::Rect").SetToolTips("Area");
        EmplacePin<CDataPin>(false)->SetValueType("cv::Mat").SetToolTips("Image");
    }

    bool Evaluate() noexcept override
    {
        if (!CBaseNode::Evaluate())
            return false;

        SDpiSetup DS;

        const auto Rect = GetInputPinsWith<EPinType::Data>().front()->As<CDataPin>()->PinGet(cv::Rect);
        if (Rect.area() == 0)
            return true; /// ???

        // 2. Get device contexts and create a bitmap
        const HDC hScreenDC = GetDC(nullptr);
        const HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        const HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, Rect.width, Rect.height);
        const auto hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hBitmap));

        // 3. Copy pixels from the screen to the memory device context
        BitBlt(hMemoryDC, 0, 0, Rect.width, Rect.height, hScreenDC, Rect.x, Rect.y, SRCCOPY);

        // 4. Prepare OpenCV Mat
        // FIX: cv::Mat takes (rows, cols) -> (height, width)!!!
        cv::Mat img(Rect.height, Rect.width, CV_8UC4);

        // 5. Set up Bitmap Info Header
        BITMAPINFOHEADER bi;
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = Rect.width;
        // IMPORTANT: Negative height ensures a top-down DIB (matches OpenCV's layout)
        bi.biHeight = -Rect.height;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;

        // FIX: Unselect the bitmap from the memory DC BEFORE calling GetDIBits
        // (Microsoft docs state the bitmap must not be selected into a DC during GetDIBits)
        SelectObject(hMemoryDC, hOldBitmap);

        // 6. Extract pixels directly into the cv::Mat data buffer
        if (!GetDIBits(hScreenDC, hBitmap, 0, Rect.height, img.data, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS)) {
            spdlog::error("GetDIBits failed to extract pixels.");
        }

        // 7. CLEANUP
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);

        // 8. Convert BGRA to BGR (OpenCV standard)
        cv::cvtColor(img, GetOutputPins()[0]->As<CDataPin>()->Set("cv::Mat", std::make_shared<cv::Mat>()), cv::COLOR_BGRA2BGR);

        return true;
    }

    std::string_view GetCategory() noexcept override
    {
        return "Image";
    }
};

class CWriteImageToClipboard : public CExecuteNode {

public:
    CWriteImageToClipboard()
    {
        EmplacePin<CDataPin>(true)->SetValueType("cv::Mat").SetToolTips("Source");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Image";
    }

protected:
    void Execute() override
    {
        CExecuteNode::Execute();

        auto* InputPin = GetInputPinsWith<EPinType::Data>().front()->As<CDataPin>();
        if (!*InputPin)
            return;

        const auto& Image = InputPin->PinGet(cv::Mat);
        if (Image.empty())
            return;

        // 1. Encode the image into a BMP format in memory
        std::vector<uchar> buffer;
        // imencode creates a full BMP file in memory
        if (!cv::imencode(".bmp", Image, buffer)) {
            spdlog::error("Failed to encode image to BMP.");
            return;
        }

        // 2. A standard BMP file has a 14-byte BITMAPFILEHEADER.
        // The Windows clipboard expects CF_DIB, which is the BMP data EXCLUDING this 14-byte header.
        const int bmpHeaderSize = 14;
        if (buffer.size() <= bmpHeaderSize)
            return;

        size_t dibSize = buffer.size() - bmpHeaderSize;

        // 3. Allocate global memory for the clipboard
        // Note: The clipboard requires memory allocated with GMEM_MOVEABLE
        // Note: The clipboard requires memory allocated with GMEM_MOVEABLE
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dibSize);
        if (!hMem) {
            spdlog::error("Failed to allocate global memory.");
            return;
        }

        // 4. Lock the memory and copy the DIB data into it
        void* ptr = GlobalLock(hMem);
        if (!ptr) {
            GlobalFree(hMem);
            return;
        }
        memcpy(ptr, buffer.data() + bmpHeaderSize, dibSize);
        GlobalUnlock(hMem);

        // 5. Open the clipboard and set the data
        if (!OpenClipboard(NULL)) {
            spdlog::error("Failed to open clipboard.");
            GlobalFree(hMem);
            return;
        }

        EmptyClipboard(); // Clear previous clipboard contents

        // SetClipboardData takes ownership of hMem if successful.
        if (!SetClipboardData(CF_DIB, hMem)) {
            spdlog::error("Failed to set clipboard data.");
            GlobalFree(hMem); // Only free if SetClipboardData fails
        }

        CloseClipboard();
    }
};

class CLoadImage : public CExecuteNode, public INodeImGuiPupUpExt, public IFileDrop {

public:
    CLoadImage()
    {
        EmplacePin<CDataPin>(false)->SetValueType("cv::Mat").SetToolTips("Image");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Image";
    }

    void Execute() override
    {
        if (std::filesystem::exists(m_ImagePath)) {
            GetOutputPinsWith<EPinType::Data>().front()->As<CDataPin>()->Set("cv::Mat", std::make_shared<cv::Mat>(cv::imread(m_ImagePath)));
        }
    }

    std::string GetTitle() override
    {
        return "Image Path";
    }

    bool Render() override
    {
        ImGui::InputText("Path", &m_ImagePath, ImGuiInputTextFlags_ElideLeft);
        return true;
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        ExtContext = m_ImagePath;
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        m_ImagePath = ExtContext;
    }

    void OnFileDrop(const std::vector<std::string>& Files) override
    {
        m_ImagePath = Files.empty() ? m_ImagePath : Files.front();
    }

private:
    std::string m_ImagePath;
};

class CImageTemplateMatch : public CBaseNode {

public:
    CImageTemplateMatch()
    {
        EmplacePin<CDataPin>(true)->SetValueType("cv::Mat").SetToolTips("Source");
        EmplacePin<CDataPin>(true)->SetValueType("cv::Mat").SetToolTips("Template");
        EmplacePin<CDataPin>(true)->SetValueType("bool").SetToolTips("grayscale");
        EmplacePin<CDataPin>(false)->SetValueType("float").SetToolTips("Max match");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Image";
    }

    bool Evaluate() noexcept override
    {
        if (!CBaseNode::Evaluate())
            return false;

        const auto InputPins = GetInputPinsWith<EPinType::Data>()
            | std::views::transform([](const auto& ptr) { return static_cast<CDataPin*>(ptr.get()); })
            | std::ranges::to<std::vector>();

        auto Source = InputPins[0]->PinGet(cv::Mat);
        auto Template = InputPins[1]->PinGet(cv::Mat);

        if (InputPins[2]->PinGetTrivial(bool)) {
            cv::cvtColor(Source, Source, cv::COLOR_BGR2GRAY);
            cv::cvtColor(Template, Template, cv::COLOR_BGR2GRAY);
        }

        cv::Mat Result;
        cv::matchTemplate(Source, Template, Result, cv::TM_CCOEFF_NORMED);

        // 5. Find the best match location
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(Result, &minVal, &maxVal, &minLoc, &maxLoc);

        GetOutputPinsWith<EPinType::Data>().front()->As<CDataPin>()->PinSet(float, maxVal);
        return true;
    }
};

REGISTER_MACROS(CScreenCapture, CLoadImage, CWriteImageToClipboard, CImageTemplateMatch)
ENABLE_IMGUI()